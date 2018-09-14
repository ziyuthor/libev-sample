/* gcc -O2 -o simple_server simple_server.c -I./inc -L./ -lev -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/tcp.h>

#include <ev.h>

typedef struct Client {
	ev_io rio;
	ev_io wio;
} Client;

const char str[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 5\r\n\r\nget /";
int len = strlen(str);

static void server_send_cb(struct ev_loop *loop, ev_io *w, int revents)
{
        ssize_t s = send(w->fd, str,  len, 0);
		if (s == (ssize_t)(len)) {
            ev_io_stop(loop, w);
            ev_io_start(loop, &((Client *)w->data)->rio);

            return;
		}
        else if (s < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                printf("server_send_cb_send\n");
				ev_io_start(loop, w);
            }
            return;
        } else {
            return;
        }
}

char buff[1460];
static void server_recv_cb(struct ev_loop *loop, ev_io *w, int revents)
{
    ssize_t r = recv(w->fd, buff, 1460, 0);
	if (r > 0) {
		ev_io_stop(loop, w);

		server_send_cb(loop, &((Client *)w->data)->wio, EV_WRITE);
	}
	else if (r == 0) {
		printf("close:socket=%d r=%d errno=%d\n", w->fd, r, errno);
		close(w->fd);
		ev_io_stop(loop, &((Client *)w->data)->wio);
		ev_io_stop(loop, w);
		free((Client *)w->data);
	}
	else {
		printf("close:socket=%d r=%d errno=%d\n", w->fd, r, errno);
		close(w->fd);
		ev_io_stop(loop, &((Client *)w->data)->wio);
		ev_io_stop(loop, w);
		free((Client *)w->data);
	}
}

void accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	static int id = 0;
    //int listenFd = *(int *)w->data;
    int clientFd = accept4(w->fd, NULL, NULL, SOCK_NONBLOCK);
    if (clientFd == -1) {
        printf("accept error %d\n", errno);
        return;
    }
	Client *client = (Client *)malloc(sizeof(Client));
	client->rio.data = client;
	client->wio.data = client;
	ev_io_init(&client->rio, server_recv_cb, clientFd, EV_READ);
	ev_io_init(&client->wio, server_send_cb, clientFd, EV_WRITE);
	ev_io_start(loop, &(client->rio));
}


int Start(const char *ip, unsigned short port)
{
	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family      = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(ip);
	saddr.sin_port        = htons(port);

	struct ev_loop *mainLoop = ev_loop_new(EVBACKEND_EPOLL);

	int listenFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (listenFd < 0) {
		printf("bind() error\n");
	}

	bind(listenFd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (listen(listenFd, SOMAXCONN) == -1) {
		printf("listen() error\n");
	}

	ev_io lio;
	ev_io_init(&lio, accept_cb, listenFd, EV_READ);
	ev_io_start(mainLoop, &lio);

	ev_run(mainLoop, 0);

	return 0;
}

int main(int argc, char **argv)
{
	short port = atoi(argv[2]);

	Start(argv[1], port);

    return 0;
}  
