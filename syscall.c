#include "types.h"
#include "defs.h"
#include "param.h"
#include "date.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep;
  struct proc *curproc = myproc();

  if(addr >= curproc->sz)
    return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
 
  if(argint(n, &i) < 0)
    return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_inc_num(void);
extern int sys_invoked_syscalls(void);
extern int sys_get_count(void);

static char* syscalls_string [24] = {
"sys_fork",
"sys_exit",
"sys_wait",
"sys_pipe",
"sys_read",
"sys_kill",
"sys_exec",
"sys_fstat",
"sys_chdir",
"sys_dup",
"sys_getpid",
"sys_sbrk",
"sys_sleep",
"sys_uptime",
"sys_open",
"sys_write",
"sys_mknod",
"sys_unlink",
"sys_link",
"sys_mkdir",
"sys_close",
"sys_inc_num",
"sys_invoked_syscalls",
"sys_get_count",
};

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_inc_num]   sys_inc_num,
[SYS_invoked_syscalls]   sys_invoked_syscalls,
[SYS_get_count] sys_get_count,
};

void fill_arglist(struct syscallarg* end, int type){
	int int_arg,int_arg2;
	switch(type){
                case 3:
                case 2:
		case 1:
			safestrcpy(end->type[0], "void", strlen("void")+1);break;
		case 6:
		case 22:
                case 23:
			safestrcpy(end->type[0], "int", strlen("int")+1);
			if (argint(0, &int_arg) < 0){
   				cprintf("inc_num: bad int arg val?\n");
   				break;
			}
			end->int_argv[0] = int_arg;
			break;
                case 24:
                        safestrcpy(end->type[0], "int", strlen("int")+1);
                        safestrcpy(end->type[1], "int", strlen("int")+1);
			if (argint(0, &int_arg) < 0 || argint(1, &int_arg2) < 0){
   				cprintf("inc_num: bad int arg val?\n");
   				break;
			}
			end->int_argv[0] = int_arg;
                        end->int_argv[1] = int_arg2;
			break;
	}
}

void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
    if (curproc->syscalls[num-1].count == 0){
    	curproc->syscalls[num-1].datelist = (struct date*)kalloc();
    	curproc->syscalls[num-1].datelist->next = 0;
    	curproc->syscalls[num-1].datelist_end = curproc->syscalls[num-1].datelist;
    	curproc->syscalls[num-1].arglist = (struct syscallarg*)kalloc();
    	curproc->syscalls[num-1].arglist->next = 0;
    	curproc->syscalls[num-1].arglist_end = curproc->syscalls[num-1].arglist;
    }else{
    	curproc->syscalls[num-1].datelist_end->next = (struct date*)kalloc();
        curproc->syscalls[num-1].datelist_end->next->next = 0;
    	curproc->syscalls[num-1].datelist_end = curproc->syscalls[num-1].datelist_end->next;
    	curproc->syscalls[num-1].arglist_end->next = (struct syscallarg*)kalloc();
        curproc->syscalls[num-1].arglist_end->next->next = 0;
    	curproc->syscalls[num-1].arglist_end = curproc->syscalls[num-1].arglist_end->next;
    }
    curproc->syscalls[num-1].count = curproc->syscalls[num-1].count + 1;
    safestrcpy(curproc->syscalls[num-1].name, syscalls_string[num-1], strlen(syscalls_string[num-1])+1);
   	cmostime(&(curproc->syscalls[num-1].datelist_end->date));
   	fill_arglist(curproc->syscalls[num-1].arglist_end, num);
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
