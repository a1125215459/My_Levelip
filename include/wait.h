#ifndef WAIT_H
#define WAIT_H

#include <syshead.h>

/* 这是对 POSIX 条件变量（condition variable） 的轻量封装，实现了一个基础的"等待-唤醒"机制。*/
struct wait_lock {
    pthread_cond_t ready;       // 条件变量
    pthread_mutex_t lock;       // 关联互斥锁
    uint8_t sleeping;           // 睡眠标记
};

static inline int wait_init(struct wait_lock *w)
{
    pthread_cond_init(&w->ready, NULL); // 初始化条件变量
    pthread_mutex_init(&w->lock, NULL); // 初始化互斥锁
    w->sleeping = 0;                    // 初始无人睡眠

    return 0;
}

/*pthread_cond_wait 是一个原子操作序列：
    调用 wait_sleep 时:
    1. w->sleeping = 1           ← 设标记（调用者已持有锁）
    2. pthread_cond_wait 内部:
        a. 释放 w->lock            ← 原子性地
        b. 阻塞在条件变量上         ← 等待别的线程 signal
        c. 被唤醒后重新获取锁      ← 原子性地
    3. 返回
*/
static inline int wait_sleep(struct wait_lock *w)
{
    w->sleeping = 1;                        // 标记"我要睡了"
    pthread_cond_wait(&w->ready, &w->lock); // 原子性地释放锁并阻塞

    return 0;                               // 被唤醒后返回
}

static inline int wait_wakeup(struct wait_lock *w)
{
    pthread_mutex_lock(&w->lock);   // 加锁

    /*
        pthread_cond_signal 唤醒一个在 w->ready 上等待的线程。
        被唤醒的线程从 pthread_cond_wait 内部返回，此时它重新持有 w->lock。
    */
    pthread_cond_signal(&w->ready); // 唤醒等待者
    w->sleeping = 0;                // 清除睡眠标记

    pthread_mutex_unlock(&w->lock); // 解锁
    return 0;    
}

/*
    先调用 wait_wakeup 释放可能阻塞的线程，再销毁底层资源。
    否则如果有线程正在 pthread_cond_wait 中阻塞，直接销毁锁和条件变量会导致未定义行为。
*/
static inline void wait_free(struct wait_lock *w)
{
    wait_wakeup(w);                     // 先确保没有线程在睡眠

    pthread_mutex_destroy(&w->lock);    // 销毁互斥锁
    pthread_cond_destroy(&w->ready);    // 销毁条件变量
}

#endif