#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tfs_init() {
    state_init();

    /* Initialize inode locks. */
    if (inode_init_locks() == -1)
        return -1;

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    if (open_file_init_locks() == -1)
        return -1;

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
        pthread_rwlock_wrlock(&inode_rw_locks[inum]);

        inode_t *inode = inode_get(inum);
        /* Null inode / deleted meanwhile ---> not successful open. */
        if (inode == NULL || inode->i_node_type == T_PREV_USED) {
            pthread_rwlock_unlock(&inode_rw_locks[inum]);
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (data_inode_blocks_free(inode) == -1) {
                    pthread_rwlock_unlock(&inode_rw_locks[inum]);
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

        pthread_rwlock_unlock(&inode_rw_locks[inum]);
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
    pthread_rwlock_wrlock(&open_file_entries_rw_locks[fhandle]);
    const int rc = remove_from_open_file_table(fhandle);
    pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);

    return rc;
}

static ssize_t read_impl(size_t of_offset, inode_t *inode, void *buffer,
                         size_t to_read) {
    if (to_read == 0) {
        return 0;
    }

    int starting_block_to_read = current_block(of_offset);
    int last_block_to_read = final_block(of_offset, to_read);
    size_t block_offset = BLOCK_OFFSET(of_offset);

    // reads the first
    int block_number = get_block_number(inode, starting_block_to_read);
    if (block_number == -1) {
        return -1;
    }
    void *real_block = data_block_get(block_number);
    if (real_block == NULL) {
        return -1;
    }

    if (starting_block_to_read == last_block_to_read) {
        memcpy(buffer, real_block + block_offset, to_read);
        return 0;
    }

    memcpy(buffer, real_block + block_offset, BLOCK_SIZE - block_offset);

    // reads medium blocks
    size_t buffer_offset = BLOCK_SIZE - block_offset;
    for (int block = starting_block_to_read + 1; block < last_block_to_read;
         block++) {
        block_number = get_block_number(inode, block);
        if (block_number == -1) {
            return -1;
        }

        real_block = data_block_get(block_number);
        if (real_block == NULL) {
            return -1;
        }
        memcpy(buffer + buffer_offset, real_block, BLOCK_SIZE);
        buffer_offset += BLOCK_SIZE;
    }

    // reads last block
    block_number = get_block_number(inode, last_block_to_read);
    if (block_number == -1) {
        return -1;
    }

    real_block = data_block_get(block_number);
    if (real_block == NULL) {
        return -1;
    }

    size_t last_read = to_read - buffer_offset;
    memcpy(buffer + buffer_offset, real_block, last_read);

    return 0;
}

static ssize_t write_impl(size_t of_offset, inode_t *inode, void const *buffer,
                          size_t to_write) {
    if (to_write == 0)
        return 0;

    int last_block_to_write = final_block(of_offset, to_write);
    int starting_block = current_block(of_offset);
    size_t block_offset = BLOCK_OFFSET(of_offset);
    ssize_t rc = 0;

    // makes the memory necessary to make the writing possible
    const int prev = last_block_to_write;
    if ((last_block_to_write = allocate_blocks(inode, of_offset, to_write)) !=
        prev) {
        /* We'll still write until where we can, but we fail anyway. */
        to_write =
            (size_t)(last_block_to_write - starting_block + 1) * BLOCK_SIZE;
        rc = -1;
    }

    // writes in the first block
    int block_number = get_block_number(inode, starting_block);
    if (block_number == -1) {
        return -1;
    }

    if (starting_block == last_block_to_write) {
        // changes the meta data of the file
        if (rc == -1 ||
            fill_block(block_number, buffer, block_offset, to_write) == -1) {
            return -1;
        }

        return 0;
    }

    if (fill_block(block_number, buffer, block_offset,
                   BLOCK_SIZE - block_offset) == -1) {
        return -1;
    }

    size_t buffer_offset = BLOCK_SIZE - block_offset;

    // writes mid blocks
    for (int block = starting_block + 1; block < last_block_to_write; block++) {
        block_number = get_block_number(inode, block);
        if (block_number == -1) {
            return -1;
        }
        if (fill_block(block_number, buffer + buffer_offset, 0, BLOCK_SIZE)) {
            return -1;
        }
        buffer_offset += BLOCK_SIZE;
    }

    // writes last block
    block_number = get_block_number(inode, last_block_to_write);
    if (block_number == -1) {
        return -1;
    }

    const size_t total_write = to_write - buffer_offset;
    if (fill_block(block_number, buffer + buffer_offset, 0, total_write)) {
        return -1;
    }

    return rc;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL || !is_taken_open_file_table(fhandle)) {
        return -1;
    }

    pthread_rwlock_wrlock(&open_file_entries_rw_locks[fhandle]);

    /* In the meantime, tfs_close might have been executed, so we double check.
     */
    ssize_t rc = -1;
    if (is_taken_open_file_table(fhandle)) {
        const int of_inumber = file->of_inumber;

        pthread_rwlock_wrlock(&inode_rw_locks[of_inumber]);

        /* From the open file table entry, we get the inode */
        inode_t *inode = inode_get(of_inumber);

        if (inode == NULL) {
            pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
            pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
            return -1;
        }

        /* Determine how many bytes to write */
        if (to_write + file->of_offset > MAX_FILE_SIZE) {
            to_write = MAX_FILE_SIZE - file->of_offset;
        }

        if (to_write > 0) {
            if (write_impl(file->of_offset, inode, buffer, to_write) == -1) {
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

        pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
        rc = (ssize_t)to_write;
    }

    pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
    return rc;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL || !is_taken_open_file_table(fhandle)) {
        return -1;
    }

    pthread_rwlock_rdlock(&open_file_entries_rw_locks[fhandle]);

    /* In the meantime, tfs_close might have been executed, so we double check.
     */
    ssize_t rc = -1;
    if (is_taken_open_file_table(fhandle)) {
        const int of_inumber = file->of_inumber;

        /* From the open file table entry, we get the inode */
        pthread_rwlock_rdlock(&inode_rw_locks[of_inumber]);

        inode_t *inode = inode_get(of_inumber);
        if (inode == NULL) {
            pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
            pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
            return -1;
        }

        /* Determine how many bytes to read */
        size_t to_read = inode->i_size - file->of_offset;
        if (to_read > len) {
            to_read = len;
        }

        if (to_read > 0) {
            if (read_impl(file->of_offset, inode, buffer, to_read) == -1) {
                pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
                pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
                return -1;
            }

            /* The offset associated with the file handle is
             * incremented accordingly */
            file->of_offset += to_read;
        }

        pthread_rwlock_unlock(&inode_rw_locks[of_inumber]);
        rc = (ssize_t)to_read;
    }

    pthread_rwlock_unlock(&open_file_entries_rw_locks[fhandle]);
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
