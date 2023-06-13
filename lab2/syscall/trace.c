#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int power(int quart){
    int result = 1;
    for(int i = 1;i<quart;i++)
        result *= 10;
    return result;
}
int turn_str_to_int(char *str){
    int length = strlen(str);
    int result = 0;
    for(int i = 0;i<length;i++){
        result+= (power(length-i)*(str[i]-'0'));
    }
    return result;

}
int 
main(int argc,char *argv[]){
    if(argc<2){
        printf("trace : wrong num of args\n");
        exit(0);
    }
    int num = turn_str_to_int(argv[1]);
    int mask = num|1;
    char *args[argc-1];
    for(int i = 2;i<argc;i++)
        args[i-2] = argv[i];
    args[argc-2] = 0;
    int p = fork();
    if(p==0){
        trace(mask);
        //sub process 
        exec(args[0],args);
    }
    else{
        // parent process
        
        wait(0);
    }
    exit(0);
}