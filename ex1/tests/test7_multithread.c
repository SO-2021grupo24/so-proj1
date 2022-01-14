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
    const char *out_s;
    const char *path;
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

int fhandles[4];
const char paths[2][7] = {"/file1", "/file2"};

void *t_func_w(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;
    const char *s = views[hidx->index].str;
    const size_t sz = views[hidx->index].sz;

    int amount = 100;
    for (int i = 0; i < amount; ++i) {
        const ssize_t sz1 = tfs_write(handle, s, sz);
        assert(sz1 == sz);
        assert(tfs_copy_to_external_fs(hidx->path, hidx->out_s) != -1);
    }

    return NULL;
}

void *t_func_r(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;

    int amount = 33;
    for (int i = 0; i < amount; ++i) {
        char str_cur[5];
        tfs_read(handle, str_cur, 4);
    }

    return NULL;
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
            strncmp(buf, views[2].str, 4) != 0)
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
    fhandles[1] = tfs_open(paths[1], TFS_O_CREAT);
    assert(fhandles[1] != -1);
    fhandles[2] = tfs_open(paths[0], 0);
    assert(fhandles[0] != -1);
    fhandles[3] = tfs_open(paths[1], 0);
    assert(fhandles[1] != -1);

    handle_index hidx[THREAD_AMOUNT_RW];

    hidx[0].handle = fhandles[0];
    hidx[0].index = 0;
    hidx[0].out_s = "f1";
    hidx[0].path = paths[0];
    assert(pthread_create(&threads[0], NULL, t_func_w, (void *)&hidx[0]) == 0);

    hidx[1].handle = fhandles[2];
    hidx[1].index = 1;
    assert(pthread_create(&threads[1], NULL, t_func_r, (void *)&hidx[1]) == 0);

    hidx[2].handle = fhandles[1];
    hidx[2].index = 2;
    hidx[2].out_s = "f2";
    hidx[2].path = paths[1];
    assert(pthread_create(&threads[2], NULL, t_func_w, (void *)&hidx[2]) == 0);

    hidx[3].handle = fhandles[3];
    hidx[3].index = 3;
    assert(pthread_create(&threads[3], NULL, t_func_r, (void *)&hidx[3]) == 0);

    for (int i = 0; i < 4; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    static char buf[200 * 2 * 4] = {0};

    FILE *fp1 = fopen("f1", "r");

    assert(fp1 != NULL);

    FILE *fp2 = fopen("f2", "r");

    assert(fp2 != NULL);

    fread(buf, 200 * 2 * 4, 1, fp1);

    assert(test_write_interference(buf, strlen(buf)) == 0);

    memset(buf, 0, 200 * 2 * 4);
    fread(buf, 200 * 2 * 4, 1, fp2);

    assert(test_write_interference(buf, strlen(buf)) == 0);

    printf("Successful test.\n");

    fclose(fp1);
    fclose(fp2);

    remove("f1");
    remove("f2");
    return 0;
}
