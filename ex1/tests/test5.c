#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    const char *str;
    const size_t sz;
} ct_string_view;

typedef struct {
    int handle;
    int index;
} handle_index;

#define THREAD_AMOUNT 6

ct_string_view views[THREAD_AMOUNT] = {
    {.str = "ABCDEFG", .sz = sizeof("ABCDEFG") - 1},
    {.str = "1234", .sz = sizeof("1234") - 1},
    {.str = "pizza", .sz = sizeof("pizza") - 1},
    {.str = "amogus", .sz = sizeof("amogus") - 1},
    {.str = "SDFef", .sz = sizeof("SDFef") - 1},
    {.str = "69aa420", .sz = sizeof("69aa420") - 1}
};

void *t_func(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;
    const char *s = views[hidx->index].str;
    const size_t sz = views[hidx->index].sz;

    int amount = 69 + rand() % 420;
    for (int i = 0; i < amount; ++i)
        assert(tfs_write(handle, s, sz) == sz);

    return NULL;
}

int test_write_interference(char *buf, size_t len) {
    char *cap = buf + len;
    /* Check if we can iterate string by string in the buffer.
     * Else, there was interference. */
    while (buf != cap) {
        const size_t cur = (size_t)(cap - buf);

        int i;
        for(i = 0; i < THREAD_AMOUNT; ++i) {
            size_t val = cur < views[i].sz ? cur : views[i].sz;
            if (!strncmp(buf, views[i].str, val)) {
                //printf("%ld %ld %ld\n", (long)val, (long)views[i].sz, (long)i);
                buf += val;
                break;
            }
        }

        if(i >= THREAD_AMOUNT) {
            return -1;
        }
    }
    return 0;
}

int main() {
    srand((unsigned int)time(NULL));
    const char *path1 = "/f1";

    /* First we test if 2 threads don't conflict with writing simulaneosly. */

    assert(tfs_init() != -1);

    /* File handle for the threads. */
    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);

    pthread_t t[THREAD_AMOUNT];

    handle_index hidx[THREAD_AMOUNT];

    for(int i = 0; i < THREAD_AMOUNT; ++i) {
        hidx[i].handle = f1;
        hidx[i].index = i;
        assert(pthread_create(&t[i], NULL, t_func, (void *)&hidx[i]) == 0);    
    }

    for(int i = 0; i < THREAD_AMOUNT; ++i)
        assert(pthread_join(t[i], NULL) == 0);

    const int len = 420 * THREAD_AMOUNT * 7;

    char buf[len];

    assert(tfs_close(f1) != -1);

    /* Abre outra. */
    f1 = tfs_open(path1, 0);

    ssize_t sz = tfs_read(f1, buf, len - 1);
    assert(sz != -1);

    /* Testar se não há interferência de escritas. */
    assert(test_write_interference(buf, (size_t)(sz - 1)) != -1);

    buf[len - 1] = '\0';
    puts(buf);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}
