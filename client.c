#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BUFFLEN 2048

static void printHelp(const char *name) {
	fprintf(stdout,"Usage: %s <remote_ip> <remote_port>\n\n",
									name);
}

static volatile sig_atomic_t run = 1;

int main(int argc, char **argv) {
    int port, sd, maxfd, ret;
    struct sockaddr_in sas;
    fd_set rfds, rfd;
    char buffer[BUFFLEN];

    if (argc != 3) {
        printHelp(argv[0]);
        exit(-1);
    }

    if (65535 < (port = atoi(argv[2]))) {
        fprintf(stderr, "Invalid port number\n");
        exit(-1);
    }

    if (0 == inet_aton(argv[1], &sas.sin_addr)) {
        fprintf(stderr, "Invalid IP-Address: %s\n", argv[1]);
        exit(-1);
    }

    sas.sin_family = AF_INET;
    sas.sin_port = htons(port);

    if (0 > (sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
        fprintf(stderr, "socket() failed\n");
        exit(-1);
    }

    if (0 > connect(sd, (const struct sockaddr *)&sas, sizeof(sas))) {
        fprintf(stderr, "connect() failed\n");
        exit(-1);
    }

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(sd, &rfds);
    maxfd = MAX(sd, STDIN_FILENO);
    while(run) {
        memset(buffer, 0, sizeof(buffer));
        rfd = rfds;

        if (0 > select(maxfd + 1, &rfd, NULL, NULL, NULL)) {
            fprintf(stderr, "select() failed\n");
            exit(-1);
        }
        
        if (FD_ISSET(sd, &rfd)) {
			ret = recv(sd, buffer, sizeof(buffer)-1, 0);

			if (0 > ret) {
				if (errno == EINTR) {
					continue;
				}
				fprintf(stderr, "recv() failed\n");
				exit(1);
			}

			if (0 == ret) {
				fprintf(stdout, "Server closed connection\n");
				run = 0;
				continue;
			}
			fprintf(stdout, "%s", buffer);
		}

        if (FD_ISSET(STDIN_FILENO, &rfd)) {
            if (NULL == fgets(buffer, sizeof(buffer) - 1, stdin)) {
                fprintf(stderr, "fgets() failed\n");
                continue;
            }

            if (0 > send(sd, buffer, strlen(buffer), 0)) {
                fprintf(stderr, "send() failed\n");
                continue;
            }
        }
    }

    close(sd);

    return 0;
}