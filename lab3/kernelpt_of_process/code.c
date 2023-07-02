//in vm.c
//initialize the processes' kernel page table 
void
ProcessKptInit(pagetable_t page){
  //copy kernel page table 
  memset(page,0,PGSIZE);//clean the allocated memory
  mappages(page,UART0,PGSIZE,UART0,PTE_R|PTE_W);
  mappages(page,VIRTIO0,PGSIZE,VIRTIO0,PTE_R|PTE_W);
  mappages(page,CLINT,0x10000,CLINT,PTE_R|PTE_W);
  mappages(page,PLIC,0x400000,PLIC,PTE_R|PTE_W);
  mappages(page,KERNBASE,(uint64)etext-KERNBASE,KERNBASE,PTE_R|PTE_X);
  mappages(page,(uint64)etext,PHYSTOP-(uint64)etext,(uint64)etext,PTE_R|PTE_W);
  mappages(page,TRAMPOLINE,PGSIZE,(uint64)trampoline,PTE_R|PTE_X);
  return;
}       
//in proc.c
//scheduler(modified)
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();   
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        //CopyKPT(p->kernelpt);
        ChangeProcKPT(p->kernelpt);
        //MapUserPtToKernel(p);
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        kvminithart();
        c->proc = 0;
        
        found = 1;
      }
      release(&p->lock);
    }

#if !defined (LAB_FS)
    if(found == 0) {

      intr_on();
      asm volatile("wfi");
    }
#else
    ;
#endif
  }
}

//in proc.c 
// modified allocproc
//alloc the field p->kernelpt 
static struct proc*
allocproc(void)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  //Allocate the processes' kernel page table 
  if((p->kernelpt = (pagetable_t)kalloc())==0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  ProcessKptInit(p->kernelpt);
  //build the kernel stack of process in prosesses' kernel page table 
  char *pa = kalloc();
  if(pa == 0)
      panic("kalloc");
  uint64 va = KSTACK((int) 0); 
  mappages(p->kernelpt,va,PGSIZE,(uint64)pa,PTE_R | PTE_W);
  p->kstack = va;


  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  //vmprint(p->pagetable);
  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

//free a page table but dont free the leaf physical mem pages
void 
freept(pagetable_t pagetable){
  if(!pagetable){
    panic("freept");
    return;
  }
  pagetable_t p2=pagetable;
  pagetable_t p1;
  pte_t pte;
  for(int i = 0;i<512;i++){
    pte = (pte_t)p2[i];
    if(pte&PTE_V){
    p1 = (pagetable_t)PTE2PA(pte);
    for(int j = 0;j<512;j++){
      pte = (pte_t)p1[j];
      if(pte&PTE_V){
        kfree((void*)PTE2PA(pte));
      }
    }
    kfree((void*)PTE2PA((pte_t)p2[i]));
    }
  }
  kfree((void*)pagetable);
}

//in proc.c
//modified freeproc
//free the kernel page table of process
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  if(p->kstack){
    pte_t *pte = walk(p->kernelpt,p->kstack,0);
    if(!pte)
    panic("freeproc:free kstack");
    kfree((void*)PTE2PA(*pte));
    p->kstack = (uint64)0;
  }
  freept(p->kernelpt);
  p->kernelpt = 0;
}
