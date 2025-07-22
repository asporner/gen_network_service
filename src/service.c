/* *************************** FILE HEADER ***********************************
 *
 *  \file		service.c
 *
 *  \brief		<b>Network service implementation.</b>\n
 *
 *				This program is used to accept network socket connections
 *   			and then round the request to an application that is linked
 *    			to this program.
 *
 *  \author		Andrew Sporner\n
 *
 *  \version	1	2025-07-20 Andrew Sporner    created
 *
 * *************************** FILE HEADER **********************************/

#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>

	/* ----------------------------------------------- 	*/
	/*  Tunable values									*/
	/*													*/
	/*  LISTEN_ADDR										*/
	/*  LISTEN_PORT										*/
	/*													*/
	/*  Here is where you can change the listener 		*/
	/*  configuration by setting the LISTEN_ADDR and 	*/
	/*  LISTEN_PORT as needed to specify the address	*/
	/*  and listener port, respectively.  We do 		*/
	/*  recommend keeping the address set to 127.0.0.1	*/
	/*  as this service does not support SSL or the		*/
	/*  server is appropriately set in the network 		*/
	/*  behind some kind of firewall.					*/
	/*													*/
	/*  N_WORKERS										*/
	/*													*/
	/*  Here is where you set the number of prespawned 	*/
	/*  instances of the worker processes.				*/
	/*													*/
	/*	MAX_REQUEST										*/
	/*													*/
	/*  This is where you can set up the maximum post	*/
	/*  size (content_length).  If the submitted 		*/
	/*  content_length is larger than this value, the	*/
	/*  value 500 is returned with a message that the	*/
	/*  request was too large.							*/
	/*													*/
	/*  STATUS_FMT										*/
	/*													*/
	/*	THis is the format string used to pass back		*/
	/*  the status of the transaction.					*/
	/*													*/
	/* ----------------------------------------------- 	*/

#define LISTEN_ADDR								"127.0.0.1"
#define LISTEN_PORT								4444
#define N_WORKERS								16
#define MAX_REQUEST								 2 * 1024 * 1024  /* 16M */
#define POST_REPLY_SIZE                          2 * 1024 * 1024  /* 2M */

#define ERR_FMT  "HTTP/1.1 %u / POST: %s\r\n    \
        Access-Control-Allow-Origin: *\r\nContent-type: text/plain\r\n\r\n"


	/* ----------------------------------------------- 	*/
	/*													*/
	/*  This program depends (or the link will fail)	*/
	/*  on three external functions.					*/
	/*													*/
	/*  app_init() is the function that will initalize 	*/
	/*  the one-time initialization functions.			*/
	/*													*/
	/*  app_poll() is called at least every second to   */
	/*  allow any period processing to occur.			*/
	/*													*/
	/*  app_main() is called with the socket, buffers	*/
	/*  and connected client address.  					*/
	/* ----------------------------------------------- 	*/

int app_init(void);

int app_poll(void);

int app_main(int c, char *in, char *out, char *h_name);

/******************************************************************************
 *  \fn		socket_init
 ******************************************************************************
 *  This function will set up various options on the socket descriptor for
 *  the connected client.   The first two establish sizes for the sending
 *  and receiving buffers associated with the socket.  This is to have the
 *  best performance for the socket I/O.   The last is to disable the NAGLE
 *  algorithm.   The NAGLE algoritm was developed to assure that interactive
 *  users (former Telnet users, but now SSH probably fits) had a way of 
 *  assuring that the network wasn't being flooded with packets of data with
 *  only a few characters in it, but yet also not waiting so long to fill the
 *  input buffers so as to create lag.   Since this isn't an interactive
 *  service (like interactive I/O), this only gets in the way and thus it is
 *  deactivated...
 *
 *****************************************************************************/
static int socket_init(
																int fd
																)
{
	int i;

    i = 102400;
 	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) == -1) {
		return -1;
	}

    i = 102400;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &i, sizeof(i)) == -1) {
		return -1;
	}

	i = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i)) == -1) {
		return -1;
	}

	return 0;
}

