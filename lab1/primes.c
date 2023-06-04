#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void settle(int p[]);
int main(int argc,char *argv[]){
    int p[2];
    pipe(p);
    for(int i = 2;i<=35;i++){
        write(p[1],&i,4);
    }
    settle(p);
    exit(0);
}

void settle(int p[]){
    close(p[1]);
    int prime = 0;
    int flag = read(p[0],(char*)&prime,4);
    if(flag==0)
    return;
    printf("prime %d\n",prime);
    int pn[2];
    pipe(pn);
    int mid_var = 0;
    //传进来的管道写端一定堵死
    //线性的
    if(fork()==0){//child proces
        close(pn[0]);
      while(flag !=0){ 
        flag = read(p[0],(char*)&mid_var,4);
        if(mid_var%prime !=0)
        write(pn[1],(char*)&mid_var,4);
      }
      close(pn[1]);
      close(p[0]);
      exit(0);
    }
    else{
        wait(0);
       close(p[0]);
        settle(pn);
        
    }
}
    
