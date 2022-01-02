#include "fs/operations.h"
#include <assert.h>

void *t1_func(void *arg) {
	const int handle = *(int *)arg;

	const char * const text1 = "ABCDEFG";
	assert(tfs_write(handle, text1, sizeof(text1)) == sizeof(text1));
	assert(tfs_write(handle, text1, sizeof(text1)) == sizeof(text1));

	return NULL;
}

void *t2_func(void *arg) {
	const int handle = *(int *)arg;
	
	(void)handle; return NULL;
}

void *t3_func(void *arg) {
	(void)arg;



	return NULL;
}

int main() {
    const char *path1 = "/f1";

    /* */

    assert(tfs_init() != -1);

    /* File handle for the threads. */
    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);

    pthread_t t1, t2, t3;

    assert(pthread_create(&t1, NULL, t1_func, (void *)&f1) != 0);
    assert(pthread_create(&t2, NULL, t2_func, (void *)&f1) != 0);
    assert(pthread_create(&t3, NULL, t3_func, NULL) != 0);

    assert(pthread_join(t1, NULL) != 0);
    assert(pthread_join(t2, NULL) != 0);
    assert(pthread_join(t3, NULL) != 0);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}
