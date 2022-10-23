/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "list.h"

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define MAXCLIENTS 16
#define BUFFLEN 2048

struct client {
    struct list_head list;
    struct sockaddr_in sa;
    int sd;
};

LIST_HEAD(cl);

static void cl_remove(struct client *c, fd_set *fds) {
	FD_CLR(c->sd, fds);
	list_del(&c->list);
	free(c);
}

static void printUsage(const char* name) {
    fprintf(stdout, "Usage: %s <port>\n\n", name);
}

int main(int argc, char **argv) {
    int sd, maxfd, csd, ret, fs;
    struct sockaddr_in sal, sac;
    uint16_t port;
    fd_set rfds, rfd;
    socklen_t slen;
    struct client* c;
    struct client* tmp;
    char buffer[BUFFLEN];

    if (argc != 2) {
        printUsage(argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);

    sal.sin_family = AF_INET;
    sal.sin_port = htons(port);
    sal.sin_addr.s_addr = INADDR_ANY;

    if (0 > (sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
        perror("socket() failed\n");
        exit(-1);
    }

    if (0 > bind(sd, (struct sockaddr *)&sal, sizeof(sal))) {
        perror("bind() failed\n");
        exit(-1);
    }

    if (0 > listen(sd, MAXCLIENTS)) {
        perror("listen() failed\n");
        exit(-1);
    }

    fprintf(stdout, "Server listening on Port: %d\n", port);

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(sd, &rfds);
    maxfd = MAX(sd, STDIN_FILENO);

    for (;;) {
        memset(buffer, 0, sizeof(buffer));
		memset(&sac, 0, sizeof(sac));
		slen = sizeof(sac);
		fs = 0;

        rfd = rfds;
        if (0 > select(maxfd + 1, &rfd, NULL, NULL, NULL)) {
            perror("select() failed\n");
            exit(-1);
        }

        if (FD_ISSET(sd, &rfd)) {
            if (0 > (csd = accept(sd, (struct sockaddr*)&sac, &slen))) {
                perror("accept() failed\n");
                exit(-1);
            }
            c = malloc(sizeof(struct client));
            memset(c, 0, sizeof(*c));
            c->sa = sac;
            c->sd = csd;

            list_add(&c->list, &cl);

            FD_SET(c->sd, &rfds);
            maxfd = MAX(maxfd, c->sd);

            fprintf(stdout, "%s:%d connected\n", inet_ntoa(c->sa.sin_addr), ntohs(c->sa.sin_port));
            continue;
        } else if (FD_ISSET(STDIN_FILENO, &rfd)) {
            if (NULL == fgets(buffer, sizeof(buffer), stdin)) {
                fprintf(stderr, "fgets() failed\n");
				continue;
            }
            fs = 1;
        } else {
            list_for_each_entry(c, &cl, list) {
                if (!FD_ISSET(c->sd, &rfd)) 
                    continue;

                ret = recv(c->sd, buffer, BUFFLEN - 1, 0);
                if (0 > ret) {
                    if (errno == EINTR)
						continue;
					if (errno == 32) {
						cl_remove(c, &rfds);
						continue;
					}
                    fprintf(stderr, "errno=%d\n", errno);
					exit(1);
                } else if (0 == ret) {
					fprintf(stdout, "%s:%d disconnected\n", inet_ntoa(c->sa.sin_addr), ntohs(c->sa.sin_port));
					cl_remove(c, &rfds);
					break;
                }

                sac = c->sa;
            }
        }

        list_for_each_entry_safe(c, tmp, &cl, list) {
			if (0 == memcmp(&c->sa, &sac, sizeof(sac)))
				continue;

			ret = send(c->sd, buffer, strlen(buffer), 0);
			if (0 > ret) {
				if (errno == EINTR) {
					continue;
				}
				if (errno == 32) {
					cl_remove(c, &rfds);
					continue;
				}
				fprintf(stderr, "errno=%d\n", errno);
				exit(1);
			}
		}

        if (!fs) {
            fprintf(stdout, "%s:%d: %s", inet_ntoa(c->sa.sin_addr), ntohs(c->sa.sin_port), buffer);
        }
    }

    (void) close(sd);

	list_for_each_entry_safe(c, tmp, &cl, list) {
		close(c->sd);
		list_del(&c->list);
		free(c);
	}

    return 0;
}
