#ifndef STATE_H
#define STATE_H

#include "config.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/*
 * Directory entry
 */
typedef struct {
    char d_name[MAX_FILE_NAME];
    int d_inumber;
} dir_entry_t;

/* Files directory and previously used. */
typedef enum { T_FILE, T_DIRECTORY, T_PREV_USED } inode_type;

/*
 * I-node
 */
typedef struct {
    inode_type i_node_type;
    size_t i_size;
    int i_indirect_data_block;
    int i_direct_data_blocks[MAX_DIRECT_DATA_BLOCKS_PER_FILE];
    /* in a real FS, more fields would exist here */
} inode_t;

typedef enum { FREE = 0, TAKEN = 1 } allocation_state_t;

/*
 * Open file entry (in open file table)
 */
typedef struct {
    int of_inumber;
    size_t of_offset;
} open_file_entry_t;

extern pthread_rwlock_t open_file_entries_rw_locks[MAX_OPEN_FILES];
extern pthread_mutex_t file_allocation_lock;
extern pthread_mutex_t dir_entry_lock;
extern pthread_mutex_t open_file_table_lock;

extern pthread_rwlock_t inode_rw_locks[INODE_TABLE_SIZE];
extern pthread_mutex_t freeinode_ts_lock;

#define MAX_DIR_ENTRIES (BLOCK_SIZE / sizeof(dir_entry_t))

void state_init();
void state_destroy();

inline int blocks_allocated(inode_t *inode) {
    return (int)BLOCK_SIZEOF(inode->i_size);
}

inline int rw_total_blocks(size_t offset, size_t to_rw) {
    return (int)BLOCK_SIZEOF(offset + to_rw);
}

inline int final_block(size_t offset, size_t to_rw) {
    return rw_total_blocks(offset, to_rw) - 1;
}

inline int current_block(size_t offset) { return (int)BLOCK_CURRENT(offset); }

int init_locks();
int inode_create(inode_type n_type);
int inode_delete(int inumber);
inode_t *inode_get(int inumber);

int clear_dir_entry(int inumber, int sub_inumber);
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name);
int find_in_dir(int inumber, char const *sub_name);

int data_block_alloc();
int data_block_free(int block_number);
int data_inode_blocks_free(inode_t *inode);
void *data_block_get(int block_number);

int allocate_blocks(inode_t *inode, size_t file_offset, size_t to_write);
int get_block_number(inode_t *inode, int block_order);
int fill_block(int block_number, const void *buffer, size_t block_offset,
               size_t to_write);

int add_to_open_file_table(int inumber, size_t offset);
int remove_from_open_file_table(int fhandle);

bool is_taken_open_file_table(int fhandle);

open_file_entry_t *get_open_file_entry(int fhandle);

#endif // STATE_H
