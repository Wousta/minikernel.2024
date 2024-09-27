/*
 *  kernel/include/mutex.h
 *
 *  Minikernel (versión 2.0)
 *
 *  Fernando Pérez Costoya
 *
 */

#ifndef _MUTEX_H
#define _MUTEX_H

#include "list.h"

#define MAX_MUTEX_NAMELENGTH	32
#define MAX_NR_MUTEX		16
#define MAX_NR_MUTEX_PER_PROC	4
#define LOCKED			1
#define UNLOCKED		0

typedef struct {
    int nMutex;
    char name[MAX_MUTEX_NAMELENGTH];
    int state;
    int ownerpid;
    list blocked;
} mutex_t;

void init_mutex_module(void);

int do_mutex_open(char *name);
int do_mutex_close(int mutid);
int close_all_mutexes(void);
int do_mutex_lock(int mutid);
int do_mutex_unlock(int mutid);


#endif /* _MUTEX_H */

