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

struct myFTP_header{
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));
struct myFTP_header OPEN_CONN_REQUEST,OPEN_CONN_REPLY,AUTH_REQUEST,AUTH_REPLY,LIST_REQUEST,LIST_REPLY,GET_REQUEST,GET_REPLY,FILE_DATA,PUT_REQUEST,PUT_REPLY,QUIT_REQUEST,QUIT_REPLY;

const char myProtocol[6]={'\xe3','m','y','f','t','p'};

void initialize(){
    //initialize OPEN_CONN_REQUEST
    memcpy(OPEN_CONN_REQUEST.m_protocol,myProtocol,6);
    OPEN_CONN_REQUEST.m_type=0xA1;
    OPEN_CONN_REQUEST.m_length=htonl(12);

    //initialize AUTH_REQUEST
    memcpy(AUTH_REQUEST.m_protocol,myProtocol,6);
    AUTH_REQUEST.m_type=0xA3;

    //initialize LIST_REQUEST
    memcpy(LIST_REQUEST.m_protocol,myProtocol,6);
    LIST_REQUEST.m_type=0xA5;
    LIST_REQUEST.m_length= htonl(12);

    //GET_REQUEST
    memcpy(GET_REQUEST.m_protocol,myProtocol,6);
    GET_REQUEST.m_type=0xA7;

    //initialize PUT_REQUEST
    memcpy(PUT_REQUEST.m_protocol,myProtocol,6);
    PUT_REQUEST.m_type=0xA9;

    //initialize FILE_DATA
    memcpy(FILE_DATA.m_protocol,myProtocol,6);
    FILE_DATA.m_type=0xFF;

    //initialize QUIT_REQUEST
    memcpy(QUIT_REQUEST.m_protocol,myProtocol,6);
    QUIT_REQUEST.m_length=htonl(12);
    QUIT_REQUEST.m_type=0xAB;
}

int MAXLINE=100;

