#define TCP_IMPLEMENTATION
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "tcp.h"

void readAndWrite(Conn *conn) {
    char buf[1024];

    while (1) {
        ssize_t bytes_recv = recv(conn->fd, &buf, 1024, 0);
        printf("recv\n");
        if (bytes_recv == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return;
        }

        ssize_t bytes_send = sendAll(conn->fd, &buf, bytes_recv);
        printf("send\n");
        if (bytes_send == -1) {
            return;
        }
    }
}

int main(void) {
    char str[INET_ADDRSTRLEN];

    Listener *listener = tcpListen(8000);
    if (!listener) {
        return -1;
    }

    fprintf(stdout,
            "[Listening] %s:%d\n",
            inet_ntop(AF_INET, &listener->addr.sin_addr, str, INET_ADDRSTRLEN),
            ntohs(listener->addr.sin_port));

    while (1) {
        Conn *conn = tcpAccept(listener);
        if (!conn) {
            continue;
        }

        fprintf(stdout,
                "[Connected] %s:%d\n",
                inet_ntop(AF_INET, &conn->addr.sin_addr, str, INET_ADDRSTRLEN),
                ntohs(conn->addr.sin_port));

        if (tcpHandler(listener, conn, readAndWrite) == -1) {
            return -1;
        }
    }
    tcpListenerClose(listener);

    return 0;
}
