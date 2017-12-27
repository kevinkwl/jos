/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
*/
#define TRAPHANDLER(name, num)						\
	.text;                                                          \
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps;                                                  \
	.data;                                                          \
	.long name;

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.text;								\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps;							\
	.data;								\
	.long name;

.data
.globl vectors
vectors:
.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(th0, 0)
TRAPHANDLER_NOEC(th1, 1)
TRAPHANDLER_NOEC(th2, 2)
TRAPHANDLER_NOEC(th3, 3)
TRAPHANDLER_NOEC(th4, 4)
TRAPHANDLER_NOEC(th5, 5)
TRAPHANDLER_NOEC(th6, 6)
TRAPHANDLER_NOEC(th7, 7)
TRAPHANDLER(th8, 8)
TRAPHANDLER_NOEC(th9, 9)
TRAPHANDLER(th10, 10)
TRAPHANDLER(th11, 11)
TRAPHANDLER(th12, 12)
TRAPHANDLER(th13, 13)
TRAPHANDLER(th14, 14)
TRAPHANDLER_NOEC(th15, 15)
TRAPHANDLER_NOEC(th16, 16)
TRAPHANDLER_NOEC(th17, 17)
TRAPHANDLER_NOEC(th18, 18)
TRAPHANDLER_NOEC(th19, 19)
TRAPHANDLER_NOEC(th20, 20)
TRAPHANDLER_NOEC(th21, 21)
TRAPHANDLER_NOEC(th22, 22)
TRAPHANDLER_NOEC(th23, 23)
TRAPHANDLER_NOEC(th24, 24)
TRAPHANDLER_NOEC(th25, 25)
TRAPHANDLER_NOEC(th26, 26)
TRAPHANDLER_NOEC(th28, 28)
TRAPHANDLER_NOEC(th29, 29)
TRAPHANDLER_NOEC(th30, 30)
TRAPHANDLER_NOEC(th31, 31)

// syscall
.data
.globl vector_syscall
vector_syscall:
TRAPHANDLER_NOEC(th48, 48)
/*
 * Lab 3: Your code here for _alltraps
 */

.text

_alltraps:
	pushl %ds
	pushl %es
	pushal

	movw $(GD_KD), %ax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp
	call trap
