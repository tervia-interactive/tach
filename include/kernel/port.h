/* tach - Port Header */
/* Kernel ports (lightweight message queues, XPC-style) */

#ifndef _KERNEL_PORT_H
#define _KERNEL_PORT_H

#include "kernel/types.h"
#include "kernel/ipc.h"

/* Port rights */
#define PORT_RIGHT_SEND     0x01
#define PORT_RIGHT_RECV     0x02
#define PORT_RIGHT_SEND_ONCE 0x04

/* Port structure */
typedef struct port {
    uint32_t id;
    uint32_t rights;
    uint32_t max_messages;
    uint32_t msg_count;
    void* queue_head;
    void* queue_tail;
    void* waiters;
} port_t;

/* Create port */
port_handle_t port_create(uint32_t max_messages);

/* Destroy port */
void port_destroy(port_handle_t port);

/* Send message */
int port_send(port_handle_t port, const ipc_msg_t* msg);

/* Receive message */
int port_recv(port_handle_t port, ipc_msg_t* msg, uint32_t timeout_ms);

/* Clone port rights */
int port_clone_rights(port_handle_t src, port_handle_t dst, uint32_t rights);

#endif /* _KERNEL_PORT_H */
