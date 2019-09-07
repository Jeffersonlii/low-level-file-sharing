#include <arpa/inet.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#define PORT argv[1]

int main(int argc, char **argv){

    int sfd;
    struct sockaddr_in address;

    if (argc <= 1){//check args
        fprintf(stderr, "Requires 1 cmd line argument for port number\n");
        return 0;
    }
    if(-1 == (sfd = socket(AF_INET,SOCK_STREAM,0))){//make socket
        perror("socket error");
        return 1;
    }
    fcntl(sfd, F_SETFL, O_NONBLOCK);
    //address set up
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(PORT));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sfd, (struct sockaddr *)&address,//0.0.0.0
     sizeof(struct sockaddr_in))) {//try to bind
        perror("bind error");
        return 1;
    }
    if (-1 == listen(sfd, 10)) {//mark sfd as accepting connections
        perror("listen error");
        return 1;
    }

    //setup epoll
    int ep;
    if(-1==(ep = epoll_create1(0))){
        perror("epoll error");
        return 1;
    }
    //sets up event for sfd, then make it an interest
    struct epoll_event e;
    e.events = EPOLLIN;
    e.data.fd = sfd;
    epoll_ctl(ep,EPOLL_CTL_ADD, sfd, &e);//add sfd as interest

    int numofchildren=0;//number of children, so we know how many times to wait
    while(1){
        for (int i = 0; i < numofchildren; i++){//after every new client, attempt to kill children
            if((waitpid(-1,NULL,WNOHANG)>0)){//note that wait is in non blocking mode
                numofchildren--;//if a child has been waited on, decrease num of children
                fprintf(stderr,"Killing Child!\n");
            }     
        }
        fprintf(stderr, "waiting for connections\n");
        epoll_wait(ep,&e,1,-1);//wait for any connections
        if(e.data.fd == sfd){//a client is attemping to connect, set up the epoll
            struct sockaddr_in ca;
            socklen_t sinlen = sizeof(struct sockaddr_in);
            int cfd = accept(sfd, (struct sockaddr *)&ca, &sinlen);
            if (cfd != -1) {//error check
                e.events = EPOLLIN;
                e.data.fd = cfd;
                epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &e);//add this client to interests
            }
        }
        else{//previously added client is ready to interact
            int cfd = e.data.fd;
            char fileReq[4097];//the file to look for
            if(-1==(read(cfd,fileReq,4097))){//get the requested file name
                perror("read error");
                epoll_ctl(ep, EPOLL_CTL_DEL, cfd, NULL);
                close(cfd);
                continue;//serve new clients
            }
            fprintf(stderr,"File requested is %s\n",fileReq);

            pid_t p;
            if(-1 == (p = fork())){//create child labourers
                perror("fork error");
                epoll_ctl(ep, EPOLL_CTL_DEL, cfd, NULL);
                close(cfd);
                continue;//serve new clients
            }
            numofchildren++;//a new worker has been created
            if(!p){//worker code
                int fdReq;//fd of requested file
                char buf[1024];
                int len;//len of buf
                if(-1==(fdReq = open(fileReq,O_RDONLY))){
                    fprintf(stderr,"File not found.\n");
                    buf[0]=4;//4 is end of transmission
                    write(cfd, buf, 1);//signals the client that file is not found
                    exit(0);//nothing more to do
                }
                fprintf(stderr,"File found!\n");//file has been found, and ready to send
                buf[0]=2;//2 is start of file
                write(cfd, buf, 1);//write key char
                while((len = read(fdReq, buf, 1024)) > 0)
                    write(cfd, buf, len);
                close(fdReq);
                exit(0);
            }
            epoll_ctl(ep, EPOLL_CTL_DEL, cfd, NULL);
            close(cfd);//finshing working, close it down
        }
    }
}
