/* tach Operating System - Port Implementation */

#include <kernel/types.h>
#include <kernel/port.h>
#include <kernel/ipc.h>
#include <kernel/errno.h>
#include <kernel/spinlock.h>

static port_t ports[256];
static spinlock_t port_lock = SPINLOCK_INIT;
static uint32_t next_port_id = 1;

#define PORT_HANDLE_INVALID ((port_handle_t)-1)

port_handle_t port_create(uint32_t max_messages) {
    spinlock_lock(&port_lock);
    
    for (uint32_t i = 0; i < 256; i++) {
        if (ports[i].id == 0) {
            ports[i].id = next_port_id++;
            ports[i].rights = PORT_RIGHT_SEND | PORT_RIGHT_RECV;
            ports[i].max_messages = max_messages;
            ports[i].msg_count = 0;
            ports[i].queue_head = NULL;
            ports[i].queue_tail = NULL;
            ports[i].waiters = NULL;
            spinlock_unlock(&port_lock);
            return i;
        }
    }
    
    spinlock_unlock(&port_lock);
    return PORT_HANDLE_INVALID;
}

void port_destroy(port_handle_t port) {
    if (port >= 256) return;
    
    spinlock_lock(&port_lock);
    ports[port].id = 0;
    ports[port].rights = 0;
    spinlock_unlock(&port_lock);
}

int port_send(port_handle_t port, const ipc_msg_t* msg) {
    if (port >= 256 || !msg) return -EINVAL;
    
    spinlock_lock(&port_lock);
    
    if (ports[port].id == 0) {
        spinlock_unlock(&port_lock);
        return -EINVAL;
    }
    
    if (!(ports[port].rights & PORT_RIGHT_SEND)) {
        spinlock_unlock(&port_lock);
        return -EPERM;
    }
    
    if (ports[port].msg_count >= ports[port].max_messages) {
        spinlock_unlock(&port_lock);
        return -EAGAIN;
    }
    
    ports[port].msg_count++;
    
    spinlock_unlock(&port_lock);
    return 0;
}

int port_recv(port_handle_t port, ipc_msg_t* msg, uint32_t timeout_ms) {
    (void)timeout_ms;
    
    if (port >= 256 || !msg) return -EINVAL;
    
    spinlock_lock(&port_lock);
    
    if (ports[port].id == 0) {
        spinlock_unlock(&port_lock);
        return -EINVAL;
    }
    
    if (!(ports[port].rights & PORT_RIGHT_RECV)) {
        spinlock_unlock(&port_lock);
        return -EPERM;
    }
    
    if (ports[port].msg_count == 0) {
        spinlock_unlock(&port_lock);
        return -EAGAIN;
    }
    
    ports[port].msg_count--;
    
    spinlock_unlock(&port_lock);
    return 0;
}

int port_clone_rights(port_handle_t src, port_handle_t dst, uint32_t rights) {
    (void)src;
    (void)dst;
    (void)rights;
    return -ENOSYS;
}
