#ifndef TIMER_H
#define TIMER_H

#include "syshead.h"
#include "utils.h"
#include "list.h"

#define timer_dbg(msg, t)                                                       \
    do {                                                                       \
            print_debug("Timer at %d: "msg": expires %d", tick, t->expires);    \
    } while (0)

struct timer {
    struct list_head list;     // 链表节点，挂入全局定时器链表
    int refcnt;                // 引用计数
    uint32_t expires;          // 到期时间（绝对 tick 值）
    int cancelled;             // 取消标志
    void *(*handler)(void *);  // 到期回调函数
    void *arg;                 // 回调参数
    pthread_mutex_t lock;      // 保护此定时器自身状态的锁
};

struct timer *timer_add(uint32_t expire, void *(*handler)(void *), void *arg);
void timer_oneshot(uint32_t expire, void *(*handler)(void *), void *arg);
void timer_release(struct timer *t);
void timer_cancel(struct timer *t);
void *timers_start();
int timer_get_tick();

#endif