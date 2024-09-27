/*
 *  kernel/mutex.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Template by Fernando Pérez Costoya
 *  Implemented by Luis Bustamante

 */


/* Funciones relacionadas con la gestión de los mutex */

#include "common.h"
#include "mutex.h"
#include "strings.h"
#include "process.h"
#include "syscall.h"
#include "sched.h"
#include "clock.h"

// mutid es el indice del mutex en la lista de mutexes del proceso (MAX_NR_MUTEX_PER_PROC como maximo)
// mutpos es el indice del mutex en la lista de mutexes del sistema (MAX_NR_MUTEX como maximo)

static int add_mutex(int mutpos);
static int checkMutid(int mutid);
static int exists_deadlock(mutex_t mutex);

static int mut_created = 0;
static mutex_t mut_list[MAX_NR_MUTEX];

void init_mutex_module(void) {
    for(int i = 0; i < MAX_NR_MUTEX; i++) {
        mut_list[i].name[0] = '\0';
        mut_list[i].nMutex = 0;
        mut_list[i].state = UNLOCKED;
        mut_list[i].ownerpid = -1;
        list_init(&mut_list[i].blocked);
    }
    printk("Modulo mutex inicializado\n");
}

int do_mutex_open(char *name) {
    // Comprueba que el nombre del mutex es valido, si no lo es, devuelve error
    if(check_syscall_arg_string_read(name, MAX_MUTEX_NAMELENGTH) == -1) {
        printk("Error: nombre de mutex invalido\n");
        return -1;
    }
    // Comprueba que no se han creado mas mutexes de los permitidos, si es asi, devuelve error
    if(mut_created == MAX_NR_MUTEX) {
        printk("Error: no mas mutexes | mutexes: %d\n", mut_created);
        return -1;
    }

    // Busca indice del mutex en la lista de mutexes, -1 si no lo encuentra
    int mutpos = -1;
    for(int i = 0; i < MAX_NR_MUTEX && mutpos == -1; i++) {
        if(str_cmp(name, mut_list[i].name, MAX_MUTEX_NAMELENGTH) == 0) {
            mutpos = i;
        }
    }

    // Si se encuentra el mutex, se añade a la lista de mutexes del proceso y no hace falta crearlo
    if(mutpos != -1) return add_mutex(mutpos);

    // Si no existe, se crea y se añade a la lista de mutexes del proceso
    // Primero buscamos un hueco en la lista de mutexes
    for(int i = 0; i < MAX_NR_MUTEX && mutpos == -1; i++) {
        if(mut_list[i].name[0] == '\0')
            mutpos = i;
    }

    // Una vez encontrado el hueco, se añade a la lista de mutexes del proceso si hay espacio
    int res = add_mutex(mutpos);
    if(res != -1){
        // Hay espacio para mutex en el proceso, por lo que copiamos el nombre dado en el campo name del mutex,
        // que es el paso que falta para crear el mutex completamente.
        str_cpy(mut_list[mutpos].name, name, MAX_MUTEX_NAMELENGTH);
        mut_created++;
        printk("Creado mutex %s | mutexes: %d | mutpos: %d\n", name, mut_created, mutpos);
    }
    return res;
}

int do_mutex_close(int mutid) {
    // variable mutex es el descriptor del mutex "mutid" en la lista de mutexes del proceso
    mutex_t *mutex = current->mutexes[mutid];

    // Comprueba que el mutid es valido
    if(checkMutid(mutid) == -1) return -1;

    //Si el proceso actual es propietario del mutex, se libera
    if(mutex->ownerpid == current->pid) do_mutex_unlock(mutid);

    // Si solo hay una ocurrencia del mutex, se reinicia el mutex de la lista de mutexes
    // y se decrementa el contador de mutexes
    if(mutex->nMutex == 1) {
        mutex->name[0] = '\0';
        mut_created--;
        printk("Mutex %d ya no tiene mas referencias\n", mutid);
    }
    // Si hay mas de una ocurrencia, solo se decrementa el contador de mutexes y
    // despues se pone a null el descriptor del proceso
    mutex->nMutex--;
    current->mutexes[mutid] = NULL;
    printk("Decrementamos mutex %d\n", mutid);
    return 0;
}

