uwsgi-bonjour
=============

uWSGI plugin for OSX bonjour services integration


Installing it
=============

The plugin is 2.0 friendly, just run *(after having cloned it)*

```sh
uwsgi --build-plugin uwsgi-bonjour
```

Using it
========

The plugin currently exposes a single option `bonjour-register` allowing you to register new records
in the bonjour DNS service. You can register CNAME and A records.

Dynamically registering entries allows you to easily implement virtualhosting without messing with `/etc/hosts`. *(and more important the other people in your LAN will see them)*

Supposing you are working on the `darthvaderisbetterthanyoda` website, you may want to reach it via the `darthvaderisbetterthanyoda.local` domain without changing the name of your system:

```ini
[uwsgi]
plugins = bonjour
bonjour-register = name=darthvaderisbetterthanyoda.local,a=127.0.0.1
plugins = psgi
psgi = myapp.pl
master = true
http-socket = :8080
```

You can now directly connect to http://darthvaderisbetterthanyoda.local:8080/

The `bonjour-register` option can take the following parameters:

- `name` (required) the name you want to register

- `cname` create a CNAME record mapping to the specified hostname

- `a` (alias `ip`) create an A record mapping to the specified IPv4 address

- `unique` if set register the record as a unique bonjour entry (default is shared)

CNAME shortcut
==============

If you specify only a hostname as the `bonjour-register` argument, it will be registered as a CNAME for the local hostname.
If your hostname is called `deathstar.local` the following config:

```ini
[uwsgi]
plugins = bonjour
bonjour-register = darthvaderisbetterthanyoda.local
bonjour-register = hansolo.local
plugins = psgi
master = true
psgi = myapp.pl
http-socket = :8080
```

will create 2 CNAME records for darthvaderisbetterthanyoda.local and hansolo.local pointing to deathstar.local.

In same (rare) cases your local hostname could be messy and bonjour is not able to correctly use it. In such a case rely to non-shortcut setup, specifying the CNAME (or A) explicitly to a resolvable address.
