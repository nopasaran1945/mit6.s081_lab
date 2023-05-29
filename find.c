#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
char buf[512];
void 
find(char *path,char *target);
char*
fmtname(char *path);
int 
main(int argc, char *argv[]){
    if(argc!=3){
        printf("wrong number of arguments\n");
        exit(0);
    }
    find(argv[1],argv[2]);
    exit(0);
}
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void 
find(char *path,char *target){
    struct dirent de;
    struct stat st;
    int fd;
    char *p;
//是目录就递归，不是目录就直接报错，用一个SWITCH 来进行判断
    fd=open(path,0);
    if(fd<0){
        printf("find:cannot open %s\n",path);
        return;
    }
    if(fstat(fd,&st)<0){
        printf("find:cannot stat %s\n",path);
        close(fd);
        return;
    }
    switch (st.type)
    {
    case T_FILE://if normal file
        /* code */
        if(strcmp(fmtname(path),target)==0){
                    printf("%s\n",buf);
                }
        break;
    case T_DIR://if directory ,continue the recrusion
        if(strlen(path)+1+DIRSIZ+1>sizeof(buf)){
            printf("find:path too long\n");
            break;
        }
        strcpy(buf,path);
        p = buf+strlen(buf);
        *p++ = '/';
        
        while(read(fd,&de,sizeof(de))==sizeof(de)){
            if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0)
            continue;
            if(de.inum==0)
            continue;
            memmove(p,de.name,DIRSIZ);
            p[DIRSIZ]=0;
            if(stat(buf,&st)<0){
                printf("find:cannot stat %s\n",buf);
            }
            if(st.type==T_DIR){
                find(buf,target);
            }
            else if(st.type==T_FILE){
                if(strcmp(de.name,target)==0)
                printf("%s\n",buf);
            }
        }
        break;
    }
    close(fd);
    return;
}
