#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

#define CLIENT_COUNT 20
#define CLIENT_PIPE_NAME_LEN 40
#define CLIENT_PIPE_NAME_FORMAT "/tmp/tfs_c%d"

void run_test(char *server_pipe, int client_id);

int main(int argc, char **argv) {
    if (argc < 2) {
        printf(
            "You must provide the following arguments: 'server_pipe_path'\n");
        return 1;
    }

    int child_pids[CLIENT_COUNT];

    for (int i = 0; i < CLIENT_COUNT; ++i) {
        int pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            /* run test on child */
            run_test(argv[1], i);
            exit(0);
        } else {
            child_pids[i] = pid;
        }
    }

    int rc = 0;
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        int result;
        waitpid(child_pids[i], &result, 0);
        if (WIFEXITED(result))
            printf("Client %d exited successfully.\n", i);
        else {
            rc = -1;
            printf("Client %d did not exit successfully.\n", i);
        }
    }

    assert(rc == 0);
    printf("Successful test.\n");

    return 0;
}

extern int session_id;
void run_test(char *server_pipe, int client_id) {
    printf("Client %d started successfully! Fuck data races, idiots!\n",
           client_id);
    char client_pipe[40];
    sprintf(client_pipe, CLIENT_PIPE_NAME_FORMAT, client_id);
    printf("client mount: client %d pid %d!\n", client_id, getpid());
    assert(tfs_mount(client_pipe, server_pipe) == 0);
    printf("client mount finished: client %d session %d pid %d!\n", client_id,
           session_id, getpid());

    printf("client unmount: %d %d!\n", client_id, session_id);
    assert(tfs_unmount() == 0);
    printf("done: %d %d!\n", client_id, session_id);
}
