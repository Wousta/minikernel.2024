/*
 *  kernel/process.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Template by Fernando Pérez Costoya
 *  Implemented by Luis Bustamante
 *
 */

/* Fichero que contiene la funcionalidad de la gestión de procesos */

#include "HAL.h"
#include "process.h"
#include "sched.h"
#include "syscall.h"
#include "common.h"
#include "clock.h"
#include "mutex.h"
#include "strings.h"

// Variable global que identifica el proceso actual
PCB * current = NULL;

/* Variable global que representa la tabla de procesos */
static PCB proc_table[MAX_NR_PROC];

/* Función que inicia la tabla de procesos */
static void init_process_table(void){
    for (int i=0; i<MAX_NR_PROC; i++) proc_table[i].state = FINISHED;
}

/* Función que busca una entrada libre en la tabla de procesos.
   Asigna PIDs en orden creciente reciclando cuando hay desbordamiento */
static int search_free_PCB(void){
    static int nproc = MAX_NR_PROC, next = 0;
    for (int i=next; nproc--; i = (i + 1) % MAX_NR_PROC)
        if (proc_table[i].state == FINISHED)  {
            next = (i + 1) % MAX_NR_PROC;
            return i;
        }
    return -1;
}

// el proceso actual no puede seguir ejecutando;
// hay que salvar su contexto y activar el proceso elegido por planificador;
// si parámetro es 0, no salva el contexto.
void pick_and_activate_next_task(int save_ctx) {
    PCB *prev = current;
    current=scheduler();
    current->state = RUNNING;
    if (prev != current) {
        if (prev) printk("-> CAMBIO DE CONTEXTO DE %d A %d\n",
                prev->pid, current->pid);
        context *ctx = (prev && save_ctx) ? &(prev->context) : NULL;
        context_switch(ctx, &(current->context));
    }
}

/* Implementación de la llamada al sistema create_process */
int do_create_process(char *prog, int prio){
    void * image, *initial_pc;
    int error=0;
    int nr_proc;
    PCB *p_new;

    if (check_syscall_arg_string_read(prog, MAX_EXEC_NAMELENGTH) == -1)
        return -1;

    if ((prio > MAX_PRIO) || (prio < MIN_PRIO)) return -1;

    image=create_image(prog, &initial_pc);
    if (image) {
        nr_proc=search_free_PCB();
        if (nr_proc==-1) return -1;    /* no hay entrada libre */
        /* A rellenar el PCB ... */
        p_new=&(proc_table[nr_proc]);
        p_new->mem=image;
        p_new->stack=create_stack(STACK_SIZE);
        set_initial_context(p_new->mem, p_new->stack, STACK_SIZE,
            initial_pc, &(p_new->context));
        p_new->pid=nr_proc;
        p_new->priority=prio;
        p_new->slice_counter=SLICE;
        // Inicializo descriptores de mutex a -1 para indicar que no tienen mutex
        for (int i = 0; i < MAX_NR_MUTEX_PER_PROC; i++) {
            p_new->mutexes[i] = NULL;
        }
        p_new->mutex_waiting = NULL;

        printk("-> NUEVO PROCESO %d\n", p_new->pid);

        /* lo inserta en la cola de listos */
        add_ready_queue(p_new);
        error= 0;
    }
    else
        error= -1; /* fallo al crear imagen */
    return error;
}

/* Implementación de la llamada al sistema exit_process */
int do_exit_process(void){
    printk("-> TERMINA PROCESO %d\n", current->pid);
    release_image(current->mem);
    current->state=FINISHED;
    remove_ready_queue();
    for(int i = 0; i < MAX_NR_MUTEX_PER_PROC; i++) {
        if(current->mutexes[i]!= NULL) {
            do_mutex_close(i);
        }
    }
    release_stack(current->stack);
    pick_and_activate_next_task(0); // no salva estado del previo
    return 0; /* no debería llegar aquí ya que el proceso terminó */
}

/* Implementación de la llamada al sistema get_pid */
int do_get_pid(void){
    return current->pid;
}

/* Implementación de la llamada al sistema get_priority */
int do_get_priority(void){
    return current->priority;
}

///////////////manipula listas de procesos
int do_proc_sleep(unsigned int secs) {
    current->state = BLOCKED;
    current->wake_up_time = gettime() + secs*100; // convierte a centisegundos
    //int level = set_int_priority_level(LEVEL_3);
    remove_ready_queue();
    add_blocked_queue(current);
    //set_int_priority_level(level);
    pick_and_activate_next_task(1); // salva estado del previo
    return 0;
}

void init_process_module(void) {
    init_process_table();
}
