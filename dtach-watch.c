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

void activity (int fd, short events, void *data) {
	struct event_base *event = (struct event_base*) data;

	puts("DATA!!!!!!");
	event_base_loopexit(event, NULL);
}

int main (int argc, char** argv) {
	int fd;
	struct sockaddr_un saddr;
	struct event_base *event;
	struct timeval timeout;
	struct fake_message fm;

	if (argc != 2) {
		fputs("Must give path to socket as only argument", stderr);
		return(EXIT_FAILURE);
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fputs("Call to socket failed.", stderr);
		return(EXIT_FAILURE);
	}

	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, argv[1]);

	if (connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) < 0) {
		close(fd);
		fputs("Connect failed", stderr);
		return(EXIT_FAILURE);
	}

	event = event_base_new();

	fm.type = 1;
	fm.len= 0;

	/* This should write an empty packet asking to attach */
	write(fd, (char*)&fm, sizeof(struct fake_message));

	/* Do events here... */
	/* Timeout in 10 seconds */
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	event_base_once(event, fd, EV_READ, &activity, (void*)event, &timeout);
	event_base_dispatch(event);

	/* Inform that we're detaching */
	fm.type = 2;
	write(fd, (char*)&fm, sizeof(struct fake_message));

	event_base_free(event);

	return EXIT_SUCCESS;
}
