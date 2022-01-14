#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

const char text1[] = "ABCDEFG";
const unsigned size1 = sizeof(text1) - 1;
const char text2[] = "1234";
const unsigned size2 = sizeof(text2) - 1;
const char text3[] = "pizza";
const unsigned size3 = sizeof(text3) - 1;

void *t1_func(void *arg) {
    const int handle = *(int *)arg;

    for (int i = 0; i < 200; ++i)
        assert(tfs_write(handle, text1, size1) == size1);

    return NULL;
}

void *t2_func(void *arg) {
    const int handle = *(int *)arg;

    for (int i = 0; i < 200; ++i)
        assert(tfs_write(handle, text2, size2) == size2);

    return NULL;
}

void *t3_func(void *arg) {
    (void)arg;

    const int handle = *(int *)arg;

    for (int i = 0; i < 69; ++i)
        assert(tfs_write(handle, text3, size3) == size3);

    return NULL;
}

int test_write_interference(char *buf, size_t len) {
    char *cap = buf + len;
    /* Check if we can iterate string by string in the buffer.
     * Else, there was interference. */
    while (buf != cap) {
        const size_t cur = (size_t)(cap - buf);
        size_t val;

        if (!strncmp(buf, text1, (val = cur < size1 ? cur : size1)))
            buf += val;
        else if (!strncmp(buf, text2, (val = cur < size2 ? cur : size2)))
            buf += val;
        else if (!strncmp(buf, text3, (val = cur < size3 ? cur : size3)))
            buf += val;
        else
            return -1;
    }

    return 0;
}

int main() {
    const char *path1 = "/f1";

    /* First we test if 2 threads don't conflict with writing simulaneosly. */

    assert(tfs_init() != -1);

    /* File handle for the threads. */
    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);

    pthread_t t1, t2, t3;

    assert(pthread_create(&t1, NULL, t1_func, (void *)&f1) == 0);
    assert(pthread_create(&t2, NULL, t2_func, (void *)&f1) == 0);
    assert(pthread_create(&t3, NULL, t3_func, (void *)&f1) == 0);

    assert(pthread_join(t1, NULL) == 0);
    assert(pthread_join(t2, NULL) == 0);
    assert(pthread_join(t3, NULL) == 0);

    const int len = 1000;

    char buf[len];

    assert(tfs_close(f1) != -1);

    /* Abre outra. */
    f1 = tfs_open(path1, 0);

    assert(tfs_read(f1, buf, len - 1) != -1);

    /* Testar se não há interferência de escritas. */
    assert(test_write_interference(buf, len - 1) != -1);

    buf[len - 1] = '\0';
    puts(buf);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}