int do_mutex_lock(int mutid){
    mutex_t *mutex = current->mutexes[mutid];

    // Comprueba que mutid es valido
    if(checkMutid(mutid) == -1) return -1;

    // Si el mutex esta bloqueado, se bloquea el proceso actual y se añade a la lista de procesos bloqueados del mutex
    while(mutex->state == LOCKED) {
        int level = set_int_priority_level(LEVEL_3);
        // Comprobamos si hay interbloqueo
        if(exists_deadlock(*mutex)) return -1;

        current->state = BLOCKED;
        current->mutex_waiting = current->mutexes[mutid];
        remove_ready_queue();
        insert_last(&mutex->blocked, current);
        set_int_priority_level(level);
        printk("Bloqueado proceso %d esperando a mutex %s\n", current->pid, current->mutex_waiting->name);
        pick_and_activate_next_task(1);
    }
    // En este punto el mutex esta desbloqueado, por lo que se toma posesion
    mutex->state = LOCKED;
    mutex->ownerpid = current->pid;
    printk("Proceso %d toma posesion del mutex mutid:%d name:%s\n", current->pid, mutid, mutex->name);
    return 1;
}

int do_mutex_unlock(int mutid){
    mutex_t *mutex = current->mutexes[mutid];

    // Comprueba que el mutid es valido y que el proceso actual es propietario del mutex
    if(checkMutid(mutid) == -1 || mutex->ownerpid != current->pid){
        printk("Error procesando unlock\n");
        return -1;
    }

    if(!list_is_empty(&mutex->blocked)) {
        int level = set_int_priority_level(LEVEL_3);
        // Buscamos procesos bloqueados en el mutex y liberamos al mas prioritario
        PCB *p = proc_to_unlock(&mutex->blocked);
        p->state = READY;
        p->slice_counter = SLICE;   // Reinicia el contador de rodaja al desbloquear
        p->mutex_waiting = NULL;    // Indicamos que el proceso ya no espera el mutex
        remove_elem(&mutex->blocked, p);
        add_ready_queue(p);
        set_int_priority_level(level);
        printk("Desbloqueado proceso %d\n", p->pid);
    }
    else printk("No hay procesos bloqueados en el mutex %d\n", mutid);

    // Liberamos el mutex
    mutex->state = UNLOCKED;
    mutex->ownerpid = -1;
    return 1;
}

////////////////////////// FUNCIONES AUXILIARES //////////////////////////

// Asigna la direccion del mutex con indide mutpos a la lista de mutexes del proceso si hay espacio
// e incrementa el contador de mutexes en la posicion mutpos
static int add_mutex(int mutpos) {
    // El mutid es el indice del mutex en la lista de mutexes del proceso, para facilitar la busqueda al cerrar el mutex
    for(int mutid = 0; mutid < MAX_NR_MUTEX_PER_PROC; mutid++) {
        if(current->mutexes[mutid] == NULL) {
            current->mutexes[mutid] = &mut_list[mutpos];
            mut_list[mutpos].nMutex++;
            // Devuelve el indice del mutex en la lista de mutexes del proceso, para asegurar que
            // multiples aperturas del mismo mutex devuelven distinto descriptor y por tanto ocupan otro espacio
            return mutid;
        }
    }
    printk("Error: espacio para mutex lleno\n");
    return -1;
}

// Comprueba que el mutid es valido
static int checkMutid(int mutid) {
    if(current->mutexes[mutid] == NULL) {
        printk("Error: mutex del proceso no encontrado\n");
        return -1;
    }

    char *name = current->mutexes[mutid]->name;
    if(name[0] == '\0') {
        printk("Error: mutex no encontrado en la lista de mutex\n");
        return -1;
    }
    if(check_syscall_arg_string_read(name, MAX_MUTEX_NAMELENGTH) == -1) {
        printk("Error: nombre de mutex invalido\n");
        return -1;
    }
    return 1;
}


// ALgoritmo recursivo que comprueba si hay interbloqueo en el sistema,
// devuelve 1 si hay interbloqueo
static int exists_deadlock(mutex_t mutex) {
    if(mutex.ownerpid == current->pid) {
        printk("        Error: Interbloqueo detectado\n\n");
        return 1;
    }

    PCB *owner = NULL;
    int found = 0;
    for(int i = 0; i < MAX_NR_MUTEX; i++){
        if(list_is_empty(&mut_list[i].blocked)) continue;
        iterator it;
        iterator_init(&mut_list[i].blocked, &it);
        // Busca al propietario del mutex en las listas de bloqueados de los mutex
        while(iterator_has_next(&it) && !found) {
            owner = iterator_next(&it);
            if(owner->pid == mutex.ownerpid) {
                found = 1;
                break;
            }
        }
    }

    // Si el propietario del mutex no esta en la lista de bloqueados, no hay interbloqueo
    if(owner == NULL || !found) return 0;

    // Si el propietario del mutex no esta esperando a ningun mutex, no hay interbloqueo
    if(owner->mutex_waiting == NULL) return 0;

    // Si el propietario del mutex esta esperando a otro mutex, se llama recursivamente a la funcion
    return exists_deadlock(*owner->mutex_waiting);
}
