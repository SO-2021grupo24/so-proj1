#ifndef TFS_SERVER_H
#define TFS_SERVER_H

void do_unmount(int session_id, bool inform);
void server_mount_state(size_t session_id);
void server_unmount_state(size_t session_id);
void server_open_state(size_t session_id);
void server_close_state(size_t session_id);
void server_read_state(size_t session_id);
void server_write_state(size_t session_id);
void server_shutdown_after_all_closed_state(size_t session_id);

#endif /*TFS_SERVER_H*/