/******************************************************************************
 *  \fn		sock_gets
 ******************************************************************************
 *  This function is modeled after the gets, fgets family of library calls
 *  except this works on a non-buffred I/O descriptor (the socket) instead
 *  of a buffered I/O file descriptor.   We ignore \m (carriage return) 
 *  characters and complete when a line-feed is recieved.
 *
 *****************************************************************************/
int sock_gets(
														int s,
														char *buff
														)
{
	int len;

	for (len=0;;len++) {
		if (read(s, buff, 1) < 1) {
			return -1;
		}

		if (*buff == 13) {
			continue;
		}

		if (*buff == 10) {
			*buff = (char)0;
			break;
		}

		buff++;
	}

	return len;
}

/******************************************************************************
 *  \fn		read_header_variables
 ******************************************************************************
 *  This function is responsible for reading the HTTP header variables
 *  from the HTTP header.   We are interesting in two things, the method
 *  (POST, GET, OPTIONS) and the content length.  We are only going to 
 *  handle the POST method.  Everything is is going to return a 500 error.
 *
 *****************************************************************************/
int read_header_variables(
														int s,
														int *content_length
														)
{
	char header_buff[1000], *dptr;


	/* Check to make sure we have a point */
	if (content_length == NULL) {
		return -1;
	}

	/* Always start with a cleared header buffer */
	memset(header_buff, 0, sizeof(header_buff));

	/* Read the first line, which is going to tell us the method */
	sock_gets(s, header_buff);
	
	if ((dptr = strchr(header_buff, ' ')) != NULL) {
		*dptr = (char)0;
	}

	/* We only support POST, if anything else, return with an error */

	if (strcmp(header_buff, "POST") != 0) {
		return -1;
	}

	/* Loop through the headers, only interested in the length */
	for (;;) {

		/* Always start with a cleared header buffer */
		memset(header_buff, 0, sizeof(header_buff));

		/* Read the next line */
		sock_gets(s, header_buff);

		/* An empty string terminates the header block of the HTTP request.  */

		if (!strlen(header_buff)) {
			break;
		}

		/* We are only interested in the content length,
		 * throw away everything else.
		 */

		if (!strncmp(header_buff, "Content-Length", 14)) {

			/* Separate the header value from its name */

			if ((dptr = strchr(header_buff, ':')) != NULL) {
				*dptr++ = (char)0;
				while (*dptr == ' ') dptr++;
			}

			*content_length = atoi(dptr);
		}
	}

	if (!*content_length || (*content_length > MAX_REQUEST)) {
		return -1;
	}

	return 0;
}

/******************************************************************************
 *  \fn		accept_main
 ******************************************************************************
 *  This function is responsible for accepting and processing a client 
 *  connection.   The first step is to acquire the connection from the
 *  listener socket and then get the HTTP headers. We then allocate an 
 *  appropriately sized buffer and then call the application handler function 
 *  to process the data and free the buffer used for input.  Then the client 
 *  socket is closed and the function waits for yet another client connection.
 *
 *  Since transactions can run long enough to not keep up with the rate of
 *  incoming connections, multiple processes are spawned to scale so that 
 *  there is no waiting for a process to accept and handle the connection.
 *  This needs to be sized appropriately.
 *
 *****************************************************************************/
