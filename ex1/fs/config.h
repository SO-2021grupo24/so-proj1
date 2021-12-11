#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_DATA_BLOCKS (10)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)

#define DELAY (5000)

#define UNALLOCATED_BLOCK (-1)

#define BLOCK_SIZEOF(x) ((x + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

#define BLOCK_CURRENT(x) (x / BLOCK_SIZE)

#define BLOCK_OFFSET(x) (x % BLOCK_SIZE)

#define MAX_FILESIZE                                                           \
    (BLOCK_SIZE * (INODE_DATA_BLOCKS + (BLOCK_SIZE / sizeof(int))))

#define TFS_PREDICT(expr, value, _probability)                                 \
    __builtin_expect_with_probability(expr, value, _probability)

#define TFS_PREDICT_TRUE(expr, probability)                                    \
    TFS_PREDICT(!!(expr), 1, probability)
#define TFS_PREDICT_FALSE(expr, probability)                                   \
    TFS_PREDICT(!!(expr), 0, probability)

#define TFS_LIKELY(expr) TFS_PREDICT_TRUE(expr, 1.0)
#define TFS_UNLIKELY(expr) TFS_PREDICT_FALSE(expr, 1.0)

#endif // CONFIG_H
