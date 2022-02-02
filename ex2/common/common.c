#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../fs/tfs_server_errors.h"

void fail_exit_if(bool arg, const char *msg) {
    if (arg) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

int try_close(int fd) {
    int rc = 0;

    do {
        errno = 0;
        rc = close(fd);
    } while (rc == -1 && errno == EINTR);

    if (errno == 0)
        return 0;

    return -1;
}

ssize_t try_read(int fd, void *buf, size_t sz) {
    ssize_t rc = 0;

    do {
        errno = 0;
        rc = read(fd, buf, sz);
    } while (rc == -1 && errno == EINTR);

    if (errno == 0)
        return rc;

    return -1;
}

ssize_t try_read_all(int fd, void *buf, size_t sz) {
    ssize_t bytes = 0;

    do {
        ssize_t rc =
            try_read(fd, (void *)((char *)buf + bytes), sz - (size_t)bytes);

        printf("bytes: %ld %lu %ld\n", bytes, sz, rc);
        if (rc == -1)
            return -1;

        bytes += rc;
    } while (bytes != sz);

    return bytes;
}

int try_open(const char *pathname, int flags) {
    int rc;

    do {
        errno = 0;
        rc = open(pathname, flags);
    } while (rc == -1 && errno == EINTR);

    if (errno == 0)
        return rc;

    return -1;
}

ssize_t try_pipe_write(int fd, const void *buf, size_t count) {
    ssize_t rc = 0;

    do {
        errno = 0;
        rc = write(fd, buf, count);
    } while (rc == -1 && errno == EINTR);

    if (errno == 0)
        return rc;

    if (errno == EPIPE)
        perror(E_SIGPIPE_CLIENT_PIPE);

    else
        perror(E_WRITE_CLIENT_PIPE);

    return -1;
}

int r_pipe_inform(int fd, int res) {
    try_pipe_write(fd, &res, sizeof(int));

    return res;
}