int accept_main(
														int fdh
														)
{

	for (;;) {
		struct sockaddr_in cli;
		char h_name[60], mesg[200], *in=NULL, *out=NULL;
		int content_length=0, i, s, rv = -1;

		memset(mesg, 0, sizeof(mesg));

		if ((s = accept(fdh, (struct sockaddr *)&cli, (socklen_t *)&i)) == -1) {
			syslog(LOG_ERR, "Failed to accept a new connection!\n");
			sleep(1);
			continue;
		}

		/* Set the socket options */
		socket_init(s);

		/* Obtain the name of the connected client */
		memset(h_name, 0, sizeof(h_name));
		strcpy(h_name, inet_ntoa(cli.sin_addr));

		if (read_header_variables(s, &content_length) == 0) {
			/* Now allocate a buffer, read the data and process */

			if ((in = malloc(content_length)) != NULL) {
				if ((out = malloc(POST_REPLY_SIZE)) != NULL) {
					/* Read the request buffer */
					memset(in, 0, content_length);
					rv = read(s, in, content_length);

					if ((rv != -1) && (content_length = rv)) {
						if ((rv = app_main(s, in, out, h_name)) != 0) {
							sprintf(mesg, "Call to application failed.");
						}

					} else {
						sprintf(mesg, "Couldn't read entire buffer");
					}

				} else {
					sprintf(mesg, "Cannot allocate output buffer");
				}
			} else {
				sprintf(mesg, "Cannot allocate input buffer");
			}

		} else {
			sprintf(mesg, "Invalid or missing content headers");
		}

		if (in != NULL) {
			free(in);
		}

		if (out != NULL) {
			free(out);
		}

		write(s, out, strlen(out));

		/* shut down the socket */
		shutdown(s, 2);

		/* close the socket */
		close(s);
	}

	// NOT reached!
}

/******************************************************************************
 *  \fn		init_listener
 ******************************************************************************
 *  This function will open a socket to be used as a listener for client 
 *  connections.  
 *
 *  The socket is first opened and then a few options are set.  One of them
 *  is to set the REUSE attribute.   Normally when a listener program exits,
 *  the socket address isn't immediately available to be used again, so this 
 *  option assure that resources are freed immediately on program exit.
 *  The second option is the amount of time that a closed connection remains
 *  allocated to the connection.  In an orderly close it could take a few
 *  seconds, but in other circumstances (like a denial of service attack where
 *  the connections are opened, but never actually used. this could take up to 
 *  several minutes.   On a busy server, either of these cases could be 
 *  problematic.   So by setting SO_LONGER to not linger, we have a 
 *  deterministic behavior as it relates to sockets that are no longer (or 
 *  never were used).
 *  
 *  The next thing to happen is that we bind the socket to a listener 
 *  address.  This is made up of an IP address on the server and a port.
 *  In our case, since we have Apache front-ending user traffic, we keep
 *  this service bound to the loopback interface, so that no external 
 *  connections from outside the server may reach this service.  The port
 *  is one of the tunable values.
 * 
 *  The last thing to happen is to then actively start listening on the 
 *  port for new connections.  We also set the accept queue to 500 nascent
 *  connections. Once the connection is accepted, it leaves this queue. So
 *  there must be enough places for connections that have not yet been 
 *  processed, otherwise when there is an overflow, the client recieves
 *  a connection reset and the connection fails.   
 *
 *  This is related to the number of handler processes that do the accepting
 *  as described above.  Ideally the number of handler processes should be 
 *  no more than twice the number of logical CPU's on the server since
 *  a single instance will not generally always been in a RUNable state.
 *
 *****************************************************************************/
static int init_listener(
														unsigned short s_port
														)
{
	struct sockaddr_in sin;
	struct linger lin = { 0 };
	int s, i, rc;

	/* Open the socket */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(LOG_ERR, "ERROR opening socket [%s]", strerror(errno));
		return -1;
	}

	/* Set Re-use address */
	i = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &i, sizeof(i));

    if (rc == -1) {
		syslog(LOG_ERR, "ERROR setsockopt SO_REUSEADDR");
		return -1;
    }

	/* free the socket immediately upon close */

	lin.l_onoff = 0;
	lin.l_linger = 0;
	rc = setsockopt(s, SOL_SOCKET, SO_LINGER, (const char *) &lin, sizeof(lin));

    if (rc == -1) {
		syslog(LOG_ERR, "ERROR setsockopt SO_LINGER");
		return -1;
    }

	/* Bind the socket to an endpoint */

	bzero((char *) &sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(LISTEN_ADDR);
	sin.sin_port = htons(s_port);

	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		syslog(LOG_ERR, "ERROR on binding");
		return -1;
	}

	/* Put the socket in listen state */

	listen(s, 500);

	return s;
}

