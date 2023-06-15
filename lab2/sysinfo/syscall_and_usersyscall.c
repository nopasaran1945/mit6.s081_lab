//syscall 
uint64
sys_sysinfo(){
  struct sysinfo sys;
  uint64 user_sys;
  if(argaddr(0,&user_sys)<0)
  return -1; //arg from user
  nfree(&sys);
  nprocess(&sys);
  if(copyout(myproc()->pagetable,user_sys,(char*)&sys,sizeof(struct sysinfo))<0)
  return -1;
  return 0;
}
//in kalloc.c
//count free mem
int 
nfree(struct sysinfo *sys){
  if(sys==0){
    printf("wrong addr of argument(pointer)");
    return -1;
  }
  unsigned int num_free = 0;
  struct run * wp = kmem.freelist;
  while(wp!=0){
    num_free++;
    wp = wp->next;
  }
  sys->freemem = num_free*4096;
  return 0;
}
//in proc.c 
//count processes that aren't 
int 
nprocess(struct sysinfo *sys){
  if(sys==0){
    printf("error:nprocess:sys is null\n");
    return -1;
  }
  struct proc *p;
  unsigned int count = 0;
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state != UNUSED)
      count++;
  }
  sys->nproc = count;
  return 0;
}
