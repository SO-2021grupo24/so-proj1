#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define MAX_DIRECT_DATA_BLOCKS_PER_FILE (10)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)

#define DELAY (5000)

#define UNALLOCATED_BLOCK (-1)

#define BLOCK_SIZEOF(x) (((x) + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

#define BLOCK_CURRENT(x) ((x) / BLOCK_SIZE)

#define BLOCK_OFFSET(x) ((x) % BLOCK_SIZE)

#define MAX_BLOCKS                                                             \
    (MAX_DIRECT_DATA_BLOCKS_PER_FILE + (BLOCK_SIZE / sizeof(int)))

#define MAX_FILE_SIZE (BLOCK_SIZE * MAX_BLOCKS)

#endif // CONFIG_H
