#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

#include "config.h"
#include "tfs_server_macros.h"

extern int req_pipe;
extern int open_session_table[S];
extern int thread_exit;

extern pthread_mutex_t open_session_locks[S];
extern unsigned char free_open_session_entries[S];

/* tfs_write is the most expensive request */
/* We are considering we only use the size provided by the teachers in the
 * filesystem. */
/* (char) op_code | (int) session_id | (int) fhandle | (size_t) len | contents
 */
#define PROD_CONS_SIZE                                                         \
    (sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + BLOCK_SIZE)

typedef struct {
    pthread_mutex_t mutex;
    char buffer[PROD_CONS_SIZE];
    char *prod_ptr;
    char *cons_ptr;
} prod_cons_t;

ssize_t thread_read_data_cons(void *dest, size_t n, size_t session_id);
int thread_worker_schedule_prod(size_t session_id, char op_code);
void *thread_wait(void *arg);
void main_thread_work();
void init_threads();
void fini_threads();

#endif /*THREAD_H*/
