#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int
main(int argc,char*argv[]){
    if(argc==1){
        write(1,"error!\n",7);
        exit(0);
    }
    int n=atoi(argv[1]);
    sleep(n);
    exit(0);
}