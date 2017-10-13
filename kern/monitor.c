// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display stack backtrace", mon_backtrace },
	{ "showmappings", "Display memory mapping for a single address or a range of or virtual/linear addresses in the currently active address space", mon_showmappings },
	{ "setperm", "Set permission bit of a mapping", mon_setperm },
	{ "dumpvm", "Dump memory content for given virtual address range", mon_dumpvm },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t ebp = read_ebp();
	uint32_t eip;
	struct Eipdebuginfo debug_info;
	cprintf("Stack backtrace:\n");
	while (ebp != 0)
	{
		cprintf("  ebp %08x", ebp);
		eip = *(uint32_t*)(ebp + 4);
		debuginfo_eip(eip, &debug_info);
		cprintf("  eip %08x", eip);
		cprintf("  args");
		for (int i = 0; i < 5; ++i)
		{
			cprintf(" %08x", *(uint32_t*)(ebp + 8 + 4 * i));
		}
		cprintf("\n");
		cprintf("         %s:%u: %.*s+%u\n", debug_info.eip_file,
			debug_info.eip_line,
			debug_info.eip_fn_namelen,
			debug_info.eip_fn_name,
			eip - debug_info.eip_fn_addr);
		ebp = *(uint32_t*)ebp;
	}
	return 0;
}

void print_flags(pte_t pte)
{
	cprintf(pte & PTE_G ? "G," : "-,");
	cprintf(pte & PTE_PS ? "PS," : "-,");
	cprintf(pte & PTE_D ? "D," : "-,");
	cprintf(pte & PTE_A ? "A," : "-,");
	cprintf(pte & PTE_PCD ? "PCD," : "-,");
	cprintf(pte & PTE_PWT ? "PWT," : "-,");
	cprintf(pte & PTE_U ? "U," : "-,");
	cprintf(pte & PTE_W ? "W," : "-,");
	cprintf(pte & PTE_P ? "P" : "-");
}

void print_perm(pte_t pte)
{
	cprintf(pte & PTE_U ? "U" : "-");
	cprintf(pte & PTE_W ? "W" : "-");
	cprintf(pte & PTE_P ? "P" : "-");
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 2) {
		cprintf("Usage: showmappings start_addr [end_addr]\n");
		cprintf("       end_addr will default to start_addr.\n");
		return 0;
	}
	uintptr_t start, end;
	start = strtol(argv[1], 0, 0);
	if (argc > 2)
		end = strtol(argv[2], 0, 0);
	else
		end = start;
	int num_pages = (ROUNDDOWN(end, PGSIZE) - ROUNDDOWN(start, PGSIZE)) / PGSIZE + 1;
	cprintf("va            pa            perm(User, Writeable, Present)\n");
	for (uintptr_t base_addr = ROUNDDOWN(start, PGSIZE); num_pages > 0; base_addr += PGSIZE, num_pages--) {
		cprintf("0x%08x", base_addr);
		int create = 0; // do not create pgtable
		pte_t *pte = pgdir_walk(kern_pgdir, (void *)base_addr, create);
		if (pte == NULL || !(*pte & PTE_P))
			cprintf("    no mappings\n");
		else {
			cprintf("    0x%08x    ", (*pte) & (~0x3FF));
			print_perm(*pte);
			cprintf("\n");
		}
	}
	return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 4) {
		cprintf("Usage: setperm virtual_addr U|W 1|0\n");
		cprintf("       U: user level access\n");
		cprintf("       W: writable access\n");
		cprintf("       1|0: set|clear the bit\n");
		return 0;
	}
	uintptr_t addr = strtol(argv[1], 0, 0);
	int create = 0;
	pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, create);
	if (pte != NULL && *pte & PTE_P) {
		uint32_t flag = (argv[2][0] == 'U' ? PTE_U : PTE_W);
		// set the perm bit
		if (argv[3][0] == '1')
			*pte |= flag;
		else
			*pte &= ~flag;
	} else {
		cprintf("Invalid address, page not exists.\n");
	}
	return 0;
}

int mon_dumpvm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Usage: dumpvm start_addr n_bytes\n");
	}
	uintptr_t start = strtol(argv[1], 0, 0);
	long bytes = strtol(argv[2], 0, 0);
	// unaligned cross page-boundary access fine, processor took care of it?
	for (; bytes > 0; bytes -= 4, start += 4) {
		cprintf("0x%08x: 0x%08x\n", start, *(int *)start);
	}
	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
