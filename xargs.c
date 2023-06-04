#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
int main(int argc,char *argv[]){
    char **args =(char **)malloc(sizeof(char*)*argc);
    char **p = args;
    int length = argc-1;
    char str[32]={0};
    char buf = ' ';
    int count_p = 0;
    char *new = 0;
    for(int i = 0;i<argc-1;i++){
        args[i]= malloc(sizeof(char)*strlen(argv[i+1]));
        strcpy(args[i],argv[i+1]);
    }
    while(read(0,&buf,sizeof(char))>0){
        if(buf==' '||buf=='\n'){          
            new  = (char*)malloc((strlen(str))*sizeof(char));
            strcpy(new,str);
            length++;
            args = (char **)malloc(sizeof(char*)*(length+1));
            for(int i = 0;i<length-1;i++)
                args[i] = p[i];
            args[length-1]=new;
            args[length]=0;
            free(p);
            p = args;            
            memset(str,0,32);
            count_p = 0;
        }else{
        str[count_p] = buf;
        count_p++;
        }
        
    }
    int pid = fork();
    int status;
    if(pid == 0){
        exec(args[0],args);
    }
    else{
        wait(&status);
    }
    exit(0);
}
