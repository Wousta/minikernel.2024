/*
 *  kernel/sched.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Template by Fernando Pérez Costoya
 *  Implemented by Luis Bustamante
 *
 */

/* Fichero que contiene la funcionalidad de la planificación */

#include "HAL.h"
#include "sched.h"
#include "common.h"
#include "bitmask.h"
#include "clock.h"

/* Variable que representa la cola de procesos listos */
static ready_vec_t ready_vec;

list blocked_list;

static void software_interrupt_handler(void) {
    //int sliceEnded = 0;
    // If para el funcionamiento del turno rotatorio cuando se agota la rodaja
    if(current != NULL && current -> state == RUNNING && current -> slice_counter == 0){
        // Elimina el proceso actual de la cola de listos y lo añade al final
        remove_ready_queue();
        add_ready_queue(current);

        current -> slice_counter = SLICE;
        //sliceEnded = 1;
    }
    //if(sliceEnded) printk("-> ACTIVA INTERRUPCIÓN SOFTWARE POR FIN DE RODAJA\n");
    //else printk("-> ACTIVA INTERRUPCIÓN SOFTWARE\n");
    pick_and_activate_next_task(1);
}

void init_sched_module(void) {
    for (int i=0; i<NR_PRIO; i++) {
        list_init(&ready_vec.ready_list[i]);
    }
    ready_vec.prio_mask = 0;

    // Registra el manejador de interrupción de software
    register_irq_handler(SW_INT, software_interrupt_handler);
}

/* Espera a que se produzca una interrupcion */
static void wait_for_int(void){
    int level;

    //printk("-> NO HAY LISTOS. ESPERA INTERRUPCIÓN\n");

    /* Baja al mínimo el nivel de interrupción mientras espera */
    level=set_int_priority_level(LEVEL_1);
    halt();
    set_int_priority_level(level);
}

// añade un proceso a la cola de listos
void add_ready_queue(PCB *p) {
    int level = set_int_priority_level(LEVEL_3);
    // Añade el proceso a la cola de listos y activa la máscara de bits
    p->state=READY;
    insert_last(&ready_vec.ready_list[p->priority], p);
    set_bit(&ready_vec.prio_mask, p->priority);

    // Comprueba si el proceso actual tiene menor prioridad que el nuevo proceso
    // Si es así, activa la interrupción de software
    if(current != NULL && p->priority > current->priority){
        activate_soft_int();
    }
    set_int_priority_level(level);
}

// elimina el proceso actual de la cola de listos
void remove_ready_queue(void) {
    int level = set_int_priority_level(LEVEL_3);
    remove_elem(&ready_vec.ready_list[current->priority], current);
    if (list_is_empty(&ready_vec.ready_list[current->priority]))
        clear_bit(&ready_vec.prio_mask, current->priority);
    set_int_priority_level(level);
}

void add_blocked_queue(PCB *p) {
    int level = set_int_priority_level(LEVEL_3);
    p->state = BLOCKED; // Ya realizado en la llamada a la función
    insert_last(&blocked_list, p);
    set_int_priority_level(level);
}

void remove_blocked_queue(PCB *p) {
    int level = set_int_priority_level(LEVEL_3);
    p->state = READY;
    p->slice_counter = SLICE; // Reinicia el contador de rodaja al desbloquear
    remove_elem(&blocked_list, p);
    set_int_priority_level(level);
}

// Desbloquea el primer proceso de la lista de procesos bloqueados del mutex
PCB *proc_to_unlock(list *list) {

    iterator it;
    PCB *p, *p_sel = NULL;
    int prio = 1<<31; // mínimo valor negativo
    for (iterator_init(list, &it); iterator_has_next(&it); ) {
        p = iterator_next(&it);
        if (p->priority > prio) {
            p_sel = p; prio = p->priority;
        }
    }
    return p_sel;
}

/* Función de planificacion  */
/* complejidad O(1) */
PCB * scheduler(void) {
    while (ready_vec.prio_mask == 0){
        wait_for_int(); // nada que hacer
    }

    // Obtiene el primer bit encendido y devuelve el primer proceso de la cola de prioridad
    int prio = find_last_bit_set(ready_vec.prio_mask);
    return (PCB *) ready_vec.ready_list[prio].first;
}

/*
void init_sched_module(void) {
    list_init(&ready_list);
}
*/
