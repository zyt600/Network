#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include<stdlib.h>
#include <fcntl.h>

#define MYTEST 1
#define MAGIC_NUMBER_LENGTH 6



int connect_success=0,auth_success=0;
int MAXLINE=100;

struct myFTP_header{
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));
struct myFTP_header OPEN_CONN_REQUEST,OPEN_CONN_REPLY,AUTH_REQUEST,AUTH_REPLY,LIST_REQUEST,LIST_REPLY,GET_REQUEST,GET_REPLY,FILE_DATA,PUT_REQUEST,PUT_REPLY,QUIT_REQUEST,QUIT_REPLY;

const char myProtocol[6]={'\xe3','m','y','f','t','p'};
void initialize(){

    //initialize OPEN_CONN_REPLY
    memcpy(OPEN_CONN_REPLY.m_protocol,myProtocol,6);
    OPEN_CONN_REPLY.m_type=0xA2;
    OPEN_CONN_REPLY.m_status=1;
    OPEN_CONN_REPLY.m_length= htonl(12);

    //initialize AUTH_REPLY
    memcpy(AUTH_REPLY.m_protocol,myProtocol,6);
    AUTH_REPLY.m_type=0xA4;
    AUTH_REPLY.m_length=htonl(12);

    //initialize LIST_REPLY
    memcpy(LIST_REPLY.m_protocol,myProtocol,6);
    LIST_REPLY.m_type=0xA6;

    //initialize GET_REPLY
    memcpy(GET_REPLY.m_protocol,myProtocol,6);
    GET_REPLY.m_type=0xA8;
    GET_REPLY.m_length= htonl(12);

    //initialize FILE_DATA
    memcpy(FILE_DATA.m_protocol,myProtocol,6);
    FILE_DATA.m_type=0xFF;

    //initialize PUT_REPLY
    memcpy(PUT_REPLY.m_protocol,myProtocol,6);
    PUT_REPLY.m_length= htonl(12);
    PUT_REPLY.m_type=0xAA;

    //initialize QUIT_REPLY
    memcpy(QUIT_REPLY.m_protocol,myProtocol,6);
    QUIT_REPLY.m_length= htonl(12);
    QUIT_REPLY.m_type=0xAC;
}

