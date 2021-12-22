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

int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

static void *rw_get_block(inode_t *inode, size_t cur_block) {
    if (TFS_LIKELY(cur_block < INODE_DATA_BLOCKS))
        return data_block_get(inode->i_data_block[cur_block]);

    const int *const index_blk =
        (int *)data_block_get(inode->i_supplement_block);
    if (index_blk == NULL)
        return NULL;

    return data_block_get(index_blk[cur_block - INODE_DATA_BLOCKS]);
}

static ssize_t read_impl(size_t of_offset, inode_t *inode, void *buffer,
                         size_t to_read, size_t cur_block) {
    if (to_read == 0)
        return 0;

    void *const block = rw_get_block(inode, cur_block);

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

    void *const block = rw_get_block(inode, cur_block);

    if (block == NULL)
        return -1;

    const size_t offset = BLOCK_OFFSET(of_offset);
    const size_t len =
        (to_write + offset) < BLOCK_SIZE ? to_write : (BLOCK_SIZE - offset);
    /* Perform the actual write */
    /*printf("%p -> %p\n", buffer, block + offset);*/
    memcpy(block + offset, buffer, len);

    return write_impl(0, inode, buffer + len, to_write - len, cur_block + 1);
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to write */
    if (to_write + file->of_offset > MAX_FILESIZE) {
        to_write = MAX_FILESIZE - file->of_offset;
    }

    if (to_write > 0) {
        if (data_inode_blocks_alloc(inode, to_write) == -1)
            return -1;

        if (write_impl(file->of_offset, inode, buffer, to_write,
                       BLOCK_CURRENT(inode->i_size)) == -1)
            return -1;

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        read_impl(file->of_offset, inode, buffer, to_read, 0);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_read;
    }

    return (ssize_t)to_read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    int f = tfs_open(source_path,
                     0); // nao ha flag TFS_O_CREATE suponho que seja por o 0?
    FILE *fd =
        fopen(dest_path,
              "w+"); // abrir em write mode com o mais para criar se nao existir

    char buffer[BLOCK_SIZE]; // nao sei se este e o tamanho maximo do ficheiro,
                             // maybe? pensei fazer com loop? talvez nao? assim
                             // se calhar e a maneira certa?
    int bytes_read = 0;
    int bytes_written = 0;

    bytes_read =
        (int)tfs_read(f, buffer, sizeof(buffer) - 1); // typecast para int??????

    if (bytes_read == -1) {
        return -1;
    }

    buffer[bytes_read] = '\0';

    bytes_written =
        (int)fwrite(buffer, 1, strlen(buffer), fd); // typecast para int????????

    if (bytes_written == -1) {
        return -1;
    }

    return 0;
}
