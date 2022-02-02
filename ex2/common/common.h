#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_NO_OP = 0,
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

#define TFS_OP_CODE_AMOUNT 8
#define BLOCK_SIZE (1024)

#define PATHNAME_MAX_SIZE sizeof(char[40])
#define MOUNT_BUFFER_SZ (sizeof(char) + PATHNAME_MAX_SIZE)
#define UNMOUNT_BUFFER_SZ (sizeof(char) + sizeof(int))
#define OPEN_BUFFER_SZ                                                         \
    (sizeof(char) + sizeof(int) + PATHNAME_MAX_SIZE + sizeof(int))
#define CLOSE_BUFFER_SZ (sizeof(char) + sizeof(int) + sizeof(int))
#define WRITE_BUFFER_SZ                                                        \
    (sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + BLOCK_SIZE)
#define READ_BUFFER_SZ                                                         \
    (sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t))
#define SHUTDOWN_BUFFER_SZ (sizeof(char) + sizeof(int))

void fail_exit_if(bool arg, const char *msg);
int try_close(int fd);
ssize_t try_read(int fd, void *buf, size_t sz);
ssize_t try_read_all(int fd, void *buf, size_t sz);
int try_open(const char *pathname, int flags);
ssize_t try_pipe_write(int fd, const void *buf, size_t count);
int r_pipe_inform(int fd, int res);

static inline int to_abort_not_recoverable() {
    errno = ENOTRECOVERABLE;
    return -1;
}

static inline int to_abort_not_recoverable_log(const char *s) {
    perror(s);
    return to_abort_not_recoverable();
}

#endif /* COMMON_H */
