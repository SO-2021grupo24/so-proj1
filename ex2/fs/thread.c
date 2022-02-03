#include "thread.h"
#include "config.h"
#include "tfs_server.h"
#include "tfs_server_essential.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int req_pipe;

unsigned char free_open_session_entries[S];
int open_session_table[S];

static pthread_t threads[S];
pthread_mutex_t threads_mutex[S];
static pthread_cond_t threads_cond[S];

static prod_cons_t prod_cons[S];

ssize_t thread_read_data_cons(void *dest, size_t n, size_t session_id) {
    prod_cons_t *const cur_pc = &prod_cons[session_id];

    printf("Locking prodcons... %lu\n", session_id);
    fail_exit_if(pthread_mutex_lock(&cur_pc->mutex), E_LOCK_PROD_CONS_MUTEX);
    printf("Working prodcons... %lu\n", session_id);

    char *const upper = cur_pc->cons_ptr + (n / sizeof(char));

    if (upper > cur_pc->prod_ptr)
        return -1;

    memcpy(dest, (void *)cur_pc->cons_ptr, n);

    cur_pc->cons_ptr = upper;

    fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                 E_UNLOCK_PROD_CONS_MUTEX);

    return (ssize_t)n;
}

#define IPC_ALREADY_READ (sizeof(char) + sizeof(int))
static size_t ipc_sizes[TFS_OP_CODE_AMOUNT] = {
    0,
    MOUNT_BUFFER_SZ - sizeof(char),
    UNMOUNT_BUFFER_SZ - IPC_ALREADY_READ,
    OPEN_BUFFER_SZ - IPC_ALREADY_READ,
    CLOSE_BUFFER_SZ - IPC_ALREADY_READ,
    WRITE_BUFFER_SZ - IPC_ALREADY_READ,
    READ_BUFFER_SZ - IPC_ALREADY_READ,
    SHUTDOWN_BUFFER_SZ - IPC_ALREADY_READ};
#undef IPC_ALREADY_READ

#define R_FAIL_IF(arg, msg, err)                                               \
    do {                                                                       \
        if (arg) {                                                             \
            perror(msg);                                                       \
            return err;                                                        \
        }                                                                      \
    } while (0)

void thread_worker_schedule_prod(size_t session_id, char op_code) {
    printf("OP Code: %hhd Session: %lu\n", op_code, session_id);
    prod_cons_t *const cur_pc = &prod_cons[session_id];

    fail_exit_if(pthread_mutex_lock(&cur_pc->mutex), E_LOCK_PROD_CONS_MUTEX);

    /* Reset prod_ptr */
    cur_pc->prod_ptr = cur_pc->cons_ptr = cur_pc->buffer;

    /* Save op_code at start and the info that has yet to be analyzed after. */
    *cur_pc->prod_ptr++ = op_code;

    if (op_code == TFS_OP_CODE_UNMOUNT) {
        fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                     E_UNLOCK_PROD_CONS_MUTEX);
        return;
    }

    /* Read all the request and save in the thread buffer. */
    ssize_t amount;

    size_t sz = ipc_sizes[(int)op_code];
    if (op_code == TFS_OP_CODE_WRITE) {
        if ((amount = try_read_all(req_pipe, cur_pc->prod_ptr,
                                   sz - BLOCK_SIZE)) != sz - BLOCK_SIZE) {
            fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                         E_UNLOCK_PROD_CONS_MUTEX);
            fail_exit_if(amount == -1, E_READ_REQUESTS_PIPE);
            perror(E_INVALID_REQUEST);
            exit(EXIT_FAILURE);
        }
        cur_pc->prod_ptr += ((size_t)amount / sizeof(char));
        memcpy(&sz, cur_pc->prod_ptr - sizeof(size_t), sizeof(size_t));
        if ((amount = try_read_all(req_pipe, cur_pc->prod_ptr, sz)) != sz) {
            fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                         E_UNLOCK_PROD_CONS_MUTEX);
            fail_exit_if(amount == -1, E_READ_REQUESTS_PIPE);
            perror(E_INVALID_REQUEST);
            exit(EXIT_FAILURE);
        }

        fail_exit_if(amount == -1, E_READ_REQUESTS_PIPE);
        cur_pc->prod_ptr += ((size_t)amount / sizeof(char));
    }

    else {
        if ((amount = try_read_all(req_pipe, cur_pc->prod_ptr, sz)) != sz) {
            fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                         E_UNLOCK_PROD_CONS_MUTEX);
            fail_exit_if(amount == -1, E_READ_REQUESTS_PIPE);
            perror(E_INVALID_REQUEST);
            exit(EXIT_FAILURE);
        }
        fail_exit_if(amount == -1, E_READ_REQUESTS_PIPE);
        cur_pc->prod_ptr += ((size_t)amount / sizeof(char));
    }

    fail_exit_if(pthread_mutex_unlock(&cur_pc->mutex),
                 E_UNLOCK_PROD_CONS_MUTEX);
    return;
}

