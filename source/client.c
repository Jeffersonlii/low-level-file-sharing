#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#define IP argv[1]
#define PORT argv[2]
#define fileReq argv[3]
#define saveAs argv[4]

int main(int argc, char **argv){

    int cfd;
    struct sockaddr_in address;
    if (argc < 5) {//check args
        fprintf(stderr, "Need atleast 4 cmd line arguments\n");
        return 1;
    }
    if(-1 == (cfd = socket(AF_INET,SOCK_STREAM,0))){//make socket
        perror("socket error");
        return 1;
    }
  
    //address set up
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(PORT));
    if (0 == inet_pton(AF_INET, IP, &address.sin_addr)) {
        fprintf(stderr, "Invalid address.\n");
        return 1;
    }

    if (-1 == connect(cfd, (struct sockaddr *)&address, sizeof(struct sockaddr_in))) {
        perror("connect error");
        return 1;
    }

    char buf[100];
    strcpy(buf, fileReq);
    if(-1==(write(cfd, buf, sizeof(buf)-1))){//give the file request name
        perror("write error");
        close(cfd);
        return 1;
    }

    char key=0;
    read(cfd,&key,1);
    if(key==4){//check first char of file, designated as file existence info
        fprintf(stderr,"File not found.\n");
        return 1;
    }

    //file found!
    fprintf(stderr,"Downloading file...\n");
    FILE* newfile = NULL;
    if((newfile=fopen(saveAs,"w"))==NULL){//create file
        perror("creating new file error");
        close(cfd);
        return 1;
    }
    int newfilefd = fileno(newfile);//the fd of the new file
    char buff[1024];
    int len;
    while((len = read(cfd, buff, 1024)) > 0)//write to new file
        write(newfilefd, buff, len);

    close(cfd);
    fprintf(stderr,"Finsished!\n");
    return 0;
     
    
}
