/* g++ -O2 -o ev_cpp ev_cpp.cc -I./inc -L./ -lev */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ev++.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <errno.h>
#include <list>
  

class EchoInstance {
private:
    ev::io io;
    static int total_clients;
    int sfd;

    void write_callback(ev::io &watcher, int revents) {
        if (EV_ERROR & revents) {
            perror("got invalid event");
            return;
        }

        if (revents & EV_WRITE)  {
            const char *str = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 5\r\n\r\nget /";
            int len = strlen(str);
            ssize_t written = write(watcher.fd, str, len);
            if (written < 0) {
                perror("read error");
                return;
            }
            else if (written < len) {
                io.set<EchoInstance, &EchoInstance::write_callback>(this);
                io.set(ev::WRITE);
            }
            else {
                io.set<EchoInstance, &EchoInstance::read_callback>(this);
                io.set(ev::READ);
            }
        }
    }
    
    char buffer[1024];
    void read_callback(ev::io &watcher, int revents) {
        if (EV_ERROR & revents) {
            perror("got invalid event");
            return;
        }

        if (revents & EV_READ) {
            ssize_t nread = recv(watcher.fd, buffer, 1024, 0);
            if (nread > 0) {
                io.set<EchoInstance, &EchoInstance::write_callback>(this);
                io.set(ev::WRITE);
            }
            else if (nread == 0) {
                // Gack - we're deleting ourself inside of ourself!
                delete this;
            }
            else { /* nread < 0 */
                perror("read error");
                return;
            }
        }
    }

    // effictivly a close and a destroy
    virtual ~EchoInstance() {
        // Stop and free watcher if client socket is closing
        io.stop();
        close(sfd);

        printf("%d client(s) connected.\n", --total_clients);
    }
  
public:
    EchoInstance(const ev::dynamic_loop &loop, int s) : sfd(s) {
        total_clients++;

        io.set(loop);
        io.set<EchoInstance, &EchoInstance::read_callback>(this);
        io.start(s, ev::READ);
    }
};
  
class EchoServer {
private:
    ev::io io;
    ev::sig sio;
    int s;
  
public:
  
    void io_accept(ev::io &watcher, int revents) {
        if (EV_ERROR & revents) {
            perror("got invalid event");
            return;
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sd = accept4(watcher.fd, (struct sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK);
        if (client_sd < 0) {
            perror("accept error");
            return;
        }

        EchoInstance *client = new EchoInstance(static_cast<ev::dynamic_loop &>(watcher.loop), client_sd);
    }

    static void signal_cb(ev::sig &signal, int revents) {
        signal.loop.break_loop();
        printf("signal int exit\n");
    }

    EchoServer(ev::dynamic_loop &dloop, int port) {
        printf("Listening on port %d\n", port);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        s = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            perror("bind");
        }

        listen(s, SOMAXCONN);

        io.set(dloop);
        io.set<EchoServer, &EchoServer::io_accept>(this);
        io.start(s, ev::READ);

        sio.set(dloop);
        sio.set<&EchoServer::signal_cb>();
        sio.start(SIGINT);
    }
          
    virtual ~EchoServer() {
        shutdown(s, SHUT_RDWR);
        close(s);
    }
};

int EchoInstance::total_clients = 0;

int main(int argc, char **argv) 
{
    int port = 60553;

    if (argc > 1)
        port = atoi(argv[1]);

    ev::dynamic_loop loop(ev::EPOLL);
    EchoServer echo(loop, port);

    loop.run(0);

    return 0;
}
