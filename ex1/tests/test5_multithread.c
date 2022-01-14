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

#define THREAD_AMOUNT 6

ct_string_view views[THREAD_AMOUNT] = {
    {.str = "ABCDEFG", .sz = sizeof("ABCDEFG") - 1},
    {.str = "1234", .sz = sizeof("1234") - 1},
    {.str = "pizza", .sz = sizeof("pizza") - 1},
    {.str = "amogus", .sz = sizeof("amogus") - 1},
    {.str = "SDFef", .sz = sizeof("SDFef") - 1},
    {.str = "69aa420", .sz = sizeof("69aa420") - 1}};

char read_log[THREAD_AMOUNT][9][9] = {0};

void *t_func_w(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;
    const char *s = views[hidx->index].str;
    const size_t sz = views[hidx->index].sz;

    int amount = 1 + rand() % 9;
    for (int i = 0; i < amount; ++i) {
        const ssize_t sz1 = tfs_write(handle, s, sz);
        assert(sz1 == sz);
    }

    return NULL;
}

void *t_func_r(void *arg) {
    const handle_index *hidx = (handle_index *)arg;

    const int handle = hidx->handle;
    const int idx = hidx->index;

    int amount = 1 + rand() % 9;
    for (int i = 0; i < amount; ++i) {
        /* Reads a string with size between 3 and 8. */
        int to_read = (3 + rand() % 6);
        tfs_read(handle, read_log[idx][i], (size_t)to_read);
    }

    return NULL;
}

int test_write_interference(char *buf, size_t len) {
    char *cap = buf + len;
    /* Check if we can iterate string by string in the buffer.
     * Else, there was interference. */
    while (buf != cap) {
        const size_t cur = (size_t)(cap - buf);

        int i;
        for (i = 0; i < THREAD_AMOUNT; ++i) {
            size_t val = cur < views[i].sz ? cur : views[i].sz;
            if (!strncmp(buf, views[i].str, val)) {
                buf += val;
                break;
            }
        }

        if (i >= THREAD_AMOUNT) {
            return -1;
        }
    }
    return 0;
}

int pattern_search(const char *s, size_t len) {
    for (size_t i = 0; i < THREAD_AMOUNT; ++i) {
        size_t sr = views[i].sz > len ? len : views[i].sz;

        size_t j;
        for (j = 0; j < sr; ++j) {
            if (s[j] != views[i].str[j])
                break;
        }

        if (j == sr)
            return (int)sr;
    }

    return -1;
}

int pattern_search_back(const char *s, size_t lim) {
    for (size_t i = 0; i < THREAD_AMOUNT; ++i) {
        const char *str = views[i].str + views[i].sz - lim;

        if (!strncmp(str, s, lim))
            return 0;
    }

    return -1;
}

int str_valid(const char *s) {
    for (int i = 0; i < THREAD_AMOUNT; ++i)
        if (strstr(views[i].str, s) != NULL)
            return 0;

    const size_t len = strlen(s);

    for (size_t i = 0; i < len; ++i) {
        const int r = pattern_search(s + i, len - i);
        if (r == -1)
            continue;

        if (r >= (len - i)) {
            if (i == 0)
                return 0;
            if (pattern_search_back(s, i) != 0)
                return -1;
            else
                return 0;
        }

        else {
            if (pattern_search(s + i + r, len - i - (size_t)r) != -1 &&
                pattern_search_back(s, i) != -1)
                return 0;
        }
    }

    return pattern_search_back(s, len);
}

int test_read_interference() {
    for (int i = 0; i < THREAD_AMOUNT; ++i) {
        for (int j = 0; j < 9; ++j) {
            if (*read_log[i][j] != '\0') {
                if (str_valid(read_log[i][j]) != 0) {
                    printf("str: %s\n", read_log[i][j]);
                    return -1;
                }
            }
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
    int f2 = tfs_open(path1, 0);
    assert(f1 != -1);

    pthread_t t[THREAD_AMOUNT * 2];

    handle_index hidx[THREAD_AMOUNT * 2];

    for (int i = 0; i < THREAD_AMOUNT; ++i) {
        hidx[i].handle = f1;
        hidx[i].index = i;
        assert(pthread_create(&t[i], NULL, t_func_w, (void *)&hidx[i]) == 0);

        hidx[THREAD_AMOUNT + i].handle = f2;
        hidx[THREAD_AMOUNT + i].index = i;
        assert(pthread_create(&t[THREAD_AMOUNT + i], NULL, t_func_r,
                              (void *)&hidx[THREAD_AMOUNT + i]) == 0);
    }

    for (int i = 0; i < THREAD_AMOUNT; ++i) {
        assert(pthread_join(t[i], NULL) == 0);
        assert(pthread_join(t[THREAD_AMOUNT + i], NULL) == 0);
    }

    const int len = 9 * THREAD_AMOUNT * 7;

    char buf[len];

    assert(tfs_close(f1) != -1);

    /* Abre outra. */
    f1 = tfs_open(path1, 0);

    ssize_t sz = tfs_read(f1, buf, len - 1);
    assert(sz != -1);

    /* Testar se não há interferência de escritas. */
    assert(test_write_interference(buf, (size_t)(sz - 1)) != -1);

    buf[len - 1] = '\0';
    // puts(buf);

    assert(test_read_interference() != -1);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}
