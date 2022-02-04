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

char *req_pipe_name;

static inline int get_session_fd(size_t session_id) {
    fail_exit_if(pthread_mutex_lock(&open_session_locks[session_id]),
                 E_LOCK_SESSION_TABLE_MUTEX);
    int rc = open_session_table[session_id];
    fail_exit_if(pthread_mutex_unlock(&open_session_locks[session_id]),
                 E_UNLOCK_SESSION_TABLE_MUTEX);
    return rc;
}

static inline void set_session_fd(size_t session_id, int fd) {
    fail_exit_if(pthread_mutex_lock(&open_session_locks[session_id]),
                 E_LOCK_SESSION_TABLE_MUTEX);
    open_session_table[session_id] = fd;
    fail_exit_if(pthread_mutex_unlock(&open_session_locks[session_id]),
                 E_UNLOCK_SESSION_TABLE_MUTEX);
}

static inline int r_pipe_inform_session(size_t session_id, int res) {
    return r_pipe_inform(get_session_fd(session_id), res);
}

void do_unmount(size_t session_id, bool inform) {
    int fd = get_session_fd(session_id);
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

    /* Unmount any files that may be open in case the client died, for example.
     */
    if (open_files_amount[session_id] != 0) {
        for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
            if (open_files[session_id][i] != -1) {
                // fprintf(stderr, "Closing file %u\n",
                // open_files[session_id][i]);
                tfs_close(open_files[session_id][i]);
                open_files[session_id][i] = -1;
                --open_files_amount[session_id];
            }
        }
    }

    /* We don't verify close, because we don't think we need to kill the server
     * if a
     * file descriptor goes unused. We'll just let it abort when the fd table is
     * full... */
    try_close(fd);
}

static inline void fail_unmount(size_t session_id, const char *msg) {
    perror(msg);
    do_unmount(session_id, false);
}

static ssize_t try_pipe_write_session(size_t session_id, const void *buf,
                                      size_t count) {
    const int session = get_session_fd(session_id);

    return try_pipe_write(session, buf, count);
}

static inline int unlock_session_mutex_fail_inform_pipe(int fd) {
    perror("[ERR] FATAL! Could not unlock a session mutex");
    r_pipe_inform(fd, -1);
    return to_abort_not_recoverable();
}

void server_mount_state(size_t session_id) {
    char client_pipe_path[PATHNAME_MAX_SIZE + 1];

    if (thread_read_data_cons(client_pipe_path, PATHNAME_MAX_SIZE,
                              session_id) == -1) {
        perror(E_READ_PROD_CONS);
        return;
    }

    client_pipe_path[PATHNAME_MAX_SIZE] = '\0';

    int fclient = try_open(client_pipe_path, O_WRONLY);
    if (fclient == -1) {
        perror(E_OPEN_CLIENT_PIPE);
        return;
    }

    /* Save the file descriptor. */
    set_session_fd(session_id, fclient);

    if (r_pipe_inform(fclient, (int)session_id) == -1) {
        do_unmount(session_id, 0);
        return;
    }
}

void server_unmount_state(size_t session_id) { do_unmount(session_id, true); }

void server_open_state(size_t session_id) {
    char name[PIPEPATH_MAX_SIZE + 1];

    if (thread_read_data_cons(name, PIPEPATH_MAX_SIZE, session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    name[PIPEPATH_MAX_SIZE] = '\0';

    int flags;
    if (thread_read_data_cons(&flags, sizeof(int), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    int fd;
    if ((fd = tfs_open(name, flags)) == -1) {
        perror(E_TFS_OPEN);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    open_files_amount[session_id]++;

    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (open_files[session_id][i] == -1)
            open_files[session_id][i] = fd;
    }

    if (r_pipe_inform_session(session_id, fd) == -1) {
        do_unmount(session_id, 0);
        return;
    }
}

void server_close_state(size_t session_id) {
    int fhandle;

    if (thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    if (tfs_close(fhandle) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (open_files[session_id][i] == fhandle) {
            open_files[session_id][i] = -1;
            open_files_amount[session_id]--;
        }
    }

    if (r_pipe_inform_session(session_id, 0) == -1) {
        do_unmount(session_id, 0);
        return;
    }
}

/* We are considering we only use the size provided by the teachers in the
 * filesystem. */
/* Request is composed by result and file contents. */
#define READ_BUFFERS_SIZE (sizeof(int) + BLOCK_SIZE)

static char tfs_read_buffers[S][READ_BUFFERS_SIZE];

void server_read_state(size_t session_id) {
    int fhandle;
    if (thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    size_t len;
    if (thread_read_data_cons(&len, sizeof(size_t), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    char *const buffer = tfs_read_buffers[session_id];

    const unsigned was_read =
        (unsigned)tfs_read(fhandle, buffer + sizeof(int), len);

    memcpy(buffer, &was_read, sizeof(unsigned));

    printf("str rd: %s\n", buffer + sizeof(unsigned));
    if (try_pipe_write_session(session_id, buffer,
                               was_read + sizeof(unsigned)) == -1) {
        do_unmount(session_id, 0);
        return;
    }
}

/* We are considering we only use the size provided by the teachers in the
 * filesystem. */
#define WRITE_BUFFERS_SIZE (BLOCK_SIZE)

static char tfs_write_buffers[S][BLOCK_SIZE];

void server_write_state(size_t session_id) {
    int fhandle;
    if (thread_read_data_cons(&fhandle, sizeof(int), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    size_t len;
    if (thread_read_data_cons(&len, sizeof(size_t), session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    char *const buffer = tfs_write_buffers[session_id];
    if (thread_read_data_cons(buffer, len, session_id) == -1) {
        perror(E_READ_PROD_CONS);
        if (r_pipe_inform_session(session_id, -1) == -1) {
            do_unmount(session_id, 0);
            return;
        }
        return;
    }

    const int was_written = (int)tfs_write(fhandle, buffer, len);

    if (r_pipe_inform_session(session_id, was_written) == -1) {
        do_unmount(session_id, 0);
        return;
    }
}

void server_shutdown_after_all_closed_state(size_t session_id) {
    r_pipe_inform_session(session_id, tfs_destroy_after_all_closed());
    unlink(req_pipe_name);
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

    req_pipe_name = pipename;

    init_threads();

    puts("OHAYO!~");

    main_thread_work();

    fini_threads();

    return 0;
}
