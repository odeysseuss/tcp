#define TCP_IMPLEMENTATION
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "tcp.h"

void readAndWrite(Conn *conn) {
    if (!conn || conn->fd < 0) {
        return;
    }

    char buf[1024];
    char str[INET_ADDRSTRLEN];
    ssize_t bytes_recv;

    while (1) {
        bytes_recv = recv(conn->fd, buf, sizeof(buf) - 1, 0);

        if (bytes_recv == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("recv");
            goto clean;
        }

        if (bytes_recv == 0) {
            fprintf(stdout,
                    "[Disconnected] %s:%d (fd: %d)\n",
                    inet_ntop(AF_INET, &conn->addr, str, INET_ADDRSTRLEN),
                    ntohs(conn->port),
                    conn->fd);

            goto clean;
        }

        ssize_t bytes_send = tcpSendAll(conn->fd, buf, bytes_recv);
        if (bytes_send == -1) {
            perror("sendAll");
            goto clean;
        }
    }

    return;

clean:
    tcpCloseConn(conn);
    return;
}

int main(void) {
    char str[INET_ADDRSTRLEN];

    Listener *listener = tcpListen(8000);
    if (!listener) {
        return -1;
    }

    fprintf(stdout,
            "[Listening] %s:%d\n",
            inet_ntop(AF_INET, &listener->addr, str, INET_ADDRSTRLEN),
            ntohs(listener->port));

    while (1) {
        Event *event = tcpPoll(listener);
        if (!event) {
            break;
        }

        int nfds = event->nfds;
        for (int i = 0; i < nfds; i++) {
            if (event->events[i].events & (EPOLLERR | EPOLLHUP)) {
                continue;
            }

            if (event->events[i].data.fd == listener->fd) {
                while (1) {
                    Conn *conn = tcpAccept(listener);
                    if (!conn) {
                        break;
                    }

                    fprintf(
                        stdout,
                        "[Connected] %s:%d (fd: %d)\n",
                        inet_ntop(AF_INET, &conn->addr, str, INET_ADDRSTRLEN),
                        ntohs(conn->port),
                        conn->fd);
                }
            } else {
                Conn *conn = event->events[i].data.ptr;
                if (tcpHandler(conn, readAndWrite) == -1) {
                    fprintf(stderr, "Error in tcpHandler\n");
                }
            }
        }
    }

    tcpCloseListener(listener);

    return 0;
}
