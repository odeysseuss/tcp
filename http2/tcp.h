#ifndef TCP_H
#define TCP_H

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C" {
#endif

/// for custom allocators
#define malloc_ malloc
#define calloc_ calloc
#define realloc_ realloc
#define free_ free

#define MAX_EPOLL_EVENTS 64

typedef struct {
    struct epoll_event ev, events[MAX_EPOLL_EVENTS];
    struct sockaddr_in addr;
    socklen_t size;
    int epoll_fd;
    int fd;
} Listener;

typedef struct {
    struct sockaddr_in addr;
    socklen_t size;
    int fd;
} Conn;

Listener *tcpListen(int port);
Conn *tcpAccept(Listener *listener);
// void tcpHandler(Conn *conn, void (*handler)(Conn *conn));
int tcpHandler(Listener *listener, Conn *conn, void (*handler)(Conn *conn));
void tcpConnClose(Conn *conn);
void tcpListenerClose(Listener *listener);

ssize_t sendAll(int fd, const void *buf, size_t len);

// #ifdef TCP_IMPLEMENTATION

static int setNonBlockingSocket_(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl set");
        return -1;
    }

    return 0;
}

Listener *tcpListen(int port) {
    Listener *listener = malloc_(sizeof(Listener));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        goto clean;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        goto clean;
    }

    if (setNonBlockingSocket_(fd) == -1) {
        goto clean;
    }

    listener->fd = fd;
    listener->size = sizeof(listener->addr);
    listener->addr.sin_family = AF_INET;
    listener->addr.sin_port = htons(port);
    listener->addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&listener->addr, listener->size) == -1) {
        perror("bind");
        goto clean;
    }

    if (listen(fd, SOMAXCONN) == -1) {
        perror("listen");
        goto clean;
    }

    int epoll_fd = epoll_create1(SOCK_CLOEXEC);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        goto clean;
    }

    listener->epoll_fd = epoll_fd;
    listener->ev.data.fd = fd;
    listener->ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &listener->ev) == -1) {
        perror("epoll ctl listener");
        goto clean;
    }

    return listener;

clean:
    free_(listener);
    return NULL;
}

Conn *tcpAccept(Listener *listener) {
    Conn *conn = malloc_(sizeof(Conn));

    conn->size = sizeof(struct sockaddr_in);
    int conn_fd =
        accept(listener->fd, (struct sockaddr *)&conn->addr, &conn->size);

    if (conn_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NULL;
        }
        perror("accept");
        goto clean;
    }

    if (setNonBlockingSocket_(conn_fd) == -1) {
        goto clean;
    }

    conn->fd = conn_fd;
    return conn;

clean:
    free_(conn);
    return NULL;
}

static int addtoEpollList_(int conn_fd, Listener *listener) {
    if (!listener) {
        return -1;
    }

    listener->ev.data.fd = conn_fd;
    listener->ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    if (epoll_ctl(listener->epoll_fd, EPOLL_CTL_ADD, conn_fd, &listener->ev) ==
        -1) {
        perror("epoll ctl listener");
        return -1;
    }

    return 0;
}

int tcpHandler(Listener *listener, Conn *conn, void (*handler)(Conn *conn)) {
    if (!listener || !conn || !handler) {
        return -1;
    }

    int nfds =
        epoll_wait(listener->epoll_fd, listener->events, MAX_EPOLL_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll_ctl client");
        return -1;
    }

    for (int i = 0; i < nfds; i++) {
        if (listener->events[i].data.fd == listener->fd) {
            if (addtoEpollList_(conn->fd, listener) == -1) {
                close(conn->fd);
                continue;
            }
        } else {
            handler(conn);
        }
    }

    return 0;
}

void tcpConnClose(Conn *conn) {
    if (!conn) {
        return;
    }

    close(conn->fd);
    free_(conn);
}

void tcpListenerClose(Listener *listener) {
    if (!listener) {
        return;
    }

    close(listener->fd);
    free_(listener);
}

ssize_t sendAll(int fd, const void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    ssize_t bytes_send = 0;

    while (total < len) {
        bytes_send = send(fd, (char *)buf + total, bytes_left, 0);
        if (bytes_send == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("send");
            return -1;
        }
        total += bytes_send;
        bytes_left -= bytes_send;
    }

    return total;
}

// #endif

#ifdef __cplusplus
}
#endif

#endif
