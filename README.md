# gen_network_service
This repository contains a free (BSD licensed) service written in "C" to handle the network operation of a service process.

The practical usage of this is in concert with an Apache webserver.   This service only handles HTTP POST operations.  The
configuration of the apache webserver would reverse proxy the POST operations to this service, rather than handle them by
itself.   This is useful for a couple reasons, first it allows the application code to link directly into the service.  This
keeps regular CGI scripts out of the picture.   

However the most important reason to use a service like this to handle CGI post operations is that the application code,
running outside of Apache context allows the service to run with privileges not possible under apache.  Normally the websites
are sandboxed in a way to not allow any filesystem visibility outside of the website.

In some cases this is helpful for IOT devices that have to use GPIO pins or file I/O.
