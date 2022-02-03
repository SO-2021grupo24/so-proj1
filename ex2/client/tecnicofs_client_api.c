#include "tecnicofs_client_api.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common/common.h"
#include "tecnicofs_client_errors.h"

int session_id;
int fclient = -1;
int fserver;
char _client_pipe_path[PATHNAME_MAX_SIZE + 1];

void *memccpy(void *restrict dest, const void *restrict src, int c,
              size_t count);

#define R_FAIL_IF(arg, msg, err)                                               \
    do {                                                                       \
        if (arg) {                                                             \
            perror(msg);                                                       \
            return err;                                                        \
        }                                                                      \
    } while (0)

static void handle_interr() {
    puts("Interruption!");
    if (fclient == -1) {
        /* Open pipe because we may have closed a client in mount between
         * sending pathname and opening client fifo (which locks the server). */
        fclient = try_open(_client_pipe_path, O_RDONLY);
        try_close(fclient);
    } else {
        tfs_unmount();
    }
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    static bool sig = false;
    if (!sig) {
        signal(SIGINT, handle_interr);
        sig = true;
    }

    printf("mount %d\n", getpid());

    strncpy(_client_pipe_path, client_pipe_path, PATHNAME_MAX_SIZE);

    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        return -1;
    }
    if (mkfifo(client_pipe_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return -1;
    }

    fserver = try_open(server_pipe_path, O_WRONLY);

    char buffer[MOUNT_BUFFER_SZ] = {TFS_OP_CODE_MOUNT};
    memccpy(buffer + sizeof(char), client_pipe_path, 0, PATHNAME_MAX_SIZE);

    R_FAIL_IF(try_pipe_write(fserver, buffer, MOUNT_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    fclient = try_open(client_pipe_path, O_RDONLY);
    printf("mount past open %d\n", getpid());

    int id;
    R_FAIL_IF(try_read(fclient, &id, sizeof(int)) == -1, E_CLIENT_PIPE_READ,
              -1);

    if (id == -1)
        return -1;

    printf("session %lu\n", (unsigned long)id);
    session_id = id;

    printf("mount done %d\n", getpid());
    return 0;
}

static int unmount_close_pipes(int res) {
    try_close(fserver);

    try_close(fclient);

    fclient = -1;

    if (unlink(_client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", _client_pipe_path,
                strerror(errno));
        return -1;
    }

    printf("umount done %d\n", getpid());
    return res;
}

int tfs_unmount() {
    printf("umount %d\n", getpid());
    char buffer[UNMOUNT_BUFFER_SZ] = {TFS_OP_CODE_UNMOUNT};

    memcpy(buffer + sizeof(char), &session_id, sizeof(int));

    R_FAIL_IF(try_pipe_write(fserver, buffer, UNMOUNT_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, unmount_close_pipes(-1));

    printf("Reading %d\n", session_id);
    int res;
    R_FAIL_IF(try_read_all(fclient, &res, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, unmount_close_pipes(-1));
    printf("Returned %d %d\n", session_id, res);

    return unmount_close_pipes(res);
}

int tfs_open(char const *name, int flags) {
    printf("open %d\n", getpid());
    char buffer[OPEN_BUFFER_SZ] = {TFS_OP_CODE_OPEN};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memccpy(ptr += sizeof(int), name, 0, PATHNAME_MAX_SIZE);
    memcpy(ptr += PATHNAME_MAX_SIZE, &flags, sizeof(int));

    R_FAIL_IF(try_pipe_write(fserver, buffer, OPEN_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    int res;
    R_FAIL_IF(try_read_all(fclient, &res, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("open done %d\n", getpid());
    return res;
}

int tfs_close(int fhandle) {
    printf("close %d\n", getpid());
    char buffer[CLOSE_BUFFER_SZ] = {TFS_OP_CODE_CLOSE};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));

    R_FAIL_IF(try_pipe_write(fserver, buffer, CLOSE_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    int res;
    R_FAIL_IF(try_read_all(fclient, &res, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("close done %d\n", getpid());
    return res;
}

ssize_t tfs_write(int fhandle, void const *in_buffer, size_t len) {
    printf("write %d\n", getpid());
    char buffer[WRITE_BUFFER_SZ] = {TFS_OP_CODE_WRITE};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));
    memcpy(ptr += sizeof(int), &len, sizeof(size_t));
    memcpy(ptr += sizeof(size_t), in_buffer, len * sizeof(char));

    R_FAIL_IF(try_pipe_write(fserver, buffer,
                             WRITE_BUFFER_SZ - BLOCK_SIZE + len) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    int res;
    R_FAIL_IF(try_read_all(fclient, &res, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("write done %d\n", getpid());
    return (ssize_t)res;
}

ssize_t tfs_read(int fhandle, void *out_buffer, size_t len) {
    printf("read %d\n", getpid());
    char buffer[READ_BUFFER_SZ] = {TFS_OP_CODE_READ};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));
    memcpy(ptr += sizeof(int), &len, sizeof(size_t));

    R_FAIL_IF(try_pipe_write(fserver, buffer, READ_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    int was_read;
    R_FAIL_IF(try_read_all(fclient, &was_read, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("Was read: %d\n", was_read);
    if (was_read == -1 || was_read > BLOCK_SIZE) {
        return -1;
    }

    R_FAIL_IF(try_read_all(fclient, out_buffer, (size_t)was_read) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("read done %d\n", getpid());
    return (ssize_t)was_read;
}

int tfs_shutdown_after_all_closed() {
    printf("shutdown %d\n", getpid());
    char buffer[TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED] = {
        TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));

    R_FAIL_IF(try_pipe_write(fserver, buffer, SHUTDOWN_BUFFER_SZ) == -1,
              E_REQUESTS_PIPE_WRITE, -1);

    int res;
    R_FAIL_IF(try_read_all(fclient, &res, sizeof(int)) == -1,
              E_CLIENT_PIPE_READ, -1);

    printf("shutdown done %d\n", getpid());
    return res;
}
