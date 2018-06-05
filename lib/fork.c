// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
		panic("pgfault: not a write or to a copy-on-write page");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	void *pg_addr = ROUNDDOWN(addr, PGSIZE);
	if ((r = sys_page_alloc(0, (void *)PFTEMP, PTE_U|PTE_W|PTE_P)) < 0)
		panic("alloc to PFTEMP failed: %e", r);
	memmove((void *)PFTEMP, pg_addr, PGSIZE);
	if ((r = sys_page_map(0, (void *)PFTEMP, 0, pg_addr, PTE_U|PTE_W|PTE_P)) < 0)
		panic("map PFTEMP to fault addr failed: %e", r);
	if ((r = sys_page_unmap(0, (void *)PFTEMP)) < 0)
		panic("unmap PFTEMP failed: %e", r);
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	uintptr_t addr = pn * PGSIZE;
	if (uvpt[pn] & PTE_SHARE) {
		// TODO: if parent env is cow, then child cannot see the updates of parent env ??
		if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, uvpt[pn] & PTE_SYSCALL)) < 0)
			panic("map to child shared failed: %e", r);
	} else if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {
		if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, PTE_COW|PTE_U|PTE_P)) < 0)
			panic("map to child failed: %e", r);

		// if the page is already COW, we still need to remap it with COW, why?
		// because when we are mapping the stack pages, the previously sys_page_map call 
		// could trigger a page fault on current env (push args to stack), therefore a new
		// page will be mapped as writable but not COW, which clear the COW bit, causing 
		// problems for future fork.
		if ((r = sys_page_map(0, (void *)addr, 0, (void *)addr, PTE_COW|PTE_U|PTE_P)) < 0)
			panic("remap to curenv COW failed: %e", r);
	} else {
		if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, uvpt[pn] & PTE_SYSCALL)) < 0)
			panic("map to child read-only failed: %e", r);
	}
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	set_pgfault_handler(pgfault);

	envid_t child_envid = sys_exofork();
	if (child_envid < 0)
		return child_envid;
	if (child_envid == 0) {
		// in child!
		// fix thisenv
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (uintptr_t addr = 0; addr < USTACKTOP; addr += PGSIZE) {
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P))
			if (duppage(child_envid, PGNUM(addr)) < 0)
				panic("fork: duppage failed at PGNUM %08x", PGNUM(addr));
	}

	// alloc mem for child exception
	if ((r = sys_page_alloc(child_envid, (void *)(UXSTACKTOP - PGSIZE), PTE_W|PTE_U|PTE_P)) < 0)
		panic("alloc for exception stack failed: %e", r);

	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(child_envid, _pgfault_upcall)) < 0)
		panic("set pgfault upcall for child failed: %e", r);

	// Start the child environment running
	if ((r = sys_env_set_status(child_envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return child_envid;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
