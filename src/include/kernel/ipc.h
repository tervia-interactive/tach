/* tach - IPC Header */
/* Message primitive shared struct */

#ifndef _KERNEL_IPC_H
#define _KERNEL_IPC_H

#include "kernel/types.h"

/* Message size limits */
#define IPC_MSG_SIZE_MAX 4096
#define IPC_MSG_DATA_MAX (IPC_MSG_SIZE_MAX - sizeof(ipc_msg_header_t))

/* Message header */
typedef struct ipc_msg_header {
    uint32_t sender_pid;
    uint32_t msg_type;
    uint32_t data_size;
    uint64_t timestamp;
} ipc_msg_header_t;

/* Message structure */
typedef struct ipc_msg {
    ipc_msg_header_t header;
    uint8_t data[IPC_MSG_DATA_MAX];
} ipc_msg_t;

/* Port handle */
typedef uint32_t port_handle_t;

/* Send message to port */
int ipc_send(port_handle_t port, const ipc_msg_t* msg);

/* Receive message from port */
int ipc_recv(port_handle_t port, ipc_msg_t* msg, uint32_t timeout_ms);

/* Reply to message */
int ipc_reply(port_handle_t port, const ipc_msg_t* msg);

#endif /* _KERNEL_IPC_H */
