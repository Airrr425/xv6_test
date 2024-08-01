#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_alloc(void){
  void *a = malloc(16);
  printf("addr of a is %p\n",a);
  printf("释放a\n");
  free(a);
  void *p1 = malloc(50);
  void *p5 = malloc(1024);
  void *p2 = malloc(100);
  void *p3 = malloc(1024);
  printheap();
  free(p1);
  free(p2);
  printf("释放p1和p2\n");
  printheap();
  void *p4 = malloc(80);
  printf("为p4分配空间\n");
  printheap();
  free(p3);
  printf("释放p3\n");
  printheap();
  free(p4);
  free(p5);
  return 0;
}

uint64
sys_bfa(void){
  void *p1 = best_fit_alloc(8*1024*1024+1024);
  void *p2 = best_fit_alloc(256);
  void *p3 = best_fit_alloc(1024);
  printheap();
  printf("释放p1\n");
  free(p1);
  printheap();
  void *p4 = best_fit_alloc(1);
  printf("为p4分配内存块\n");
  printheap();
  free(p2);
  free(p3);
  free(p4);
  return 0;
}

uint64
sys_wfa(void){
  void *p1 = worst_fit_alloc(1024);
  void *p2 = worst_fit_alloc(256);
  void *p3 = worst_fit_alloc(1024);
  printheap();
  printf("释放p2\n");
  free(p2);
  printheap();
  void *p4 = worst_fit_alloc(1);
  printf("为p4分配内存块\n");
  printheap();
  free(p1);
  free(p3);
  free(p4);
  return 0;
}



uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
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
