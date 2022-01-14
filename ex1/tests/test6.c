#include "fs/operations.h"
#include <assert.h>

int fhandles_for_the_tests[3];

typedef struct {
    int *handle;
    int magic;
} pair_handle_magic;

void *t1_func(void *arg) {
    const int handle = *(int *)arg;

    const char text[] = "ABCDEFG";
    const int sz = sizeof(text) - 1;
    assert(tfs_write(handle, text, sz) == sz);
    assert(tfs_write(handle, text, sz) == sz);

    char* filename = "/spaghettiiiiiiii";
    int fhandle = tfs_open(filename, TFS_O_CREAT);
    
    fhandles_for_the_tests[0] = fhandle;

    return NULL;
}

void *t2_func(void *arg) {
    const int handle = *(int *)arg;
    
    char* filename = "/spaghettiiiiiiii";
    int fhandle = tfs_open(filename, TFS_O_CREAT);

    fhandles_for_the_tests[1] = fhandle;


    (void)handle; return NULL;
}

void *t3_func(void *arg) {
    (void)arg;

    char* filename = "/spaghettiiiiiiii";
    int fhandle = tfs_open(filename, TFS_O_CREAT);

    fhandles_for_the_tests[2] = fhandle;



    return NULL;
}

void *file_from_magic(void *arg) {
   pair_handle_magic *m = (pair_handle_magic *)arg;

   char base[] = "/g1";

   base[2] += (char)m->magic;
   *m->handle = tfs_open(base, TFS_O_CREAT);

   return NULL;
}

int main() {
    const char *path1 = "/f1";

    /* */

    assert(tfs_init() != -1);
    
    const int lim1 = 69;
    int handles[lim1];
    pthread_t threads[lim1];
    static pair_handle_magic phm[69];

    for(int i = 0; i < lim1; ++i) {
        phm[i].magic = i;
        phm[i].handle = &handles[i];
        assert(pthread_create(&threads[i], NULL, file_from_magic, (void *)&phm[i]) == 0);
    }

    for(int i = 0; i < lim1; ++i)
        assert(pthread_join(threads[i], NULL) == 0);
    
    for(int i = 0; i < lim1; ++i) {
        /* 20 files should be the limit. */
        assert(i <= 19 || handles[i] == -1);
        //printf("n%d: %d\n", i, handles[i]);
    }
    return 0;

    /* File handle for the threads. */
    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);

    pthread_t t1, t2, t3;

    assert(pthread_create(&t1, NULL, t1_func, (void *)&f1) == 0);

    assert(pthread_create(&t2, NULL, t2_func, (void *)&f1) == 0);

    assert(pthread_create(&t3, NULL, t3_func, NULL) == 0);

    assert(pthread_join(t1, NULL) == 0);
    assert(pthread_join(t2, NULL) == 0);
    assert(pthread_join(t3, NULL) == 0);

    assert(tfs_close(f1) != -1);

    printf("Successful test.\n");

    return 0;
}


