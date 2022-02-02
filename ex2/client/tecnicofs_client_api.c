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

int session_id;
int fclient;
int fserver;
char _client_pipe_path[PATHNAME_MAX_SIZE + 1];

void *memccpy(void *restrict dest, const void *restrict src, int c,
              size_t count);

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
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

    if (try_pipe_write(fserver, buffer, MOUNT_BUFFER_SZ) == -1) {
        return -1;
    }

    fclient = try_open(client_pipe_path, O_RDONLY);

    int id;
    if (try_read(fclient, &id, sizeof(int)) == -1) {
        return -1;
    }

    if (id == -1) {
        return -1;
    }
    printf("session %lu\n", (unsigned long)id);
    session_id = id;

    strncpy(_client_pipe_path, client_pipe_path, PATHNAME_MAX_SIZE);
    return 0;
}

static int unmount_close_pipes(int res) {
    try_close(fserver);

    try_close(fclient);

    if (unlink(_client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", _client_pipe_path,
                strerror(errno));
        return -1;
    }

    return res;
}

int tfs_unmount() {
    char buffer[UNMOUNT_BUFFER_SZ] = {TFS_OP_CODE_UNMOUNT};

    memcpy(buffer + sizeof(char), &session_id, sizeof(int));

    if (try_pipe_write(fserver, buffer, UNMOUNT_BUFFER_SZ) == -1) {
        return unmount_close_pipes(-1);
    }

    int res;
    if (try_read_all(fclient, &res, sizeof(int)) == -1) {
        return unmount_close_pipes(-1);
    }

    return unmount_close_pipes(res);
}

int tfs_open(char const *name, int flags) {
    char buffer[OPEN_BUFFER_SZ] = {TFS_OP_CODE_OPEN};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memccpy(ptr += sizeof(int), name, 0, PATHNAME_MAX_SIZE);
    memcpy(ptr += PATHNAME_MAX_SIZE, &flags, sizeof(int));

    if (try_pipe_write(fserver, buffer, OPEN_BUFFER_SZ) == -1) {
        return -1;
    }

    int res;
    if (try_read_all(fclient, &res, sizeof(int)) == -1) {
        return -1;
    }

    return res;
}

int tfs_close(int fhandle) {
    char buffer[CLOSE_BUFFER_SZ] = {TFS_OP_CODE_CLOSE};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));

    if (try_pipe_write(fserver, buffer, CLOSE_BUFFER_SZ) == -1) {
        return -1;
    }

    int res;
    if (try_read_all(fclient, &res, sizeof(int)) == -1) {
        return -1;
    }

    return res;
}

ssize_t tfs_write(int fhandle, void const *in_buffer, size_t len) {
    char buffer[WRITE_BUFFER_SZ] = {TFS_OP_CODE_WRITE};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));
    memcpy(ptr += sizeof(int), &len, sizeof(size_t));
    memcpy(ptr += sizeof(size_t), in_buffer, len * sizeof(char));

    if (try_pipe_write(fserver, buffer, WRITE_BUFFER_SZ - BLOCK_SIZE + len) ==
        -1) {
        return -1;
    }

    int res;
    if (try_read_all(fclient, &res, sizeof(int)) == -1) {
        return -1;
    }

    return (ssize_t)res;
}

ssize_t tfs_read(int fhandle, void *out_buffer, size_t len) {
    char buffer[READ_BUFFER_SZ] = {TFS_OP_CODE_READ};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));
    memcpy(ptr += sizeof(int), &fhandle, sizeof(int));
    memcpy(ptr += sizeof(int), &len, sizeof(size_t));

    if (try_pipe_write(fserver, buffer, READ_BUFFER_SZ) == -1) {
        return -1;
    }

    int was_read;
    if (try_read_all(fclient, &was_read, sizeof(int)) == -1) {
        return -1;
    }

    printf("Was read: %d\n", was_read);
    if (was_read == -1 || was_read > BLOCK_SIZE) {
        return -1;
    }

    if (try_read_all(fclient, out_buffer, (size_t)was_read) == -1) {
        return -1;
    }

    return (ssize_t)was_read;
}

int tfs_shutdown_after_all_closed() {
    char buffer[TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED] = {
        TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED};
    char *ptr = buffer;

    memcpy(ptr += sizeof(char), &session_id, sizeof(int));

    if (try_pipe_write(fserver, buffer, SHUTDOWN_BUFFER_SZ) == -1) {
        return -1;
    }

    int res;
    if (try_read_all(fclient, &res, sizeof(int)) == -1) {
        return -1;
    }

    return res;
}
