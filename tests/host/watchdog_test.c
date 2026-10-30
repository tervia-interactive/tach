/* tach - Host test: timer wheel and supervised-process watchdog */
#include <kernel/types.h>
#include <kernel/timer.h>
#include <kernel/watchdog.h>
#include <kernel/signal.h>
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <hal/time.h>

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while (0)

/* ---- fakes for the pieces the real kernel provides elsewhere ---- */
static uint64_t g_ticks;
uint64_t hal_timer_get_ticks(void) { return g_ticks; }

static pid_t g_killed_pid;
static int g_killed_signal;
static int g_kill_calls;
int signal_send(pid_t pid, int sig) {
    g_killed_pid = pid;
    g_killed_signal = sig;
    g_kill_calls++;
    return 0;
}

void klog_info(const char* component, const char* fmt, ...) { (void)component; (void)fmt; }
void klog_warn(const char* component, const char* fmt, ...) { (void)component; (void)fmt; }
void klog_err(const char* component, const char* fmt, ...) { (void)component; (void)fmt; }

static void advance(unsigned count) {
    while (count--) {
        g_ticks++;
        timer_tick();
    }
}

/* ---- timer wheel ---- */
static int g_fired;
static int g_order[8];
static int g_order_len;

static void on_fire(void* arg) {
    (void)arg;
    g_fired++;
}

static void on_order(void* arg) {
    if (g_order_len < 8) g_order[g_order_len++] = (int)(uintptr_t)arg;
}

static timer_t g_periodic;
static void on_periodic(void* arg) {
    g_fired++;
    timer_set(&g_periodic, 2, on_periodic, arg);
}

static int test_timer(void) {
    timer_subsystem_init();

    timer_t one;
    timer_init(&one);
    CHECK(!timer_is_active(&one));
    timer_set(&one, 3, on_fire, NULL);
    CHECK(timer_is_active(&one));
    advance(2);
    CHECK(g_fired == 0 && timer_is_active(&one));
    advance(1);
    CHECK(g_fired == 1 && !timer_is_active(&one));
    advance(10);
    CHECK(g_fired == 1);

    /* Earlier deadline first; equal deadlines fire in arming order. */
    timer_t a, b, c;
    timer_init(&a); timer_init(&b); timer_init(&c);
    timer_set(&a, 5, on_order, (void*)(uintptr_t)1);
    timer_set(&b, 2, on_order, (void*)(uintptr_t)2);
    timer_set(&c, 2, on_order, (void*)(uintptr_t)3);
    advance(5);
    CHECK(g_order_len == 3);
    CHECK(g_order[0] == 2 && g_order[1] == 3 && g_order[2] == 1);

    /* Cancel prevents firing. */
    g_fired = 0;
    timer_set(&one, 2, on_fire, NULL);
    timer_cancel(&one);
    CHECK(!timer_is_active(&one));
    advance(5);
    CHECK(g_fired == 0);

    /* Re-arming an active timer moves its deadline. */
    timer_set(&one, 2, on_fire, NULL);
    timer_set(&one, 6, on_fire, NULL);
    advance(5);
    CHECK(g_fired == 0);
    advance(1);
    CHECK(g_fired == 1);

    /* A callback may re-arm its own timer. */
    g_fired = 0;
    timer_init(&g_periodic);
    timer_set(&g_periodic, 2, on_periodic, NULL);
    advance(6);
    CHECK(g_fired == 3);
    timer_cancel(&g_periodic);
    advance(4);
    CHECK(g_fired == 3);

    /* Zero ticks is clamped to the next tick. */
    g_fired = 0;
    timer_set(&one, 0, on_fire, NULL);
    advance(1);
    CHECK(g_fired == 1);
    return 0;
}

/* ---- watchdog ---- */
static int test_watchdog(void) {
    watchdog_init();

    CHECK(watchdog_register(1, NULL) == WATCHDOG_INVALID);
    watchdog_config_t bad = { .timeout_ms = 0, .restart_count_max = 0, .auto_restart = false };
    CHECK(watchdog_register(1, &bad) == WATCHDOG_INVALID);

    /* Fed in time: survives. Missed deadline: SIGKILL, then failed. */
    watchdog_config_t plain = { .timeout_ms = 50, .restart_count_max = 0, .auto_restart = false };
    watchdog_handle_t wd = watchdog_register(7, &plain);
    CHECK(wd != WATCHDOG_INVALID);
    CHECK(watchdog_is_alive(wd));
    advance(4);
    watchdog_feed(wd);
    advance(4);
    CHECK(watchdog_is_alive(wd) && g_kill_calls == 0);
    advance(1);
    CHECK(!watchdog_is_alive(wd));
    CHECK(g_kill_calls == 1 && g_killed_pid == 7 && g_killed_signal == SIGKILL);
    CHECK(!watchdog_restart_pending(wd));
    watchdog_feed(wd);
    advance(20);
    CHECK(g_kill_calls == 1);
    CHECK(watchdog_rebind(wd, 8) == -EBUSY);
    watchdog_unregister(wd);
    CHECK(!watchdog_is_alive(wd));

    /* Auto restart with a budget of one. */
    g_kill_calls = 0;
    watchdog_config_t restartable = { .timeout_ms = 10, .restart_count_max = 1, .auto_restart = true };
    wd = watchdog_register(9, &restartable);
    CHECK(wd != WATCHDOG_INVALID);
    CHECK(watchdog_rebind(wd, 10) == -EBUSY);
    advance(1);
    CHECK(g_kill_calls == 1 && g_killed_pid == 9);
    CHECK(!watchdog_is_alive(wd) && watchdog_restart_pending(wd));
    CHECK(watchdog_rebind(wd, 10) == 0);
    CHECK(watchdog_is_alive(wd) && watchdog_restart_count(wd) == 1);
    advance(1);
    CHECK(g_kill_calls == 2 && g_killed_pid == 10);
    CHECK(!watchdog_restart_pending(wd));
    CHECK(watchdog_rebind(wd, 11) == -EBUSY);
    watchdog_unregister(wd);

    /* A stale handle never matches a reused slot. */
    watchdog_handle_t first = watchdog_register(20, &plain);
    watchdog_unregister(first);
    watchdog_handle_t second = watchdog_register(21, &plain);
    CHECK(first != WATCHDOG_INVALID && second != WATCHDOG_INVALID && first != second);
    CHECK(!watchdog_is_alive(first) && watchdog_is_alive(second));
    CHECK(watchdog_rebind(first, 22) == -EINVAL);
    watchdog_unregister(first);
    CHECK(watchdog_is_alive(second));
    watchdog_unregister(second);

    /* Table full. */
    watchdog_handle_t all[WATCHDOG_MAX];
    for (int i = 0; i < WATCHDOG_MAX; i++) {
        all[i] = watchdog_register(100 + i, &plain);
        CHECK(all[i] != WATCHDOG_INVALID);
    }
    CHECK(watchdog_register(200, &plain) == WATCHDOG_INVALID);
    for (int i = 0; i < WATCHDOG_MAX; i++) watchdog_unregister(all[i]);
    g_kill_calls = 0;
    advance(20);
    CHECK(g_kill_calls == 0);
    return 0;
}

int main(void) {
    int status = test_timer();
    if (status) return status;
    return test_watchdog();
}