/*用于分开形如    aaa<空格>bbb以0或\n结尾    的字符串*/
void parseCMD(char* cmd,char* arg1,char* arg2){
    int i = 0;
    while (cmd[i] != ' ')
        i++;
    while (cmd[i] == ' ')
        i++;
    int offset1=0;
    while (cmd[i] != ' ') {
        arg1[offset1] = cmd[i];
        i++;offset1++;
    }
    while (cmd[i] == ' ')
        i++;
    int offset2=0;
    while (cmd[i] != 0&&cmd[i]!='\n') {
        arg2[offset2] = cmd[i];
        i++;
        offset2++;
    }
}
int main(){

    initialize();
    int socket_fd;
    socket_fd=socket(AF_INET,SOCK_STREAM,0);

    struct sockaddr_in servaddr;
    bzero(&servaddr,   sizeof(servaddr));
    servaddr.sin_family   =   AF_INET;
    
    char cmd[100];
    memset(cmd,0,sizeof(cmd));

    int connect_success=0,auth_success=0;
    printf("Client> ");
    while (fgets(cmd,100,stdin)){
        if(cmd[strlen(cmd)-1]=='\n'){
            cmd[strlen(cmd)-1]=0;
            if(MYTEST){printf("change endl to 0\n");}
        }

        int write_num=0,read_num=0;

        switch (cmd[0]) {
            case 'o': {//open
                char charIP[100],charPort[100];
                memset(charIP,0,100);
                memset(charPort,0,100);
                parseCMD(cmd,charIP,charPort);
                inet_pton(AF_INET, charIP, &servaddr.sin_addr);
                int inputPort = htons(atoi(charPort));
                servaddr.sin_port = inputPort;
                if(MYTEST){printf("IP:%s\nPORT:%s\nint big:%d\nint host:%d\n",charIP,charPort,inputPort,atoi(charPort));}

                if(connect(socket_fd, &servaddr, sizeof(servaddr))!=0)
                    printf("connect error\n");


                write_num=write(socket_fd, &OPEN_CONN_REQUEST, 12);
                if(write_num<12){ printf("write number is:%d,less than 12\n",write_num);}
                if(MYTEST){printf("finish writing OPEN_CONN_REQUEST\n");}


                read_num=read(socket_fd,&OPEN_CONN_REPLY,12);


                if(read_num<12){printf("read number is %d,not enough\n",read_num);}
                if(MYTEST){printf("client read: protocol:%s\ntype:%x\nstatus:%d\nlength:%d\n",OPEN_CONN_REPLY.m_protocol,OPEN_CONN_REPLY.m_type,OPEN_CONN_REPLY.m_status,ntohl(OPEN_CONN_REPLY.m_length));}
                if(strncmp(myProtocol,OPEN_CONN_REPLY.m_protocol,6)!=0){printf("protocol different:%s\n",OPEN_CONN_REPLY.m_protocol);break;}
                if(OPEN_CONN_REPLY.m_type!=(char)0xA2){ printf("OPEN_CONN_REPLY type error\n");break;}
                if(ntohl(OPEN_CONN_REPLY.m_length)!=12){printf("OPEN_CONN_REPLY length error\n");break;}
                if(MYTEST){ printf("OPEN_CONN_REPLY works good\n");}
                connect_success=1;
                break;
            }
            case 'a':{//auth
                if(connect_success==0){ printf("haven't connected\n");break;}
                char username[100],password[100];
                memset(username,0,100);
                memset(password,0,100);
                parseCMD(cmd,username,password);
                if(MYTEST){printf("input:username:%s\npassword:%s\n",username,password);}
                int len_username=strlen(username),len_password=strlen(password);
                AUTH_REQUEST.m_length=htonl(12+len_username +1/*中间一个空格的1*/+len_password +1/*结尾一个0的1*/);
                if(ntohl(AUTH_REQUEST.m_length)>100){ printf("username+password too long!\n");break;}
                char write_buf[120];
                memset(write_buf,0,120);
                memcpy(write_buf,&AUTH_REQUEST,12);
                memcpy(write_buf+12,username,len_username);
                memset(write_buf+12+len_username,' ',1);
                memcpy(write_buf+12+len_username+1,password,len_password);
//                if(MYTEST){
//                    for(int i=0;i<ntohl(AUTH_REQUEST.m_length);i++)
//                        printf("num%d:int%d:char:%c\n",i,write_buf[i],write_buf[i]);
//                }


                write_num=write(socket_fd,write_buf,ntohl(AUTH_REQUEST.m_length));
                if(write_num<ntohl(AUTH_REQUEST.m_length)){ printf("write number is:%d,less than %d\n",write_num,AUTH_REQUEST.m_length);}


                read_num=read(socket_fd,&AUTH_REPLY,12);


                if(read_num<12){printf("read number is %d,not enough\n",read_num);}
                if(strncmp(myProtocol,AUTH_REPLY.m_protocol,6)!=0){printf("protocol different:%s\n",AUTH_REPLY.m_protocol);break;}
                if(AUTH_REPLY.m_type!=(char)0xA4){ printf("AUTH_REPLY type error\n");break;}
                if(ntohl(AUTH_REPLY.m_length)!=12){printf("AUTH_REPLY length error\n");break;}
                if(AUTH_REPLY.m_status==0){
                    if(MYTEST){ printf("auth fail\n");}
                    connect_success=0;
                    auth_success=0;
                }
                else{
                    if(MYTEST){ printf("auth success\n");}
                    auth_success=1;
                }
                break;
            }
            case 'l':{
                if(connect_success==0){ printf("haven't connected\n");break;}
                if(auth_success==0){ printf("haven't auth\n");break;}

                write_num=write(socket_fd,&LIST_REQUEST,12);
                if(write_num<12){printf("write number is:%d,less than 12\n",write_num);}

                read_num= read(socket_fd,&LIST_REPLY,12);
                if(read_num<12){ printf("read number is%d,less than 12\n",read_num);}
                if(strncmp(myProtocol,LIST_REPLY.m_protocol,6)!=0){printf("protocol different:%s\n",LIST_REPLY.m_protocol);break;}
                if(LIST_REPLY.m_type!=(char)0xA6){ printf("LIST_REPLY type error\n");break;}


                int file_len= ntohl(LIST_REPLY.m_length)-13,got_len_now=0;
                char read_file_buf[2500];

                while (read(socket_fd,read_file_buf,2500)){
                    read_num=strlen(read_file_buf);
                    got_len_now+=read_num;
                    printf("%s",read_file_buf);
                    if(MYTEST){ printf("read num this time:%d\n",read_num);}
                    if(got_len_now==file_len)
                        break;
                }
                if(MYTEST){ printf("finish list successfully\n");}

                break;
            }
            case 'g':{
                if(connect_success==0){ printf("haven't connected\n");break;}
                if(auth_success==0){ printf("haven't auth\n");break;}

                char file_name[2500],file_path[2500];
                memset(file_name,0,2500);
                memset(file_path,0,2500);
                file_path[0]='.';file_path[1]='/';
                int file_name_offset=0,cmd_offset=0,file_path_offset=2;
                while (cmd[cmd_offset]!=' ')
                    cmd_offset++;
                while (cmd[cmd_offset]==' ')
                    cmd_offset++;
                while (cmd[cmd_offset]!=0){
                    file_name[file_name_offset]=cmd[cmd_offset];
                    file_path[file_path_offset]=cmd[cmd_offset];
                    if(MYTEST){printf("int:%d,char:%c\n",cmd[cmd_offset],cmd[cmd_offset]);}
                    cmd_offset++;file_name_offset++;file_path_offset++;
                }
                GET_REQUEST.m_length= htonl(12+1+file_name_offset);

                write_num=write(socket_fd,&GET_REQUEST,12);
                if(write_num<12){printf("write number is:%d,less than 12\n",write_num);}
                write(socket_fd,file_name,file_name_offset+1);
                if(MYTEST){ printf("filename length is:%d\n",file_name_offset);}


                read_num=read(socket_fd,&GET_REPLY,12);
                if(read_num<12){ printf("read number is%d,less than 12\n",read_num);}
                if(strncmp(myProtocol,GET_REPLY.m_protocol,6)!=0){printf("protocol different:%s\n",GET_REPLY.m_protocol);break;}
                if(GET_REPLY.m_type!=(char)0xA8){ printf("GET_REPLY type error\n");break;}
                if (ntohl(GET_REPLY.m_length)!=12){printf("GET_REPLY length error\n");break;}

                if(GET_REPLY.m_status==0){
                    printf("reply says no such file\n");
                }
                else{
                    if(MYTEST){ printf("file exists,start to get\n");}
                    read_num= read(socket_fd,&FILE_DATA,12);
                    if(read_num<12){ printf("read number is%d,less than 12\n",read_num);}
                    if(strncmp(myProtocol,FILE_DATA.m_protocol,6)!=0){printf("protocol different:%s\n",FILE_DATA.m_protocol);break;}
                    if(FILE_DATA.m_type!=(char)0xFF){ printf("FILE_DATA type error\n");break;}

                    long long file_len= ntohl(FILE_DATA.m_length)-12;
                    char read_file_buf[2500];
                    int write_get_fd;
                    write_get_fd=open(file_path,O_CREAT|O_RDWR|O_TRUNC,0777);
                    if(MYTEST){ printf("open file fd:%d\n",write_get_fd);}
                    while (file_len!=0&&((read_num= read(socket_fd,read_file_buf,2500))>0)){
                        file_len-=read_num;
                        write(write_get_fd,read_file_buf,read_num);
                        if(file_len==0){
                            if(MYTEST){printf("finish getting\n");}
                            break;
                        }
                    }
                    close(write_get_fd);
                }
                break;
            }
            case 'p':{
                if(connect_success==0){ printf("haven't connected\n");break;}
                if(auth_success==0){ printf("haven't auth\n");break;}
                int file_name_offset=0,cmd_offset=0,popen_cmd_offset=3;
                char file_name[2500],popen_cmd[2500];
                memset(file_name,0,2500);
                memset(popen_cmd,0,2500);
                memcpy(popen_cmd,"ls ",3);
                while (cmd[cmd_offset]!=' ')
                    cmd_offset++;
                while (cmd[cmd_offset]==' ')
                    cmd_offset++;
                while (cmd[cmd_offset]!=0){
                    file_name[file_name_offset]=cmd[cmd_offset];
                    popen_cmd[popen_cmd_offset]=cmd[cmd_offset];
                    cmd_offset++;file_name_offset++;popen_cmd_offset++;
                }
                FILE *fp;
                char read_fp_buf[2500];
                fp=popen(popen_cmd,"r");
                fgets(read_fp_buf,2500,fp);
//                  TODO:client和server各种gfets有没有吃最后的换行
                if(strncmp(read_fp_buf,file_name,file_name_offset)==0){
                    if(MYTEST){ printf("file exists, able to put,start putting\n");}
                    PUT_REQUEST.m_length= ntohl(12+1+file_name_offset);

                    write_num=write(socket_fd,&PUT_REQUEST,12);

                    if(write_num<12){printf("write number is:%d,less than 12\n",write_num);}

                    //写文件名
                    write_num=write(socket_fd,file_name,file_name_offset+1);
                    if(write_num<file_name_offset+1){printf("write number is:%d,less than file_name_offset+1\n",write_num);}

                    read(socket_fd,&PUT_REPLY,12);
//                    TODO:一堆检查


                    memset(popen_cmd,0,2500);
                    memcpy(popen_cmd,"du -b ",6);
                    memcpy(popen_cmd+6,file_name,file_name_offset);
                    fp= popen(popen_cmd,"r");
                    int file_length=-999;
                    fscanf(fp,"%d",&file_length);
                    if (MYTEST){ printf("file_length:%d\n",file_length);}
                    FILE_DATA.m_length= htonl(12+file_length);
                    write_num=write(socket_fd,&FILE_DATA,12);
                    if(write_num<12){printf("write number is:%d,less than 12\n",write_num);}


                    memset(popen_cmd,0,2500);
                    memcpy(popen_cmd,"cat ",4);
                    memcpy(popen_cmd+4,file_name,file_name_offset);
                    fp= popen(popen_cmd,"r");
                    char file_buf[2000];
                    int ret=0;
                    do {
                        ret=fread(file_buf,1,2000,fp);
                        if(ret==0){
                            if(MYTEST){ printf("finish putting\n");}
                            break;
                        }
                        write_num=write(socket_fd,file_buf,ret);
                        if(write_num<ret){printf("write number is:%d,less than ret\n",write_num);}

                    } while (1);
                }
                else{
                    if(MYTEST){ printf("no such file\n");}
                }
                break;
            }
            case 'q':{
                if(connect_success==0){ printf("haven't connected\n");break;}

                write_num= write(socket_fd,&QUIT_REQUEST,12);
                if(write_num<12){printf("write number is:%d,less than 12\n",write_num);}

                read(socket_fd,&QUIT_REPLY,12);
                if(strncmp(myProtocol,QUIT_REPLY.m_protocol,6)!=0){printf("protocol different:%s\n",GET_REPLY.m_protocol);break;}
                if(QUIT_REPLY.m_type!=(char)0xAC){ printf("QUIT_REPLY type error\n");break;}
                if (ntohl(QUIT_REPLY.m_length)!=12){printf("QUIT_REPLY length error\n");break;}
                sleep(1);
                close(socket_fd);
                break;
            }
            default:{
                printf("no such cmd\n");
                break;
            }
        }
        printf("Client> ");
    }
}

