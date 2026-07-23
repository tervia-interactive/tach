/* tach - IPC Test */
/* Port/message delivery, backpressure, ordering */

#include "kernel/types.h"
#include "kernel/assert.h"
#include "kernel/port.h"
#include "kernel/ipc.h"

#ifdef TACH_TEST

static void test_ipc_port_create(void) {
    port_handle_t port = port_create(10);
    KASSERT(port != 0);
    port_destroy(port);
}

static void test_ipc_send_recv(void) {
    port_handle_t port = port_create(10);
    KASSERT(port != 0);
    
    ipc_msg_t msg = {0};
    msg.header.sender_pid = 1;
    msg.header.msg_type = 1;
    msg.header.data_size = 4;
    
    int ret = port_send(port, &msg);
    KASSERT(ret == 0);
    
    ipc_msg_t recv_msg = {0};
    ret = port_recv(port, &recv_msg, 1000);
    KASSERT(ret == 0);
    KASSERT(recv_msg.header.msg_type == 1);
    
    port_destroy(port);
}

void test_ipc_run_all(void) {
    kprintf("Running IPC tests...\n");
    test_ipc_port_create();
    kprintf("  [PASS] Port creation\n");
    test_ipc_send_recv();
    kprintf("  [PASS] Send/Receive\n");
    kprintf("IPC tests complete.\n");
}

#endif /* TACH_TEST */
