static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
二者什么差异，什么使用场景，各举例

在多线程编程中，pthread_mutex_t（互斥锁）和 pthread_rwlock_t（读写锁）是两种最常用的同步机制。简单来说，它们的内核逻辑差异在于：互斥锁是“独占”的，而读写锁实现了“读者共享，写者独占”的精细化控制。1. 核心差异对比特性互斥锁 (pthread_mutex_t)读写锁 (pthread_rwlock_t)锁定规则独占。 无论线程是想读还是写，同一时间只能有一个线程持有锁。共享/独占。 允许多个线程同时持有“读锁”；但“写锁”是独占的。锁的状态2种：锁定、未锁定。3种：读锁定、写锁定、未锁定。开销极小，速度非常快，内部结构简单。比互斥锁略大，因为需要维护读者计数器、等待队列等。并发度较低（所有线程排队）。较高（读操作不互相阻塞）。2. 使用场景与选择指南使用互斥锁（Mutex）的场景：写操作多、读操作少（或者读写频率差不多）。临界区非常小，执行极快（此时读写锁维护状态的额外开销可能大于并发带来的好处）。对性能要求极高、不需要复杂控制的简单同步。使用读写锁（RWLock）的场景：读操作极其频繁，而写操作极少（典型的“读多写少”场景）。临界区内的操作较慢，且多个读线程并行执行能显著提升吞吐量。3. 代码示例示例一：互斥锁（Mutex）应用场景： 多个线程同时往一个全局账单中“写入”数据（如多线程累加余额）。因为每一次操作都是写（修改），所以必须用互斥锁完全独占。C#include <pthread.h>
#include <stdio.h>

static int balance = 1000; // 共享资源
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* deposit(void* arg) {
    pthread_mutex_lock(&lock);     // 【加锁】独占访问
    
    balance += 100;                // 修改数据（写操作）
    printf("存款成功，当前余额: %d\n", balance);
    
    pthread_mutex_unlock(&lock);   // 【解锁】
    return NULL;
}
示例二：读写锁（RWLock）应用场景： 一个在线查分系统，有几千个学生在同时“查询成绩”（只读），极少数情况下老师会“修改成绩”（写入）。C#include <pthread.h>
#include <stdio.h>

static int score = 95; // 共享资源
static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

// 读线程：可以有成百上千个线程同时进入该函数而互不阻塞
void* view_score(void* arg) {
    pthread_rwlock_rdlock(&rwlock);  // 【加读锁】允许多个读线程同时进入
    
    printf("学生查询到成绩为: %d\n", score); // 只读操作
    
    pthread_rwlock_unlock(&rwlock);  // 【解读锁】
    return NULL;
}

// 写线程：同一时间只能有一个写线程进入，且会阻塞所有的读线程
void* update_score(void* arg) {
    pthread_rwlock_wrlock(&rwlock);  // 【加写锁】独占，阻止其他读和写
    
    score = 98;                      // 修改操作
    printf("老师修改了成绩为: %d\n", score);
    
    pthread_rwlock_unlock(&rwlock);  // 【解写锁】
    return NULL;
}
总结在你的项目重构（如 My_Levelip IP 协议栈）中：如果是全局统计数据累加、队列入队出队，直接用 pthread_mutex_t 效率最高。如果是路由表、ARP 缓存表、套接字状态表的查询与更新（通常查询极为频繁，更新极少），可以考虑使用 pthread_rwlock_t 来提升网络协议栈的多线程并发吞吐量。