int main(int argn,char **argv){
    initialize();
    if(argn<3){
        printf("argn<3,error\n");
        return 0;
    }
    int socket_fd;
    socket_fd=socket(AF_INET,SOCK_STREAM,0);
    
    struct sockaddr_in servaddr,cli_addr;
    bzero(&servaddr,   sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    inet_pton(AF_INET,argv[1],&servaddr.sin_addr);
    servaddr.sin_port=htons(atoi(argv[2]));
    // printf("%d,%d\n",atoi(argv[2]),servaddr.sin_port);
    if(MYTEST){printf("ip:%d,port:%s\nint big:%d\nint host:%d\n",servaddr.sin_addr,argv[2],servaddr.sin_port,atoi(argv[2]));}
    if(bind(socket_fd,&servaddr,sizeof(servaddr))<0)
        printf("bind error\n");

    listen(socket_fd,10);
    int got_client=0;
    int acpt_fd,cli_len=sizeof(cli_addr);
    while (1){
        if(MYTEST){ printf("prepare to accept\n");}
        acpt_fd= accept(socket_fd,&cli_addr,&cli_len);
        got_client=1;
        if(MYTEST){printf("successful accept\n");}

        struct myFTP_header receive_header;
        while ((read(acpt_fd,&receive_header,12))>0&&got_client==1){
            int write_num=0,read_num=0;

            switch (receive_header.m_type) {
                case (char) 0xA1:{//OPEN_CONN_REQUEST
                    memcpy(&OPEN_CONN_REQUEST,&receive_header,12);
                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status, ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xA1){ printf("OPEN_CONN_REQUEST type error\n");continue;}
                    if(ntohl(receive_header.m_length)!=12){printf("OPEN_CONN_REQUEST length error\n");continue;}
                    if(MYTEST){ printf("OPEN_CONN_REQUEST works good\n");}

                    connect_success=1;
                    write(acpt_fd,&OPEN_CONN_REPLY,12);

                    break;
                }
                case (char) 0xA3:{//AUTH_REQUEST
                    if(connect_success==0){
                        printf("no connection\n");
                        continue;
                    }
                    memcpy(&AUTH_REQUEST,&receive_header,12);
                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status,ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xA3){ printf("AUTH_REQUEST type error\n");continue;}
                    char read_buf[120];
                    memset(read_buf,0,120);

                    if(read(acpt_fd,read_buf,ntohl(receive_header.m_length)-12)<ntohl(receive_header.m_length)-12)
                        printf("read number is %d,not enough\n",strlen(read_buf));//TODO
                    if(strncmp(read_buf,"user 123123", strlen(read_buf))!=0){
                        if(MYTEST){ printf("auth fail,read_buf is:%s\nit's length is%d\n",read_buf,strlen(read_buf));
                            for(int i=0;i<strlen(read_buf);i++){
                                printf("%d:%d\n",i,read_buf[i]);
                            }
                        }
                        connect_success=0;
                        AUTH_REPLY.m_status=0;
                    }
                    else{
                        if(connect_success==1){
                            if(MYTEST){ printf("auth succeed\n");}
                            auth_success=1;
                            AUTH_REPLY.m_status=1;
                        }
                        else{
                            AUTH_REPLY.m_status=0;
                            if(MYTEST){ printf("no connection,so can't auth\n");}
                        }
                    }
                    write(acpt_fd,&AUTH_REPLY,12);

                    break;
                }
                case (char) 0xA5:{//LIST_REQUEST
                    if(connect_success==0){
                        printf("no connection\n");
                        continue;
                    }
                    if(auth_success==0){
                        printf("no auth\n");
                        continue;
                    }
                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status,ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xA5){ printf("LIST_REQUEST type error\n");continue;}
                    if(ntohl(receive_header.m_length)!=12){printf("LIST_REQUEST length error\n");continue;}


                    FILE *fp;
                    fp=popen("ls .","r");
                    char file_buf[2500],write_buf[2500];
                    memset(write_buf,0,2500);
                    int write_offset=0;
                    while (fgets(file_buf,2490,fp)){
                        int file_len=strlen(file_buf);
                        memcpy(write_buf+write_offset,file_buf,file_len);
                        write_offset+=file_len;
                    }
                    fclose(fp);
                    write_buf[write_offset]=0;
                    LIST_REPLY.m_length= htonl(12+write_offset+1);
                    write(acpt_fd,&LIST_REPLY,12);
                    write(acpt_fd,write_buf,write_offset+1);

                    if(MYTEST){ printf("list file num is %d\n",write_offset);}
                    break;
                }
                case (char) 0xA7:{//GET_REQUEST
                    if(connect_success==0){
                        printf("no connection\n");
                        continue;
                    }
                    if(auth_success==0){
                        printf("no auth\n");
                        continue;
                    }

                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status,ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xA7){ printf("GET_REQUEST type error\n");continue;}

                    int file_name_len= ntohl(receive_header.m_length)-12;//把最后一个结尾0也读进去
                    char file_name[2500],file_path[2500];
                    memset(file_name,0,2500);
                    memset(file_path,0,2500);

                    read_num= read(acpt_fd,file_name,file_name_len);

                    if(read_num<file_name_len){ printf("read number is%d,less than file_name_len\n",read_num);continue;}

                    char popen_cmd[2500];
                    memset(popen_cmd,0,2500);
                    memcpy(popen_cmd,"ls ",3);
                    memcpy(popen_cmd+3,file_name,file_name_len);
                    if(MYTEST){
                        printf("popencmd:");
                        for(int i=0;i< strlen(popen_cmd);i++){
                            printf("int:%d.char:%c\n",popen_cmd[i],popen_cmd[i]);
                        }
                    }

                    FILE *fp;
                    fp=popen(popen_cmd,"r");
                    char read_fp_buf[2500];
                    fgets(read_fp_buf,2500,fp);
                    if(MYTEST){ printf("read_fp_buf is %s\n",read_fp_buf);}
                    if(strncmp(read_fp_buf,file_name,file_name_len-1)==0){//文件存在
                        GET_REPLY.m_status=1;
                        write(acpt_fd,&GET_REPLY,12);

                        memcpy(popen_cmd,"du -b ",6);
                        memcpy(popen_cmd+6,file_name,file_name_len);
                        fp= popen(popen_cmd,"r");
                        int file_length=-999;
                        fscanf(fp,"%d",&file_length);
                        if (MYTEST){ printf("file_length:%d\n",file_length);}
                        FILE_DATA.m_length= htonl(12+file_length);

                        write(acpt_fd,&FILE_DATA,12);

                        memcpy(popen_cmd,"cat ",4);
                        memcpy(popen_cmd+4,file_name,file_name_len);
                        fp= popen(popen_cmd,"r");

                        char file_buf[2000];
                        int ret=0;
                        do {
                            ret=fread(file_buf,1,2000,fp);
                            if(ret==0){
                                if(MYTEST){ printf("finish getting\n");}
                                break;
                            }
                            write(acpt_fd,file_buf,ret);
                        } while (1);
                    }
                    else{//文件不存在
                        GET_REPLY.m_status=0;
                        write(acpt_fd,&GET_REPLY,12);
                        if(MYTEST){
                            printf("no such file\n");
                            printf("input name is:%s\nreadbuf is:%s\n",file_name,read_fp_buf);
                        }
                    }
                    break;
                }
                case (char) 0xA9:{//PUT_REQUEST
                    if(connect_success==0){
                        printf("no connection\n");
                        continue;
                    }
                    if(auth_success==0){
                        printf("no auth\n");
                        continue;
                    }
                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status,ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xA9){ printf("GET_REQUEST type error\n");continue;}
                    int file_name_len= ntohl(receive_header.m_length)-12;//包括了结尾的0
                    char file_name_buf[2500];
                    read(acpt_fd,file_name_buf,file_name_len);

                    write(acpt_fd,&PUT_REPLY,12);

                    read_num=read(acpt_fd,&FILE_DATA,12);
                    if(read_num<12){ printf("read number is%d,less than 12\n",read_num);}
                    if(strncmp(myProtocol,FILE_DATA.m_protocol,6)!=0){printf("protocol different:%s\n",FILE_DATA.m_protocol);continue;}
                    if(FILE_DATA.m_type!=(char)0xFF){ printf("FILE_DATA type error\n");continue;}

                    long long file_len= ntohl(FILE_DATA.m_length)-12;
                    char read_file_buf[2500];
                    int write_get_fd;
                    write_get_fd=open(file_name_buf,O_CREAT|O_RDWR|O_TRUNC,0777);
                    if(MYTEST){ printf("open file fd:%d\n",write_get_fd);}

                    while ((file_len!=0&&(read_num= read(acpt_fd,read_file_buf,2500))>0)){
                        file_len-=read_num;
                        write(write_get_fd,read_file_buf,read_num);
                        if(file_len==0){
                            close(write_get_fd);
                            if(MYTEST){printf("finish getting\n");}
                            break;
                        }
                    }
                    if(MYTEST){printf("zheli yunxing\n");}
                    close(write_get_fd);
                    break;
                }
                case (char) 0xAB:{//QUIT_REQUEST
                    if(connect_success==0){
                        printf("no connection\n");
                        continue;
                    }
                    if(auth_success==0){
                        printf("no auth\n");
                        continue;
                    }

                    if(strncmp(myProtocol,receive_header.m_protocol,6)!=0){printf("protocol different:%s\n",receive_header.m_protocol);continue;}
                    if(MYTEST){printf("server read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n", receive_header.m_protocol, receive_header.m_type, receive_header.m_status,ntohl(receive_header.m_length));}
                    if(receive_header.m_type!=(char)0xAB){ printf("QUIT_REQUEST type error\n");continue;}
                    if(ntohl(receive_header.m_length)!=12){ printf("QUIT_REQUEST length error\n");continue;}

                    auth_success=0;
                    connect_success=0;

                    write(acpt_fd,&QUIT_REPLY,12);
                    sleep(1);
                    got_client=0;
                    close(acpt_fd);
                    break;
                }
                default:{
                    printf("receive_header type error\n");
                    break;
                }
            }
//
        }

    }


}
