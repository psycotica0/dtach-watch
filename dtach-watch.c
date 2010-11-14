#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pty.h>

#include <event.h>

/* This structure was replicated from the dtach code. It's what actually gets sent to request we attach. */
struct fake_message {
	unsigned char type;
	unsigned char len;
	struct winsize ws;
};

void activity(int fd, short events, void* data) {
	struct event_base* event = (struct event_base*) data;

	event_base_loopexit(event, NULL);
}

struct silent_data {
	struct event_base* eb;
	struct event* ev;
	struct timeval* tv;
};

void silent(int fd, short events, void* data) {
	struct silent_data* sd = (struct silent_data*) data;

	if (events & EV_TIMEOUT) {
		event_base_loopexit(sd->eb, NULL);
	} else {
		/* Consume input */
		char buf[sizeof(struct fake_message)];
		read(fd, buf, sizeof(struct fake_message));
		/* Add the event again */
		event_add(sd->ev, sd->tv);
	}
}

int main (int argc, char** argv) {
	int fd;
	struct sockaddr_un saddr;
	struct event_base* event;
	struct timeval timeout;
	struct fake_message fm;
	struct silent_data sd;
	struct event ev;
	/* This holds the filename of the socket to watch */
	char* filename = NULL;
	/* This does two things. First, it temporarily holds the timeout to determine a terminal is silent. */
	/* Second, if it is anything by NULL we use silence mode, rather than activity mode */
	char* silence = NULL;
	int i;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if (argc == 1) {
		fputs("Must give path to socket as argument\n", stderr);
		return(EXIT_FAILURE);
	}

	for (i=1; i < argc; i++) {
		if (strncmp(argv[i], "-s", 2) == 0) {
			if (strlen(argv[i]) == 2) {
				/* This flag is only -s, next argument will be the timeout */
				silence = argv[++i];
			} else {
				/* This flag contains the timeout jammed against it (-s16) */
				silence = argv[i] + 2;
			}
		} else if (strncmp(argv[i], "-h", 2) == 0) {
			/* Print usage */
			fputs("dtach-watch: This command watches a dtach socket for activity or inactivity.\n", stderr);
			fputs("             The default mode watches the given socket and returns with an exit\n", stderr);
			fputs("             code of 0 when activity is detected on the socket. If the -s option\n", stderr);
			fputs("             is given with a time, then instead we use silence mode. In this\n", stderr);
			fputs("             mode the the socket is watched and dtach-watch will exit with an\n", stderr);
			fputs("             exit code of 0 when there are the requested number of seconds\n", stderr);
			fputs("             without activity.\n", stderr);
			fputs("Usage: dtach-watch [-s TIME] /path/to/socket\n", stderr);
			fputs("    /path/to/socket  This is the path to the dtach socket to watch\n", stderr);
			fputs("    -s TIME          Use 'silence' mode with a timeout of TIME seconds\n", stderr);
			return(EXIT_FAILURE);
		} else {
			/* Filename */
			filename = argv[i];
		}
	}

	if (filename == NULL) {
		fputs("A path to the dtach socket must be given.\n", stderr);
		return(EXIT_FAILURE);
	}

	if (silence != NULL) {
		char *endptr;
		timeout.tv_sec = strtol(silence, &endptr, 10);
		if (silence == endptr) {
			fprintf(stderr, "Time '%s' not a valid integer\n", silence);
			exit(EXIT_FAILURE);
		} else if (timeout.tv_sec <= 0) {
			fputs("Time must be greater than 0.\n", stderr);
			exit(EXIT_FAILURE);
		}
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fputs("Call to socket failed.\n", stderr);
		return(EXIT_FAILURE);
	}

	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, filename);

	if (connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) < 0) {
		close(fd);
		fputs("Connect failed.\n", stderr);
		return(EXIT_FAILURE);
	}

	event = event_base_new();

	fm.type = 1;
	fm.len= 0;

	/* This should write an empty packet asking to attach */
	write(fd, (char*)&fm, sizeof(struct fake_message));

	if (silence) {
		sd.eb = event;
		sd.ev = &ev;
		sd.tv = &timeout;

		event_set(&ev, fd, EV_READ, &silent, (void*)&sd);
		event_base_set(event, &ev);
		event_add(&ev, &timeout);
	} else {
		event_base_once(event, fd, EV_READ, &activity, (void*)event, NULL);
	}

	event_base_dispatch(event);

	/* Inform that we're detaching */
	fm.type = 2;
	write(fd, (char*)&fm, sizeof(struct fake_message));

	event_base_free(event);

	return EXIT_SUCCESS;
}