/******************************************************************************
 *  \fn		check_restart_workers
 ******************************************************************************
 *  We have a pool of pre-spawned workers to handle the processing of 
 *  inbound client connections.   For various reasons one of these processes
 *  can die or otherwise exit (generally in the application handler) and so
 *  there is a need to detect this and relaunch the worker so that the 
 *  number of processes in the pool remain constant.
 *
 *****************************************************************************/
int check_restart_workers(
														int fdh,
														pid_t *worker_pids
														)
{
	pid_t pid;
	int i;

	for (i=0; i < N_WORKERS; i++) {

		/* we are not really going to kill the worker, just go through the
         * motions of this to see if the process even exists.  
		 */
		if (kill(worker_pids[i], 0) == -1) {

			/* OK it does not exist (it exited on its own), so we need to
 			 * relaunch it 
			 */
			if (errno == ESRCH) {

				/* Same handling as inital lauch, but this code is too small
				 * to warrant a common function. It's used only twice anywasys.
 				 */
				if ((pid = fork()) == 0) {
					accept_main(fdh);
					exit (1);
				}

				syslog(LOG_INFO, "RESTART Worker %u PID=%u\n", 
					i, (unsigned int)pid);

				/* Keep track of the newly launch worker process.  No 
				 * access syncronization is necessary here since any 
				 * use of this is at the initialization (where it is single
				 * threaded and in this particular case where it is also
				 * single-threaded.
				 */
				worker_pids[i] = pid;
			}
		}
	}

	return 0;
}


/****************************************************************************
 *  \fn		main	
 ******************************************************************************
 *  This is the main function that is called when the service program is
 *  started.   We need to make sure that this service is started as root
 *  because of the system interfaces that are being used.  If we are not root
 *  then we fail with an exit code of 1.
 *
 *  Next we need to establish the service as an independant process that
 *  has it's default directory as the root directory (/) and have no 
 *  standard input, output or error.
 *
 *  We then open the system log and then call the function in the application
 *  that is responsible for initializing that application (or return, doing
 *  nothing).
 *
 *  We then create the worker pool by launching the configured number
 *  of listener processes.
 *
 *  Lastly, we don't really exit, but rather we enter a monitor loop
 *  whereby we inspect the worker pool to make sure we didn't lose a
 *  process (and if so restart it).  Then we call into the application
 *  to do whatever periodic things that need to be done.  Care must be
 *  taken that this application call does not block or otherwise take
 *  so much time as to not be able to check for lost worker processes
 *  or any activity done at the start of the application callout.
 *  
 *  
 ***************************************************************************/
int main(
														int argc, 
														char **argv
														)
{
	int fdh;
	pid_t worker_pids[N_WORKERS];

#ifdef NOTDEF
	/* Make sure we are effectively root */

	if (setuid(0) == -1 ) {		
		exit(1);
	}

	/* become a daemon process */

	if (daemon(0,0) == -1) {
		exit(1);
	}
#endif


	/* open the system log facility */

	openlog("appservd", (LOG_CONS | LOG_PID), LOG_USER);

	/* Call the application init hook */

	if (app_init()  == -1) {
		exit(1);
	}

	/* set up the listener socket */
	if ((fdh = init_listener(LISTEN_PORT)) == -1) {
		syslog(LOG_ERR, "Failed to initialize http listener!\n");
		exit (1);
	}

	/* Establish the worker pool */

	for (int i=0; i < N_WORKERS; i++) {
		if ((worker_pids[i] = fork()) == 0) {
			accept_main(fdh);
			exit(1);  /* should never be reached and if so -- error! */
		}
	
		syslog(LOG_INFO, "REST Worker PID=%u\n", (unsigned int)worker_pids[i]);
	}

	/* run the monitor loop */

	for (;;) {
		check_restart_workers(fdh, worker_pids);
		app_poll();
		sleep(1);
	}

	// NOT REACHED

	closelog();

	return 0;
}

