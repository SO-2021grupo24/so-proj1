#ifndef TFS_SERVER_ERRORS_H
#define TFS_SERVER_ERRORS_H

#define E_INIT_SESSION_MUTEX                                                   \
    ("[ERR] FATAL! Failed to initialize session thread mutex")
#define E_INIT_PROD_CONS_MUTEX                                                 \
    ("[ERR] FATAL! Failed to initialize prod cons thread mutex")
#define E_LOCK_SESSION_MUTEX                                                   \
    ("[ERR] FATAL! Failed to lock session thread mutex")
#define E_UNLOCK_SESSION_MUTEX                                                 \
    ("[ERR] FATAL! Failed to lock session thread mutex")
#define E_FINI_SESSION_MUTEX                                                   \
    ("[ERR] FATAL! Failed to destroy session thread mutex")
#define E_FINI_PROD_CONS_MUTEX                                                 \
    ("[ERR] FATAL! Failed to destroy prod cons mutex")
#define E_LOCK_PROD_CONS_MUTEX ("[ERR] FATAL! Failed to lock prod cons mutex")
#define E_UNLOCK_PROD_CONS_MUTEX                                               \
    ("[ERR] FATAL! Failed to unlock prod cons mutex")
#define E_READ_PROD_CONS                                                       \
    ("[ERR] Failed to read and process prod cons data correctly")
#define E_INIT_SESSION_CONDVAR                                                 \
    ("[ERR] FATAL! Failed to initialize session thread condvar")
#define E_WAIT_SESSION_CONDVAR                                                 \
    ("[ERR] FATAL! Failed to wait on session thread condvar")
#define E_SIGNAL_SESSION_CONDVAR                                               \
    ("[ERR] FATAL! Failed to signal session thread condvar")
#define E_FINI_SESSION_CONDVAR                                                 \
    ("[ERR] FATAL! Failed to destroy session thread condvar")
#define E_INIT_SESSION_THREAD ("[ERR] FATAL! Could not create a session thread")
#define E_JOIN_SESSION_THREAD ("[ERR] FATAL! Could not join a session thread")
#define E_OPEN_REQUESTS_PIPE                                                   \
    ("[ERR] FATAL! Could not open server requests pipe")
#define E_READ_REQUESTS_PIPE                                                   \
    ("[ERR] FATAL! Could not read server requests pipe")
#define E_INVALID_REQUEST ("[ERR] Invalid request")
#define E_OPEN_CLIENT_PIPE ("[ERR] FATAL! Could not open client pipe")
#define E_SIGPIPE_CLIENT_PIPE ("[ERR] Client stopped listening (SIGPIPE)")
#define E_WRITE_CLIENT_PIPE ("[ERR] Failed to write to client pipe")
#define E_TFS_INIT ("[ERR] FATAL! Could not init TFS")
#define E_TFS_OPEN ("[ERR] Could not open file in TFS")
#define E_TFS_READ ("[ERR] Could not read file in TFS")

#define E_INIT_SESSION_TABLE_MUTEX                                             \
    ("[ERR] FATAL! Failed to initialize session table mutex")
#define E_LOCK_SESSION_TABLE_MUTEX                                             \
    ("[ERR] FATAL! Failed to lock session table mutex")
#define E_UNLOCK_SESSION_TABLE_MUTEX                                           \
    ("[ERR] FATAL! Failed to lock session table mutex")
#define E_FINI_SESSION_TABLE_MUTEX                                             \
    ("[ERR] FATAL! Failed to lock session table mutex")

#define E_UNLINK ("[ERR] FATAL! unlink(%s) failed: %s\n")
#define E_MKFIFO ("[ERR] FATAL! mkfifo failed")

#endif /*TFS_SERVER_ERRORS_H*/
