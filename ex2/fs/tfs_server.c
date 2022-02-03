#include "operations.h"
#include "tecnicofs_client_api.h"
#include "tfs_server_essential.h"
#include "thread.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static inline int r_pipe_inform_session(size_t session_id, int res) {
    return r_pipe_inform(open_session_table[session_id], res);
}

void do_unmount(size_t session_id, bool inform) {
    int fd = open_session_table[session_id];
    if (pthread_mutex_lock(&open_session_locks[session_id]) != 0) {
        if (inform)
            r_pipe_inform_session(session_id, -1);
        perror(E_LOCK_SESSION_TABLE_MUTEX);
        exit(EXIT_FAILURE);
    }

    free_open_session_entries[session_id] = FREE;

    fail_exit_if(pthread_mutex_unlock(&open_session_locks[session_id]),
                 E_UNLOCK_SESSION_TABLE_MUTEX);

    printf("Freeing session %lu\n", session_id);
    if (inform) {
        r_pipe_inform(fd, 0);
    }

    /* We don't verify close, because we don't think we need to kill the server
     * if a
     * file descriptor goes unused. We'll just let it abort when the table is
     * full... */
    // void usleep(unsigned);
    // usleep(1000*1000);
    try_close(fd);
}

static inline void fail_unmount(size_t session_id, const char *msg) {
    perror(msg);
    do_unmount(session_id, false);
}

#define R_FAIL_IF(arg, msg)                                                    \
    do {                                                                       \
        if (arg) {                                                             \
            perror(msg);                                                       \
            return;                                                            \
        }                                                                      \
    } while (0)

#define R_FAIL_UNMOUNT_IF(arg, session_id, msg)                                \
    do {                                                                       \
        if (arg) {                                                             \
            fail_unmount(session_id, msg);                                     \
            return;                                                            \
        }                                                                      \
    } while (0)

#define R_UNMOUNT_IF(arg, session_id)                                          \
    do {                                                                       \
        if (arg) {                                                             \
            do_unmount(session_id, false);                                     \
            return;                                                            \
        }                                                                      \
    } while (0)

