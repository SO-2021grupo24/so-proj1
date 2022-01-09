#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (pthread_rwlock_init(&open_file_entries_rw_locks[i], NULL) != 0)
            return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (data_inode_blocks_free(inode) == -1) {
                    return -1;
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }

        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }

        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) {
    /* Since writes are individual, we use it to close as well. */
    if (pthread_rwlock_wrlock(&open_file_entries_rw_locks[fhandle]))
        return -1;
    const int rc = remove_from_open_file_table(fhandle);
    if (pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]))
        return -1;

    return rc;
}

static inline void *r_get_block(inode_t *inode, size_t cur_block) {
    const int idx = *data_block_get_current_index_ptr(inode, cur_block);
    /* This should leave... */
    if (idx == -1)
        return NULL;

    return data_block_get(idx);
}

static inline void *w_get_block(inode_t *inode, size_t cur_block) {
    if (cur_block == INODE_DATA_BLOCKS &&
        inode->i_supplement_block == UNALLOCATED_BLOCK) {
        const int new_block = data_block_alloc();
        if (new_block == -1)
            return NULL;

        inode->i_supplement_block = new_block;
        *(int *)data_block_get(new_block) = -1;
    }

    int *const idx = data_block_get_current_index_ptr(inode, cur_block);

    if (*idx == -1) {
        const int new_block = data_block_alloc();

        if (new_block == -1)
            return NULL;

        *idx = new_block;
        /* Sliding the mark. */
        if (cur_block != INODE_DATA_BLOCKS - 1 && cur_block != MAX_BLOCKS - 1)
            *(idx + 1) = -1;
    }

    return data_block_get(*idx);
}

static ssize_t read_impl(size_t of_offset, inode_t *inode, void *buffer,
                         size_t to_read, size_t cur_block) {
    if (to_read == 0)
        return 0;

    void *const block = r_get_block(inode, cur_block);

    if (block == NULL)
        return -1;

    const size_t offset = BLOCK_OFFSET(of_offset);
    const size_t len =
        (to_read + offset) < BLOCK_SIZE ? to_read : (BLOCK_SIZE - offset);
    /* Perform the actual read */
    memcpy(buffer, block + offset, len);

    return read_impl(of_offset + len, inode, buffer + len, to_read - len,
                     cur_block + 1);
}

static ssize_t write_impl(size_t of_offset, inode_t *inode, void const *buffer,
                          size_t to_write, size_t cur_block) {
    if (to_write == 0)
        return 0;

    void *const block = w_get_block(inode, cur_block);

    if (block == NULL)
        return -1;

    const size_t offset = BLOCK_OFFSET(of_offset);
    const size_t len =
        (to_write + offset) < BLOCK_SIZE ? to_write : (BLOCK_SIZE - offset);
    /* Perform the actual write */
    /*printf("%p -> %p\n", buffer, block + offset);*/
    memcpy(block + offset, buffer, len);

    return write_impl(of_offset + len, inode, buffer + len, to_write - len,
                      cur_block + 1);
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL || !is_taken_open_file_table(fhandle)) {
        return -1;
    }

    if (pthread_rwlock_wrlock(&open_file_entries_rw_locks[fhandle]) != 0)
        return -1;

    /* In the meantime, tfs_close might have been executed, so we double check.
     */
    ssize_t rc = -1;
    if (is_taken_open_file_table(fhandle)) {
        const int of_inumber = file->of_inumber;

        /* From the open file table entry, we get the inode */
        inode_t *inode = inode_get(of_inumber);

        if (inode == NULL) {
            pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
            return -1;
        }

        if (pthread_rwlock_wrlock(&inode_rw_locks[of_inumber]) != 0)
            return -1;
        /* Determine how many bytes to write */
        if (to_write + file->of_offset > MAX_FILE_SIZE) {
            to_write = MAX_FILE_SIZE - file->of_offset;
        }

        if (to_write > 0) {
            if (write_impl(file->of_offset, inode, buffer, to_write,
                           BLOCK_CURRENT(file->of_offset)) == -1) {
                pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
                pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
                return -1;
            }

            /* The offset associated with the file handle is
             * incremented accordingly */
            file->of_offset += to_write;
            if (file->of_offset > inode->i_size) {
                inode->i_size = file->of_offset;
            }
        }

        if (pthread_rwlock_unlock(&inode_rw_locks[of_inumber]) != 0)
            return -1;
        rc = (ssize_t)to_write;
    }

    if (pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]) != 0)
        return -1;
    return rc;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL || !is_taken_open_file_table(fhandle)) {
        return -1;
    }

    if (pthread_rwlock_rdlock(&open_file_entries_rw_locks[fhandle]) != 0)
        return -1;

    /* In the meantime, tfs_close might have been executed, so we double check.
     */
    ssize_t rc = -1;
    if (is_taken_open_file_table(fhandle)) {
        const int of_inumber = file->of_inumber;

        /* From the open file table entry, we get the inode */
        inode_t *inode = inode_get(of_inumber);
        if (inode == NULL) {
            pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
            return -1;
        }

        if (pthread_rwlock_rdlock(&inode_rw_locks[of_inumber]) != 0)
            return -1;
        /* Determine how many bytes to read */
        size_t to_read = inode->i_size - file->of_offset;
        if (to_read > len) {
            to_read = len;
        }

        if (to_read > 0) {
            if (read_impl(file->of_offset, inode, buffer, to_read,
                          BLOCK_CURRENT(file->of_offset)) == -1) {
                pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
                pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
                return -1;
            }

            /* The offset associated with the file handle is
             * incremented accordingly */
            file->of_offset += to_read;
        }

        if (pthread_rwlock_unlock(&inode_rw_locks[of_inumber]) != 0)
            return -1;
        rc = (ssize_t)to_read;
    }

    if (pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]) != 0)
        return -1;
    return rc;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    const int f = tfs_open(source_path, 0);
    if (f == -1)
        return -1;

    /* Create file if not present, replace otherwise. */
    FILE *fd = fopen(dest_path, "w");

    if (fd == NULL)
        return -1;

    // Use buffer in .bss with maximum filesize.
    static char buffer[MAX_FILE_SIZE];

    const ssize_t bytes_read = tfs_read(f, buffer, sizeof(buffer) - 1);

    if (bytes_read == -1) {
        return -1;
    }

    const size_t bytes_written = fwrite(buffer, 1, (size_t)bytes_read, fd);

    if (bytes_written != bytes_read) {
        return -1;
    }

    return fclose(fd);
}
