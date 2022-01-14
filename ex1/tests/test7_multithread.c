#include "fs/operations.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const char *str;
    const size_t sz;
} ct_string_view;

typedef struct {
    int handle;
    int index;
} handle_index;

typedef struct {
    const char *s;
    const char *out_s;
    int index;
} str_idx;

#define THREAD_AMOUNT_RW 4

ct_string_view views[THREAD_AMOUNT_RW] = {
    {.str = "ABCD", .sz = sizeof("ABCD") - 1},
    {.str = "", .sz = sizeof("") - 1},
    {.str = "1234", .sz = sizeof("1234") - 1},
};

int fhandles[2];
const char paths[2][7] = {"/file1", "/file2"};

int th_operations[THREAD_AMOUNT_RW] = {0};

typedef struct {
    int *handle;
    int magic;
} pair_handle_magic;

void *file_from_magic(void *arg) {
    pair_handle_magic *m = (pair_handle_magic *)arg;

    char base[] = "/g1";

    base[2] += (char)m->magic;
    *m->handle = tfs_open(base, TFS_O_CREAT);

    return NULL;
}

int usleep(unsigned);

void *copy_func(void *arg) {
    const str_idx *s = (str_idx *)arg;

    for (int i = 0; i < 2; ++i) {
        usleep(900);
        int f1 = tfs_open(s->s, TFS_O_TRUNC);

        assert(f1 != -1);

        assert(tfs_copy_to_external_fs(s->s, s->out_s) != -1);
    }

    return NULL;
}

void *t_func_w(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;
    const char *s = views[hidx->index].str;
    const size_t sz = views[hidx->index].sz;

    int amount = 100;
    for (int i = 0; i < amount; ++i) {
        const ssize_t sz1 = tfs_write(handle, s, sz);
        assert(sz1 == sz);

        ++th_operations[hidx->index];
    }

    return NULL;
}

void *t_func_r(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;

    int amount = 33;
    for (int i = 0; i < amount; ++i) {
        char str_cur[5];
        ssize_t rc = tfs_read(handle, str_cur, 4);

        if (rc > 0)
            ++th_operations[hidx->index];
    }

    return NULL;
}

int verify_less_operations(char *buf) {
    const size_t len = strlen(buf);

    /* Less reads and writes because of the truncs. */

    /* Both strings used have 4 chars of length. */

    if (th_operations[0] + th_operations[2] >= (len / 4))
        return -1;

    if (th_operations[1] + th_operations[3] >= (len / 4))
        return -1;

    return 0;
}

int test_write_interference(char *buf, size_t len) {
    char *cap = buf + len;
    /* Check if we can iterate string by string in the buffer.
     * Else, there was interference. */
    while (buf != cap) {
        const size_t cur = (size_t)(cap - buf);

        if (cur < 4)
            break;

        if (strncmp(buf, views[0].str, 4) != 0 &&
            strncmp(buf, views[1].str, 4) != 0)
            return -1;

        buf += 4;
    }
    return 0;
}

int main() {
    assert(tfs_init() != -1);

    pthread_t threads[THREAD_AMOUNT_RW + 2];

    /* File handle for the threads. */
    fhandles[0] = tfs_open(paths[0], TFS_O_CREAT);
    assert(fhandles[0] != -1);
    fhandles[1] = tfs_open(paths[1], 0);
    assert(fhandles[1] != -1);

    handle_index hidx[THREAD_AMOUNT_RW];

    hidx[0].handle = fhandles[0];
    hidx[0].index = 0;
    assert(pthread_create(&threads[0], NULL, t_func_w, (void *)&hidx[0]) == 0);

    hidx[1].handle = fhandles[1];
    hidx[1].index = 1;
    assert(pthread_create(&threads[1], NULL, t_func_r, (void *)&hidx[1]) == 0);

    hidx[2].handle = fhandles[0];
    hidx[2].index = 2;
    assert(pthread_create(&threads[2], NULL, t_func_w, (void *)&hidx[2]) == 0);

    hidx[3].handle = fhandles[1];
    hidx[3].index = 3;
    assert(pthread_create(&threads[3], NULL, t_func_r, (void *)&hidx[3]) == 0);

    str_idx s = {.s = paths[0], .out_s = "out1", .index = 0};
    assert(pthread_create(&threads[4], NULL, copy_func, (void *)&s) == 0);

    str_idx u = {.s = paths[1], .out_s = "out2", .index = 1};
    assert(pthread_create(&threads[5], NULL, copy_func, (void *)&u) == 0);

    for (int i = 0; i < 6; ++i) {
        assert(pthread_join(threads[i], NULL) != 0);
    }

    static char buf[200 * 2 * 4] = {0};

    FILE *fp1 = fopen(s.out_s, "r");

    assert(fp1 != NULL);

    FILE *fp2 = fopen(s.out_s, "r");

    assert(fp2 != NULL);

    size_t c_sz = fread(buf, 200 * 2 * 4, 1, fp1);

    assert(test_write_interference(buf, c_sz));

    c_sz = fread(buf, 200 * 2 * 4, 1, fp2);

    assert(test_write_interference(buf, c_sz));

    printf("Successful test.\n");

    return 0;
}
