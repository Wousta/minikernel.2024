/*
 *  kernel/terminal.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Template by Fernando Pérez Costoya
 *  Implemented by Luis Bustamante
 *
 */


#include "HAL.h"
#include "terminal.h"
#include "syscall.h"
#include "common.h"
#include "fifo.h"
#include "list.h"
#include "process.h"
#include "clock.h"
#include "sched.h"

static void add_wait_list(PCB *p);
static void remove_wait_list(PCB *p);

// Buffer del terminal
static char term_buffer[TERM_BUF_SIZE];
static fifo term_fifo;
list wait_list;

/* Funciones relacionadas con la gestión del terminal */
static void terminal_interrupt_handler(void) {
    char c = read_port(KEYBOARD_PORT);

    if(fifo_is_full(&term_fifo)) {
        printk("-> INT. TECLADO FIFO LLENO\n");
        return;
    }
    printk("-> INT. TECLADO: proc %d lee caracter %c\n",current->pid, c);

    // Bloquea la interrupción de teclado
    int level = set_int_priority_level(LEVEL_2);
    // Inserta el carácter en el buffer del terminal
    fifo_in(&term_fifo, &c);
    // Desbloquea al proceso mas prioritario de la lista de espera si la cola no está vacía
    if(!list_is_empty(&wait_list)) {
        PCB *p = (proc_to_unlock(&wait_list));
        // Comprobamos prioridad del proceso para ver si se desbloquea
        printk("-> Añade proceso %d a ready list\n", p->pid);
        remove_wait_list(p);
        add_ready_queue(p);
    }
    set_int_priority_level(level);
}

// implementación de la llamada al sistema
int do_print(char * buf, int size) {
    if (size <= 0) return -1;
    if (check_syscall_arg_pointer_read(buf, size) == -1)
        return -1;
    print_terminal(buf, size); // operación del HAL
    return 0;
}
void init_terminal_module(void) {
    init_keyboard_controller();
    fifo_init(&term_fifo, sizeof(char), TERM_BUF_SIZE, term_buffer);
    list_init(&wait_list);
    register_irq_handler(KEYBOARD_INT, terminal_interrupt_handler);
}

int do_get_char(void) {
    char c;
    // inhibir las interrupciones de ese dispositivo en el tramo que abarca la consulta de la condición y el bloqueo
    int level = set_int_priority_level(LEVEL_3);

    while(fifo_is_empty(&term_fifo)) {
        printk("-> NO HAY CARACTERES. ESPERA INTERRUPCION proceso %d\n", current->pid);
        remove_ready_queue();
        add_wait_list(current);
        set_int_priority_level(level);
        pick_and_activate_next_task(1);
        set_int_priority_level(LEVEL_3); // Restaura el nivel de interrupción antes de volver a entrar al bucle
    }
    level = set_int_priority_level(LEVEL_2);  // Aqui deberia ser nivel 2
    fifo_out(&term_fifo, &c);
    set_int_priority_level(level);
    printk("-> CARACTER LEIDO por proc %d: %c\n",current->pid, c);
    return c;
}

// Funciones auxiliares
static void add_wait_list(PCB *p) {
    int level = set_int_priority_level(LEVEL_2);
    p->state = BLOCKED; // Ya realizado en la llamada a la función
    insert_last(&wait_list, p);
    set_int_priority_level(level);
}

static void remove_wait_list(PCB *p) {
    int level = set_int_priority_level(LEVEL_2);
    p->state = READY;
    p->slice_counter = SLICE; // Reinicia el contador de rodaja al desbloquear
    remove_elem(&wait_list, p);
    set_int_priority_level(level);
}
