#include <uwsgi.h>
#include <dns_sd.h>

extern struct uwsgi_server uwsgi;

static struct uwsgi_bonjour {
	struct uwsgi_string_list *record;
} ubonjour;

static DNSServiceRef bonjour_sdref = NULL;

static struct uwsgi_option bonjour_options[] = {
	{"bonjour-register", required_argument, 0, "register a record in the Bonjour service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &ubonjour.record, 0},
	{"bonjour-register-record", required_argument, 0, "register a record in the Bonjour service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &ubonjour.record, 0},
	{"bonjour-rr", required_argument, 0, "register a record in the Bonjour service (default: register a CNAME for the local hostname)", uwsgi_opt_add_string_list, &ubonjour.record, 0},
	UWSGI_END_OF_OPTIONS
};

// fake function
static void dnsCallback(DNSServiceRef sdRef __attribute__((unused)),
            DNSRecordRef RecordRef __attribute__((unused)),
            DNSServiceFlags flags __attribute__((unused)),
            DNSServiceErrorType errorCode __attribute__((unused)),
            void *context __attribute__((unused))) {
}

// convert a c string to a dns string (each chunk is prefix with its size in uint8_t) + \0 suffix
static struct uwsgi_buffer *to_dns(char *name, size_t len) {
	uint8_t chunk_len = 0;
	size_t i;
	struct uwsgi_buffer *ub = uwsgi_buffer_new(len);
	if (!ub) goto error;
	char *ptr = name;

	for(i=0;i<len;i++) {
		if (name[i] == '.') {
			if (uwsgi_buffer_u8(ub, chunk_len)) goto error;
			if (uwsgi_buffer_append(ub, ptr,chunk_len)) goto error;
			ptr = name + i + 1;
			chunk_len = 0;
		}
		else {
			chunk_len++;	
		}
	}

	if (chunk_len > 0) {
		if (uwsgi_buffer_u8(ub, chunk_len)) goto error;
                if (uwsgi_buffer_append(ub, ptr,chunk_len)) goto error;
	}

        if (uwsgi_buffer_append(ub, "\0", 1)) goto error;

	return ub;
error:
	uwsgi_log("[uwsgi-bonjour] unable to generate dns name for %s\n", name);
	exit(1);
}

static void register_cname(char *name, char *cname, int unique) {

	DNSServiceFlags f = unique ? kDNSServiceFlagsUnique : kDNSServiceFlagsShared;

	DNSRecordRef r = 0;
	struct uwsgi_buffer *ub = to_dns(cname, strlen(cname));
	DNSServiceErrorType error = DNSServiceRegisterRecord(bonjour_sdref, &r, f, 0, name, kDNSServiceType_CNAME, kDNSServiceClass_IN,
		ub->pos, ub->buf, 60,
                dnsCallback, NULL);

	if (error) {
		uwsgi_log("[uwsgi-bonjour] unable to register CNAME for %s, error code: %d\n", name, error);
		exit(1);
	}

	uwsgi_buffer_destroy(ub);
	uwsgi_log("[uwsgi-bonjour] registered record %s CNAME %s\n", name, cname);
}

static void register_a(char *name, char *addr, int unique) {

        DNSServiceFlags f = unique ? kDNSServiceFlagsUnique : kDNSServiceFlagsShared;

        DNSRecordRef r = 0;
	uint32_t ip = inet_addr(addr);
        DNSServiceErrorType error = DNSServiceRegisterRecord(bonjour_sdref, &r, f, 0, name, kDNSServiceType_A, kDNSServiceClass_IN,
                4, &ip, 60,
                dnsCallback, NULL);

        if (error) {
                uwsgi_log("[uwsgi-bonjour] unable to register A for %s, error code: %d\n", name, error);
                exit(1);
        }

        uwsgi_log("[uwsgi-bonjour] registered record %s A %s\n", name, addr);
}

static void *bonjour_loop(void *arg) {
	for(;;) {
		DNSServiceErrorType error = DNSServiceProcessResult(bonjour_sdref);
		if (error) {
			uwsgi_log("[uwsgi-bonjour] error %d while processing result\n", error);
		}
	}
	return NULL;
}

static void bonjour_init() {

	if (!ubonjour.record) return;

	DNSServiceErrorType error = DNSServiceCreateConnection(&bonjour_sdref);
        if (error) {
                uwsgi_log("[uwsgi-bonjour] unable to initialize DNS resolution, error code: %d\n", error);
                exit(1);
        }

	char *myself = uwsgi.hostname;
	if (!uwsgi_endswith(myself, ".local") && !uwsgi_endswith(myself, ".lan")) {
		myself = uwsgi_concat2(uwsgi.hostname, ".local");
	}

	struct uwsgi_string_list *usl = NULL;
	uwsgi_foreach(usl, ubonjour.record) {
		char *b_name = NULL;
		char *b_cname = NULL;
		char *b_unique = NULL;
		char *b_ip = NULL;
		if (strchr(usl->value, '=')) {
			if (uwsgi_kvlist_parse(usl->value, usl->len, ',', '=',
			"name", &b_name,
			"cname", &b_cname,
			"unique", &b_unique,
			"ip", &b_ip,
			"a", &b_ip,
			NULL)) {
				uwsgi_log("[uwsgi-bonjour] invalid keyval syntax\n");
				exit(1);
			}

			if (!b_name) {
				uwsgi_log("[uwsgi-bonjour] you need to specift the name key to register a record\n");
				exit(1);
			}
			
			if (b_cname) {
				register_cname(b_name, b_cname, b_unique ? 1 : 0);	
			}
			else if (b_ip) {
				register_a(b_name, b_ip, b_unique ? 1 : 0);	
			}
			else {
				register_cname(b_name, myself, b_unique ? 1 : 0);
			}

			if (b_name) free(b_name);
			if (b_cname) free(b_cname);
			if (b_unique) free(b_unique);
			if (b_ip) free(b_ip);
		}
		else {
			register_cname(usl->value, myself, 0);
		}
	}

	if (myself != uwsgi.hostname)
		free(myself);

	// now start a pthread for managing bonjour
	pthread_t t;
        pthread_create(&t, NULL, bonjour_loop, NULL);
}

static void bonjour_close_fd() {
	if (bonjour_sdref) {
		close(DNSServiceRefSockFD(bonjour_sdref));
	}
}

struct uwsgi_plugin bonjour_plugin = {
	.name = "bonjour",
	.options = bonjour_options,
	.post_fork = bonjour_close_fd,
	// we use .post_init instead of .init to avoid the mdsn socket to be closed on reloads
	.post_init = bonjour_init,
};
