#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

int vmprint(pagetable_t page)
{ 
  if(page==0)
    return -1;
  printf("page table %p\n",page);
  pte_t pte;
  pagetable_t p2=page;
  pagetable_t p1;
  pagetable_t p0;
  printf("%p\n",p2[0]);
  for(int i = 0;i<512;i++){//level 2
    pte = (pte_t)p2[i];
    if(pte&PTE_V){  
    p1 = (pagetable_t)PTE2PA(pte);
    printf("..%d: pte %p pa %p \n",i,pte,p1);
      for(int j = 0;j<512;j++){//level 1
        pte = (pte_t)p1[j];
        if(pte&PTE_V){
        p0 = (pagetable_t)PTE2PA(pte);
        printf(".. ..%d: pte %p pa %p \n",j,pte,p0);
          for(int k = 0;k<512;k++){//level 0 
            pte = (pte_t)p0[k];
            if(pte&PTE_V){  
            printf(".. .. ..%d: pte %p pa %p\n",k,pte,PTE2PA(pte));
            }
          }
        }
      }
    }

  }

  return 0;
}
