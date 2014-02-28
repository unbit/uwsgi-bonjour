uwsgi-bonjour
=============

uWSGI plugin for OSX bonjour services integration


Installing it
=============

The plugin is 2.0 friendly, just run (after having cloned it)

```sh
uwsgi --build-plugin uwsgi-bonjour
```

Using it
========

The plugin currently exposes a single option `bonjour-register` alowing you to register new record
in the bonjour dns service. You can register CNAME and A records.

Dinamically registering entries allows you to easily implement virtualhosting without messing with /etc/hosts.

Supposing you are working on the `darthvaderisbetterthanyoda` website, you may want to reach it via the `darthvaderisbetterthanyoda.local` domain without changing the name of your system:

```ini
[uwsgi]
plugins = bonjour
bonjour-register = name=darthvaderisbetterthanyoda.local,a=127.0.0.1
plugins = psgi
psgi = myapp.pl
http-socket = :8080
```

you can now directly connect to http://darthvaderisbetterthanyoda.local:8080/

The `bojour-register` option can take the following parameters:

`name` (required) the name you want to register

`cname` create a CNAME record mapping to the specified hostname

`a` (alias `ip`) create an A record mapping to the specified IPv4 address

`unique` if set register the record as a unique bonjour entry (default is shared)
