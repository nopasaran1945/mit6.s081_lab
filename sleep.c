#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
//#include"user/ulib.c"
int 
main(int argc,char * argv[]){
    int n;
    if(argc!=2){
        printf("invalid number of arg\n");
        exit(1);
    }
    n = atoi(argv[1]);
    if(n<=0){
       printf("invalid arg\n");
        exit(1);
    }
    
    sleep(n);
    exit(0);
    
}