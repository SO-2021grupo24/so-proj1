#include "state.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
static inode_t inode_table[INODE_TABLE_SIZE];
static char freeinode_ts[INODE_TABLE_SIZE];

/* Data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];

/* Volatile FS state */

static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];

pthread_rwlock_t open_file_entries_rw_locks[MAX_OPEN_FILES];
pthread_mutex_t file_allocation_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dir_entry_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t open_file_table_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_rwlock_t inode_rw_locks[INODE_TABLE_SIZE];
pthread_mutex_t freeinode_ts_lock = PTHREAD_MUTEX_INITIALIZER;

const pthread_rwlock_t g_rw_init = PTHREAD_RWLOCK_INITIALIZER;

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    if (pthread_mutex_init(&file_allocation_lock, NULL) != 0)
        exit(1);

    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }
}

void state_destroy() {}

static inline void initializes_file_data_blocks(inode_t *inode) {
    // puts directs blocks to UNALLOCATED_BLOCK (start at 0)
    // will slide UNALLOCATED_BLOCK through them to detect when we need to
    // allocate
    inode->i_direct_data_blocks[0] = UNALLOCATED_BLOCK;

    // puts indirect block to UNALLOCATED_BLOCK
    inode->i_indirect_data_block = UNALLOCATED_BLOCK;
}

int inode_init_locks() {
    for (int i = 0; i < INODE_TABLE_SIZE; ++i) {
        inode_rw_locks[i] = g_rw_init;
        if (pthread_rwlock_init(&inode_rw_locks[i], NULL) != 0) {
            return -1;
        }

        if (pthread_mutex_init(&freeinode_ts_lock, NULL) != 0) {
            return -1;
        }
    }

    return 0;
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int)sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

        /* Modifying inode. */
        pthread_rwlock_wrlock(&inode_rw_locks[inumber]);
        
        /* Using the bytemap resource. */
        pthread_mutex_lock(&freeinode_ts_lock);

        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;

            insert_delay(); // simulate storage access delay (to i-node)
            
            inode_table[inumber].i_node_type = n_type;
            inode_table[inumber].i_indirect_data_block = UNALLOCATED_BLOCK;

            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int b = data_block_alloc();
                if (b == -1) {
                    freeinode_ts[inumber] = FREE;

                    pthread_mutex_unlock(&freeinode_ts_lock);
                    pthread_rwlock_unlock(&inode_rw_locks[inumber]);
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
                inode_table[inumber].i_direct_data_blocks[0] = b;

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {
                    freeinode_ts[inumber] = FREE;

                    pthread_mutex_unlock(&freeinode_ts_lock);
                    pthread_rwlock_unlock(&inode_rw_locks[inumber]);
                    return -1;
                }
               
                pthread_mutex_unlock(&freeinode_ts_lock);
                pthread_rwlock_unlock(&inode_rw_locks[inumber]);

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                /* In case of a new file, simply sets its size to 0 */
                pthread_mutex_unlock(&freeinode_ts_lock);
                
                inode_table[inumber].i_size = 0;
                initializes_file_data_blocks(&inode_table[inumber]);
                
                pthread_rwlock_unlock(&inode_rw_locks[inumber]);
            }

            return inumber;
        }
        else pthread_mutex_unlock(&freeinode_ts_lock);
    }
    return -1;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();

    if(!valid_inumber(inumber))
        return -1;

    /* Inode operation, aquire imediately. */
    pthread_rwlock_wrlock(&inode_rw_locks[inumber]);
    
    pthread_mutex_lock(&freeinode_ts_lock);

    if (freeinode_ts[inumber] == FREE) {
        return -1;
    }

    freeinode_ts[inumber] = FREE;

    inode_t *const inode = &inode_table[inumber];
    if (inode->i_size > 0) {
        if (data_inode_blocks_free(inode) == -1) {
            pthread_mutex_unlock(&freeinode_ts_lock);
            pthread_rwlock_unlock(&inode_rw_locks[inumber]);
            return -1;
        }
    }

    /* In case it was previously being used and we want to check if
     * another thread deleted the inode. */
    inode->i_node_type = T_PREV_USED;
    
    initializes_file_data_blocks(inode);
    pthread_mutex_unlock(&freeinode_ts_lock);
    pthread_rwlock_unlock(&inode_rw_locks[inumber]);

    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    return &inode_table[inumber];
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber
    pthread_rwlock_rdlock(&inode_rw_locks[inumber]);

    const inode_type cur_type = inode_table[inumber].i_node_type;

    if (cur_type != T_DIRECTORY) {
        pthread_rwlock_unlock(&inode_rw_locks[inumber]);
        return -1;
    }

    if (strlen(sub_name) == 0) {
        pthread_rwlock_unlock(&inode_rw_locks[inumber]);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(
        inode_table[inumber].i_direct_data_blocks[0]);
    
    pthread_rwlock_unlock(&inode_rw_locks[inumber]);

    if (dir_entry == NULL) {
        return -1;
    }

    pthread_mutex_lock(&dir_entry_lock);

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_entry[i].d_inumber == -1) {
            dir_entry[i].d_inumber = sub_inumber;
            /*printf("name @ %p\n", dir_entry[i].d_name);*/
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;
            pthread_mutex_unlock(&dir_entry_lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&dir_entry_lock);
    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber

    if(!valid_inumber(inumber))
        return -1;

    pthread_rwlock_rdlock(&inode_rw_locks[inumber]);
    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(
        inode_table[inumber].i_direct_data_blocks[0]);
    
    pthread_rwlock_unlock(&inode_rw_locks[inumber]);

    if (dir_entry == NULL) {
        return -1;
    }

    pthread_mutex_lock(&dir_entry_lock);

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            pthread_mutex_unlock(&dir_entry_lock);
            return dir_entry[i].d_inumber;
        }
    }

    pthread_mutex_unlock(&dir_entry_lock);
    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {
    pthread_mutex_lock(&file_allocation_lock);

    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int)sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
            pthread_mutex_unlock(&file_allocation_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&file_allocation_lock);
    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    pthread_mutex_lock(&file_allocation_lock);

    if (!valid_block_number(block_number)) {
        pthread_mutex_unlock(&file_allocation_lock);
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
    free_blocks[block_number] = FREE;

    pthread_mutex_unlock(&file_allocation_lock);

    return 0;
}

