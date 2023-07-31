// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

struct spinlock memlock;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int memcount[INDEX(PHYSTOP)];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&memlock,"mem");
  freerange(end, (void*)PHYSTOP);
  printf("size of memcount:%d\n",sizeof(memcount)/sizeof(int));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&memlock);
  if(memcount[INDEX(pa)]>0)
  memcount[INDEX(pa)]--;
  // Fill with junk to catch dangling refs.
  if(memcount[INDEX(pa)]>0){
    release(&memlock);
    return;
  }
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  memcount[INDEX(pa)]=0;
  release(&kmem.lock);
  release(&memlock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    //acquire(&memlock);
    memcount[INDEX((uint64)r)] = 1;
    //release(&memlock);
    kmem.freelist = r->next;
  }
  release(&kmem.lock);
  
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int 
copycowpage(pagetable_t pagetable,uint64 va){
  //printf("before copy\n");
  //cowvmprint(pagetable);
  //acquire(&memlock);
  va = PGROUNDDOWN(va);
  //cowvmprint(pagetable);
  if(ifcowpage(pagetable,va)!=1){
  printf("copycowpage:its not cow page\n");
  //release(&memlock);
  return -1;
  }
  uint64 mem = (uint64)kalloc();
  if(mem==0){
    //printf("copycowpage:kalloc error\n");
    //release(&memlock);
    return -1;
  }
  pte_t *pte = walk(pagetable,va,0);
  uint64 src = PTE2PA(*pte);
  if(pte==0){
    //release(&memlock);
    printf("copypagetable:walk error\n");
    return -1;
  }
  uint64 flag = PTE_FLAGS(*pte);
  flag = (flag&(~PTE_COW))|PTE_W;
  memmove((void*)mem,(void*)src,PGSIZE);
  uvmunmap(pagetable,va,1,1);
  if(mappages(pagetable,va,PGSIZE,mem,flag)<0){
    //release(&memlock);
    //uvmunmap(pagetable,va,1,1);
    printf("copycowpage:mappages error\n");
    return -1;
  }
  //printf("after copy\n");
  //cowvmprint(pagetable);
  //release(&memlock);
  return 0;
}