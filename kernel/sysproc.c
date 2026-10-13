#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  backtrace();
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


uint64
sys_sigalarm(void)
{
  printf("workhere\n");
  int ticks;
  if(argint(0,&ticks)<0)
    return -1;
  uint64 p;
  if(argaddr(1,&p)<0)
    return -1;
  //printf("%p\n",p);
  if(p==0&&ticks==0)
    myproc()->aset=0;  
  else{
    myproc()->ticks=ticks;
    myproc()->handler=p;
    myproc()->aset=1;
  }

  return 0;
}

uint64
sys_sigreturn(void)
{
  printf("sys:b epc:%p\n",myproc()->trapframe->epc);
  memmove(myproc()->trapframe,
        &myproc()->backup_tf,
        sizeof(struct trapframe));
  printf("sys:a epc:%p\n",myproc()->trapframe->epc);
  return 0;
}