void *thread_wait(void *arg) {
    const size_t session_id = (const size_t)arg;

    fail_exit_if(pthread_mutex_lock(&threads_mutex[session_id]),
                 E_LOCK_SESSION_MUTEX);

    while (true) {
        fail_exit_if(pthread_cond_wait(&threads_cond[session_id],
                                       &threads_mutex[session_id]),
                     E_WAIT_SESSION_CONDVAR);
        printf("Woke %ld\n", session_id);

        char op_code = TFS_OP_CODE_NO_OP;

        thread_read_data_cons(&op_code, sizeof(char), session_id);

        switch (op_code) {
        case TFS_OP_CODE_NO_OP:
            break;
        case TFS_OP_CODE_MOUNT:
            printf("mount %lu %lu\n", session_id, pthread_self());
            server_mount_state(session_id);
            printf("mount done %lu\n", session_id);
            break;
        case TFS_OP_CODE_UNMOUNT:
            printf("unmount %lu\n", session_id);
            server_unmount_state(session_id);
            printf("unmount done %lu\n", session_id);
            break;
        case TFS_OP_CODE_OPEN:
            printf("open %lu\n", session_id);
            server_open_state(session_id);
            printf("open done %lu\n", session_id);
            break;
        case TFS_OP_CODE_WRITE:
            printf("write %lu\n", session_id);
            server_write_state(session_id);
            printf("write done %lu\n", session_id);
            break;
        case TFS_OP_CODE_READ:
            printf("read %lu\n", session_id);
            server_read_state(session_id);
            printf("read done %lu\n", session_id);
            break;
        case TFS_OP_CODE_CLOSE:
            printf("close %lu\n", session_id);
            server_close_state(session_id);
            printf("close done %lu\n", session_id);
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            printf("shutdown %lu\n", session_id);
            server_shutdown_after_all_closed_state(session_id);
            printf("shutdown done %lu\n", session_id);
            break;
        default:
            exit(EXIT_FAILURE);
        }
        printf("Stopping session %lu\n", session_id);
    }

    fail_exit_if(pthread_mutex_unlock(&threads_mutex[session_id]),
                 E_UNLOCK_SESSION_MUTEX);

    return NULL;
}

static int try_make_pipe_and_send_result(int rc) {
    char client_pipe_path[PATHNAME_MAX_SIZE];

    fail_exit_if(try_read_all(req_pipe, client_pipe_path, PATHNAME_MAX_SIZE) !=
                     PATHNAME_MAX_SIZE,
                 E_INVALID_REQUEST);

    int fclient = try_open(client_pipe_path, O_WRONLY);

    if (fclient == -1)
        return -1;

    return r_pipe_inform(fclient, rc);
}

static void try_make_pipe_and_send_result_fail_exit_if(bool arg,
                                                       const char *msg) {
    if (arg) {
        perror(msg);
        try_make_pipe_and_send_result(-1);
        exit(EXIT_FAILURE);
    }
}

