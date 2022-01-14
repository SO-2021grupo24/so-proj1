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

void *trunc_func(void *arg) {
    const str_idx *s = (str_idx *)arg;

    for (int i = 0; i < 2; ++i) {
        usleep(900);
        int f1 = tfs_open(s->s, TFS_O_TRUNC);

        assert(f1 != -1);

        assert(tfs_close(f1) != -1);
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

int main() {
    assert(tfs_init() != -1);

    const int lim1 = 69;
    int handles[lim1];
    pthread_t threads[lim1];
    static pair_handle_magic phm[69];

    /* Checks if we don't trespass file limit. */
    for (int i = 0; i < lim1; ++i) {
        phm[i].magic = i;
        phm[i].handle = &handles[i];
        assert(pthread_create(&threads[i], NULL, file_from_magic,
                              (void *)&phm[i]) == 0);
    }

    for (int i = 0; i < lim1; ++i)
        assert(pthread_join(threads[i], NULL) == 0);

    for (int i = 0; i < lim1; ++i) {
        /* 20 files should be the limit. */
        assert(i <= 19 || handles[i] == -1);
        // printf("n%d: %d\n", i, handles[i]);
    }

    for (int i = 0; i < MAX_OPEN_FILES; ++i)
        assert(tfs_close(i) != -1);

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

    str_idx s = {.s = paths[0], .index = 0};
    assert(pthread_create(&threads[4], NULL, trunc_func, (void *)&s) == 0);

    str_idx u = {.s = paths[1], .index = 1};
    assert(pthread_create(&threads[5], NULL, trunc_func, (void *)&u) == 0);

    for (int i = 0; i < 6; ++i)
        assert(pthread_join(threads[i], NULL) != 0);

    const int len = 200 * 2 * 4;

    static char buf[200 * 2 * 4] = {0};

    /* Abre outra. */
    int f1 = tfs_open(paths[0], 0);
    assert(f1 != -1);
    int f2 = tfs_open(paths[1], 0);
    assert(f2 != -1);

    ssize_t sz = tfs_read(f1, buf, len - 1);
    assert(sz != -1);

    sz = tfs_read(f2, buf + sz, len - 1);
    assert(sz != -1);

    assert(verify_less_operations(buf) != -1);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}
