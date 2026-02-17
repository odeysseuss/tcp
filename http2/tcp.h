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
int tcpHandler(Conn *conn, void (*handler)(Conn *conn));
int tcpPoll(Listener *listener);
void tcpCloseListener(Listener *listener);
void tcpCloseConn(Conn *conn);

ssize_t sendAll(int fd, const void *buf, size_t len);

#ifdef TCP_IMPLEMENTATION

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
    if (!listener) {
        perror("malloc");
        return NULL;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        free_(listener);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(fd);
        free_(listener);
        return NULL;
    }

    if (setNonBlockingSocket_(fd) == -1) {
        close(fd);
        free_(listener);
        return NULL;
    }

    listener->fd = fd;
    listener->size = sizeof(listener->addr);
    listener->addr.sin_family = AF_INET;
    listener->addr.sin_port = htons(port);
    listener->addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&listener->addr, listener->size) == -1) {
        perror("bind");
        close(fd);
        free_(listener);
        return NULL;
    }

    if (listen(fd, SOMAXCONN) == -1) {
        perror("listen");
        close(fd);
        free_(listener);
        return NULL;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(fd);
        free_(listener);
        return NULL;
    }

    listener->epoll_fd = epoll_fd;
    listener->ev.data.fd = fd;
    listener->ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &listener->ev) == -1) {
        perror("epoll ctl listener");
        close(epoll_fd);
        close(fd);
        free_(listener);
        return NULL;
    }

    return listener;
}

static int addtoEpollList_(Conn *conn, Listener *listener) {
    if (!listener || !conn) {
        return -1;
    }

    struct epoll_event ev;
    ev.data.ptr = conn;
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    if (epoll_ctl(listener->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev) == -1) {
        perror("epoll_ctl add client");
        return -1;
    }

    return 0;
}

Conn *tcpAccept(Listener *listener) {
    if (!listener) {
        return NULL;
    }

    Conn *conn = malloc_(sizeof(Conn));
    if (!conn) {
        perror("malloc conn");
        return NULL;
    }

    conn->size = sizeof(struct sockaddr_in);
    int conn_fd = accept(listener->fd, (struct sockaddr *)&conn->addr, &conn->size);

    if (conn_fd == -1) {
        free_(conn);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NULL;
        }
        perror("accept");
        return NULL;
    }

    conn->fd = conn_fd;

    if (setNonBlockingSocket_(conn_fd) == -1) {
        goto clean;
    }

    if (addtoEpollList_(conn, listener) == -1) {
        goto clean;
    }

    return conn;

clean:
    close(conn_fd);
    free_(conn);
    return NULL;
}

int tcpHandler(Conn *conn, void (*handler)(Conn *conn)) {
    if (!conn || !handler) {
        return -1;
    }

    handler(conn);
    return 0;
}

int tcpPoll(Listener *listener) {
    int nfds = epoll_wait(listener->epoll_fd, listener->events, MAX_EPOLL_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll_wait");
        return -1;
    }

    return nfds;
}

void tcpCloseConn(Conn *conn) {
    if (!conn) {
        return;
    }

    close(conn->fd);
    free_(conn);
}

void tcpCloseListener(Listener *listener) {
    if (!listener) {
        return;
    }

    close(listener->epoll_fd);
    close(listener->fd);
    free_(listener);
}

ssize_t sendAll(int fd, const void *buf, size_t len) {
    if (fd < 0 || !buf) {
        return -1;
    }

    size_t total = 0;
    size_t bytes_left = len;
    ssize_t bytes_send = 0;

    while (total < len) {
        bytes_send = send(fd, (const char *)buf + total, bytes_left, 0);
        if (bytes_send == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("send");
            return -1;
        }
        if (bytes_send == 0) {
            break;  // Connection closed
        }
        total += bytes_send;
        bytes_left -= bytes_send;
    }

    return total;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
