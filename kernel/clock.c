/*
 *  kernel/clock.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Template by Fernando Pérez Costoya
 *  Implemented by Luis Bustamante
 *
 */

/* Fichero que contiene la funcionalidad relacionada con el tiempo */

#include "common.h"
#include "HAL.h"
#include "clock.h"
#include "sched.h"

static long ticks = 0;

unsigned int gettime(void) {
    return ticks;
}

static void clock_interrupt_handler(void) {
    ticks++;
    //printk("-> INT. RELOJ tick %d\n", ticks);

    iterator it;
    PCB *p;

    // Inicia un iterador para la lista de procesos bloqueados
    iterator_init(&blocked_list, &it);

    // Recorre la lista de procesos bloqueados
    while(iterator_has_next(&it)) {
        p = (PCB *) iterator_next(&it);

        // Verifica si el tiempo de despertar del proceso ha llegado
        if(p->wake_up_time <= ticks) {
            // Elimina el proceso de la lista de bloqueados y añade el proceso a la cola de listos

            //int level = set_int_priority_level(LEVEL_3);
            remove_blocked_queue(p);
            add_ready_queue(p);
            //set_int_priority_level(level);
        }
    }

    if(current != NULL) {
        current->slice_counter--;
        if(current->slice_counter == 0) {
            activate_soft_int();
        }
    }
}

void init_clock_module(void) {
    init_clock_controller(TICK);
    register_irq_handler(CLOCK_INT, clock_interrupt_handler);
}
