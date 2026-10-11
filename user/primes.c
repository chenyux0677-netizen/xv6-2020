#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void loop(int *p1){
    int x;
    int n=read(p1[0],&x,sizeof(int));
    if(n==0){return;}
    int pid=fork();
    if(pid==0){
        printf("prime %d\n",x);
        int p2[2];
        pipe(p2);
        int y;
        while(read(p1[0],&y,sizeof(int))){
            if(y%x!=0){
                 write(p2[1],&y,sizeof(int));
             }
        }
        close(p1[0]);
        close(p2[1]);
        loop(p2);
    }else{
        close(p1[0]);
        wait(0);
        return;
    }
}

int main(void){
    int p[2];
    pipe(p);
    for(int i=2;i<=35;i++){
        write(p[1],&i,sizeof(int));
    }
    close(p[1]);
   loop(p);
   exit(0);
}