#define R_FAIL_REQUEST_IF(arg, session_id, msg)                                \
    do {                                                                       \
        if (arg) {                                                             \
            perror(msg);                                                       \
            R_UNMOUNT_IF(r_pipe_inform_session(session_id, -1) == -1,          \
                         session_id);                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

static ssize_t try_pipe_write_session(size_t session_id, const void *buf,
                                      size_t count) {
    const int session = open_session_table[session_id];

    return try_pipe_write(session, buf, count);
}

static inline int unlock_session_mutex_fail_inform_pipe(int fd) {
    perror("[ERR] FATAL! Could not unlock a session mutex");
    r_pipe_inform(fd, -1);
    return to_abort_not_recoverable();
}

void server_mount_state(size_t session_id) {
    char client_pipe_path[PATHNAME_MAX_SIZE + 1];

    R_FAIL_IF(thread_read_data_cons(client_pipe_path, PATHNAME_MAX_SIZE,
                                    session_id) == -1,
              E_READ_PROD_CONS);
    client_pipe_path[PATHNAME_MAX_SIZE] = '\0';

    int fclient = try_open(client_pipe_path, O_WRONLY);
    R_FAIL_IF(fclient == -1, E_OPEN_CLIENT_PIPE);

    /* Save the file descriptor. */
    open_session_table[session_id] = fclient;

    R_UNMOUNT_IF(r_pipe_inform(fclient, (int)session_id) == -1, session_id);
}

void server_unmount_state(size_t session_id) { do_unmount(session_id, true); }

void server_open_state(size_t session_id) {
    char name[PIPEPATH_MAX_SIZE + 1];

    R_FAIL_REQUEST_IF(
        thread_read_data_cons(name, PIPEPATH_MAX_SIZE, session_id) == -1,
        session_id, E_READ_PROD_CONS);
    name[PIPEPATH_MAX_SIZE] = '\0';

    int flags;
    R_FAIL_REQUEST_IF(thread_read_data_cons(&flags, sizeof(int), session_id) ==
                          -1,
                      session_id, E_READ_PROD_CONS);

    int fd;
    R_FAIL_REQUEST_IF((fd = tfs_open(name, flags)) == -1, session_id,
                      E_TFS_OPEN);

    printf("open: %d %lu %lu\n", fd, (long)session_id,
           (long)open_session_table[session_id]);
    R_UNMOUNT_IF(r_pipe_inform_session(session_id, fd) == -1, session_id);
}

void server_close_state(size_t session_id) {
    int fhandle;
    R_FAIL_REQUEST_IF(
        thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1,
        session_id, E_READ_PROD_CONS);

    R_FAIL_REQUEST_IF(tfs_close(fhandle) == -1, session_id, E_READ_PROD_CONS);

    R_UNMOUNT_IF(r_pipe_inform_session(session_id, 0) == -1, session_id);
}

/* We are considering we only use the size provided by the teachers in the
 * filesystem. */
/* Request is composed by result and file contents. */
#define READ_BUFFERS_SIZE (sizeof(int) + BLOCK_SIZE)

static char tfs_read_buffers[S][READ_BUFFERS_SIZE];

void server_read_state(size_t session_id) {
    int fhandle;
    R_FAIL_REQUEST_IF(
        thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1,
        session_id, E_READ_PROD_CONS);

    size_t len;
    R_FAIL_REQUEST_IF(thread_read_data_cons(&len, sizeof(size_t), session_id) ==
                          -1,
                      session_id, E_READ_PROD_CONS);

    char *const buffer = tfs_read_buffers[session_id];

    const unsigned was_read =
        (unsigned)tfs_read(fhandle, buffer + sizeof(int), len);

    memcpy(buffer, &was_read, sizeof(unsigned));

    R_UNMOUNT_IF(try_pipe_write_session(session_id, buffer,
                                        was_read + sizeof(unsigned)) == -1,
                 session_id);
}

/* We are considering we only use the size provided by the teachers in the
 * filesystem. */
#define WRITE_BUFFERS_SIZE (BLOCK_SIZE)

static char tfs_write_buffers[S][BLOCK_SIZE];

void server_write_state(size_t session_id) {
    int fhandle;
    R_FAIL_REQUEST_IF(
        thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1,
        session_id, E_READ_PROD_CONS);

    size_t len;
    R_FAIL_REQUEST_IF(thread_read_data_cons(&len, sizeof(size_t), session_id) ==
                          -1,
                      session_id, E_READ_PROD_CONS);

    char *const buffer = tfs_write_buffers[session_id];
    R_FAIL_REQUEST_IF(thread_read_data_cons(buffer, len, session_id) == -1,
                      session_id, E_READ_PROD_CONS);

    const int was_written = (int)tfs_write(fhandle, buffer, len);

    R_UNMOUNT_IF(r_pipe_inform_session(session_id, was_written) == -1,
                 session_id);
}

void server_shutdown_after_all_closed_state(size_t session_id) {
    r_pipe_inform_session(session_id, tfs_destroy_after_all_closed());
    puts("OYASUMI!~");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *const pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    // create server pipe
    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, E_UNLINK, pipename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fail_exit_if(mkfifo(pipename, 0640) != 0, E_MKFIFO);

    /* Open pipename in the server so that clients can connect. */
    fail_exit_if((req_pipe = try_open(pipename, O_RDONLY)) == -1,
                 E_OPEN_REQUESTS_PIPE);

    fail_exit_if(tfs_init() == -1, "[ERR]: couldn't initialize file system");

    signal(SIGPIPE, SIG_IGN);

    init_threads();

    puts("OHAYO!~");

    main_thread_work();

    fini_threads();

    return 0;
}
