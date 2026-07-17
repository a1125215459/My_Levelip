#ifndef SKBUFF_H
#define SKBUFF_H

#include "netdev.h"
#include "route.h"
#include "list.h"
#include <pthread.h>

struct sk_buff {
    struct list_head list;
    struct rtentry *rt;
    struct netdev *dev;
};






#endif