static int decide_mount() {
    // fprintf(stderr, "trying %lu\n", pthread_self());
    for (int i = 0; i < S; i++) {
        try_make_pipe_and_send_result_fail_exit_if(
            pthread_mutex_lock(&threads_mutex[i]) != 0, E_LOCK_SESSION_MUTEX);

        if (free_open_session_entries[i] == FREE) {
            printf("Mount decided id: %d %lu\n", i, pthread_self());
            free_open_session_entries[i] = TAKEN;

            try_make_pipe_and_send_result_fail_exit_if(
                pthread_mutex_unlock(&threads_mutex[i]) != 0,
                E_UNLOCK_SESSION_MUTEX);

            /* We found an available entry! */
            return i;
        }

        try_make_pipe_and_send_result_fail_exit_if(
            pthread_mutex_unlock(&threads_mutex[i]) != 0,
            E_UNLOCK_SESSION_MUTEX);
    }

    /* We return -1 for error... */
    // fprintf(stderr, "not found %lu\n", pthread_self());
    return try_make_pipe_and_send_result(-1);
}

void main_thread_work() {
    while (true) {
        ssize_t sz;
        char op_code;

        do {
            fail_exit_if((sz = try_read(req_pipe, &op_code, sizeof(char))) ==
                             -1,
                         E_READ_REQUESTS_PIPE);
        } while (sz == 0);

        ssize_t session_id;

        if (op_code == TFS_OP_CODE_NO_OP)
            continue;

        if (op_code == TFS_OP_CODE_MOUNT) {
            if ((session_id = decide_mount()) == -1)
                continue;
        }

        else {
            ssize_t data = 0;
            fail_exit_if(
                (data = try_read_all(req_pipe, &session_id, sizeof(int))) == -1,
                E_READ_REQUESTS_PIPE);
            if (data != sizeof(int) || session_id < 0 || session_id >= S)
                continue;
        }

        thread_worker_schedule_prod((unsigned)session_id, op_code);
        printf("Waking session %lu\n", session_id);
        fail_exit_if(pthread_mutex_lock(&threads_mutex[session_id]),
                     E_LOCK_SESSION_MUTEX);
        fail_exit_if(pthread_cond_signal(&threads_cond[session_id]),
                     E_SIGNAL_SESSION_CONDVAR);
        fail_exit_if(pthread_mutex_unlock(&threads_mutex[session_id]),
                     E_UNLOCK_SESSION_MUTEX);
    }
}

void init_threads() {
    for (int i = 0; i < S; ++i) {
        fail_exit_if(pthread_mutex_init(&threads_mutex[i], NULL),
                     E_INIT_SESSION_MUTEX);
        prod_cons_t *const cur_pc = &prod_cons[i];

        fail_exit_if(pthread_mutex_init(&cur_pc->mutex, NULL),
                     E_INIT_PROD_CONS_MUTEX);

        cur_pc->prod_ptr = cur_pc->cons_ptr = cur_pc->buffer;
        fail_exit_if(pthread_cond_init(&threads_cond[i], NULL),
                     E_INIT_SESSION_CONDVAR);

        fail_exit_if(pthread_mutex_init(&threads_mutex[i], NULL),
                     E_INIT_SESSION_MUTEX);
        free_open_session_entries[i] = FREE;
    }

    for (size_t i = 0; i < S; ++i)
        fail_exit_if(pthread_create(&threads[i], NULL, thread_wait, (void *)i),
                     E_INIT_SESSION_THREAD);
}

void fini_threads() {
    for (int i = 0; i < S; ++i)
        fail_exit_if(pthread_join(threads[i], NULL), E_JOIN_SESSION_THREAD);

    for (int i = 0; i < S; ++i) {
        fail_exit_if(pthread_mutex_destroy(&threads_mutex[i]),
                     E_FINI_SESSION_MUTEX);
        fail_exit_if(pthread_mutex_destroy(&prod_cons[i].mutex),
                     E_FINI_PROD_CONS_MUTEX);
        fail_exit_if(pthread_cond_destroy(&threads_cond[i]),
                     E_FINI_SESSION_CONDVAR);
    }
}
