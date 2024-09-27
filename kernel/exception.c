/*
 *  kernel/exception.c
 *
 *  Minikernel (versión 2.0)
 *
 *  Fernando Pérez Costoya
 *
 */

#include "HAL.h"
#include "exception.h"
#include "process.h"
#include "common.h"
#include "syscall.h"

/* Funciones relacionadas con el tratamiento de excepciones */
static void exit_proc_y_msg(char *msg) {
    printk(msg);
    do_exit_process();
}

static void arit_exception_handler(void) {
    comes_from_usermode() ? exit_proc_y_msg("excepcion aritmetica modo usuario\n") : panic("excepcion aritmetica en modo sistema");
}

static void mem_exception_handler(void) {
    if(comes_from_usermode()) {
        exit_proc_y_msg("excepcion de memoria en modo usuario\n");
    } else if(check_arg) {
        check_arg = 0;
        exit_proc_y_msg("excepcion de memoria en modo sistema y checkarg\n");
    } else {
        panic("excepcion de memoria en modo sistema");
    }
}

void init_exception_module(void) {
    register_irq_handler(ARITM_EXC, arit_exception_handler);
    register_irq_handler(MEM_EXC, mem_exception_handler);
}