/* Frees all data blocks from an inode
 * Input
 * 	- pointer to an inode
 * Returns: 0 if success, -1 otherwise
 */
int data_inode_blocks_free(inode_t *inode) {
    const int last_block_allocated = blocks_allocated(inode) - 1;

    int rc = 0;
    for (int block = last_block_allocated; block >= 0; block--) {
        int block_number = get_block_number(inode, block);
        if (block_number == -1) {
            rc = -1;
        }

        if (data_block_free(block_number) == -1) {
            rc = -1;
        }

        if (block == MAX_DIRECT_DATA_BLOCKS_PER_FILE) {
            if (data_block_free(inode->i_indirect_data_block) == -1) {
                rc = -1;
            }
        }
    }

    return rc;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    return &fs_data[block_number * BLOCK_SIZE];
}

static int allocate_per_block(int *block) {
    int block_number = data_block_alloc();
    if (block_number == -1) {
        return -1;
    }

    *block = block_number;
    return block_number;
}

static int allocate_per_block_more(int indirect_block, int idx) {
    int *indirect_data_block = (int *)data_block_get(indirect_block);
    if (indirect_data_block == NULL) {
        return -1;
    }

    return allocate_per_block(&indirect_data_block[idx]);
}

static int allocate_blocks_impl(inode_t *inode, int starting_block,
                                int last_block) {
    for (int block = starting_block; block <= last_block; block++) {
        if (block < MAX_DIRECT_DATA_BLOCKS_PER_FILE) {
            if (allocate_per_block(&inode->i_direct_data_blocks[block]) == -1) {
                return block - 1;
            }
        }

        else if (block == MAX_DIRECT_DATA_BLOCKS_PER_FILE) {
            int block_number;
            if ((block_number =
                     allocate_per_block(&inode->i_indirect_data_block)) == -1) {
                return block - 1;
            }

            if (allocate_per_block_more(block_number, 0) == -1)
                return block - 1;
        }

        else if (allocate_per_block_more(
                     inode->i_indirect_data_block,
                     block - MAX_DIRECT_DATA_BLOCKS_PER_FILE) == -1)
            return block - 1;
    }

    return last_block;
}

int allocate_blocks(inode_t *inode, size_t file_offset, size_t to_write) {
    const int block = final_block(file_offset, to_write);
    const int total = rw_total_blocks(file_offset, to_write);
    if (blocks_allocated(inode) < total) {
        return allocate_blocks_impl(inode, blocks_allocated(inode), block);
    }

    return total - 1;
}

int get_block_number(inode_t *inode, int block_order) {
    if (block_order >= MAX_BLOCKS) {
        return -1;
    }

    if (block_order < MAX_DIRECT_DATA_BLOCKS_PER_FILE) {
        return inode->i_direct_data_blocks[block_order];
    }

    const int *const indirect_data_block =
        (int *)data_block_get(inode->i_indirect_data_block);

    if (indirect_data_block == NULL) {
        return -1;
    }

    return indirect_data_block[block_order - MAX_DIRECT_DATA_BLOCKS_PER_FILE];
}

int fill_block(int block_number, const void *buffer, size_t block_offset,
               size_t to_write) {
    void *block = data_block_get(block_number);

    if (block == NULL) {
        return -1;
    }

    memcpy(block + block_offset, buffer, to_write);

    return 0;
}

int open_file_init_locks() {
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        open_file_entries_rw_locks[i] = g_rw_init;
        if (pthread_rwlock_init(&open_file_entries_rw_locks[i], NULL) != 0) {
            return -1;
        }
    }

    if (pthread_mutex_init(&dir_entry_lock, NULL) != 0)
        return -1;

    return 0;
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    pthread_mutex_lock(&open_file_table_lock);

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            free_open_file_entries[i] = TAKEN;
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;
            pthread_mutex_unlock(&open_file_table_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&open_file_table_lock);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    if(!valid_file_handle(fhandle))
        return -1;

    pthread_mutex_lock(&open_file_table_lock);

    if (free_open_file_entries[fhandle] != TAKEN) {
        pthread_mutex_unlock(&open_file_table_lock);
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;
    
    pthread_mutex_unlock(&open_file_table_lock);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    return &open_file_table[fhandle];
}

bool is_taken_open_file_table(int fhandle) {
    pthread_mutex_lock(&open_file_table_lock);
    const bool res = free_open_file_entries[fhandle] == TAKEN;
    pthread_mutex_unlock(&open_file_table_lock);

    return res;
}
