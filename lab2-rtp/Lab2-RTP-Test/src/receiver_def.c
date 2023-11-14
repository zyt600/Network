#include "receiver_def.h"
#include "util.h"
#include "rtp.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include<stdlib.h>
#include <fcntl.h>
#include <signal.h>


static int receiver_window_size;




static struct timeval zerotime;
static struct timeval timeout10second;
static int receiver_succ_connect=0,reveiver_seq_num;
static struct sockaddr_in senderaddr,receiveraddr;

static int receiver_socket_fd;
static int *recved_pkt=0/**用于记录某pkt是否被收到过*/;
static rtp_packet_t *cache_pkt=NULL/**用于缓存收到的包*/;
static int now_Iwant=0;

#define MYTEST 0
#define NOT_IMPORTANT_TEST 0
#define SPECIFFIC 0

void receiver_initialize(){
    timeout10second.tv_sec=10;
    timeout10second.tv_usec=0;

    zerotime.tv_usec=0;
    zerotime.tv_sec=0;
}


/**
 * 释放所有malloc的内存，关闭socket
 */
void receiver_freeall(){
 
    sleep(3);
    free(recved_pkt);
    free(cache_pkt);
    close(receiver_socket_fd);
 
}


/**
 * @brief 开启receiver并在所有IP的port端口监听等待连接
 *
 * @param port receiver监听的port
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 */
int initReceiver(uint16_t port, uint32_t window_size){
    receiver_initialize();
    receiver_window_size=window_size;
    recved_pkt= malloc(sizeof (int)*window_size);
    memset(recved_pkt,0,sizeof (int)*window_size);
 

    cache_pkt= malloc(sizeof (rtp_packet_t)*window_size);
    memset(cache_pkt,0,sizeof (rtp_packet_t)*window_size);

    receiver_socket_fd= socket(AF_INET, SOCK_DGRAM, 0);
    //将这个socket设为10秒超时
    int set_ret=setsockopt(receiver_socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout10second,sizeof(timeout10second));
 


    bzero(&receiveraddr, sizeof(receiveraddr));
    receiveraddr.sin_family=AF_INET;
    receiveraddr.sin_addr.s_addr= htonl(INADDR_ANY);

    receiveraddr.sin_port= htons(port);


    rtp_header_t receive_head,answer_head;

    int bind_ret=bind(receiver_socket_fd, (struct sockaddr *)&receiveraddr, sizeof (receiveraddr));


    //出错点：int n= recvfrom(sender_socket_fd,&receive_head,sizeof (receive_head),0,&senderaddr,sizeof (senderaddr));
    //改正后的正确写法
    //出错点2：sender_addr_len要初始化才能传进recvfrom
    unsigned int sender_addr_len=sizeof (senderaddr);
    int n= recvfrom(receiver_socket_fd, &receive_head, sizeof (receive_head), 0, (struct sockaddr *)&senderaddr, &sender_addr_len);
 

    if(n<0){
 
        return -1;
    }

    if(receive_head.length==0&&receive_head.type==RTP_START) {
        int temp_checksum=receive_head.checksum;
 
        receive_head.checksum=0;

        if(temp_checksum != compute_checksum(&receive_head,sizeof(receive_head))){
 
            return -1;
        }
        else{//一切ok，连接建立
 
//            reveiver_seq_num=receive_head.seq_num;
            answer_head.type=RTP_ACK;
            answer_head.seq_num=receive_head.seq_num;
            answer_head.length=0;
            answer_head.checksum=0;
            answer_head.checksum= compute_checksum(&answer_head,sizeof (answer_head));

            sendto(receiver_socket_fd, &answer_head, sizeof (answer_head), 0, (const struct sockaddr *) &senderaddr, sizeof (senderaddr));
            receiver_succ_connect=1;
            return 0;
        }
    }else{
 
        return -1;
    }
}


/**
 * @brief 用于接收数据并在接收完后断开RTP连接
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int recvMessage(char* filename){
    int write_fd,recv_num,total_recv_num=0,send_num;
    write_fd= open(filename,O_CREAT|O_RDWR|O_TRUNC,0777);
    rtp_packet_t buf_pkt;
    while (1){
        recv_num= recvfrom(receiver_socket_fd,&buf_pkt,sizeof (buf_pkt),0,NULL,NULL);
 
        if(recv_num<0){//应该是超时了
 
            return -1;
        }
        switch (buf_pkt.rtp.type) {
            case RTP_DATA:{
                //type正确
                int tempchecksum=buf_pkt.rtp.checksum;
                buf_pkt.rtp.checksum=0;
                if(tempchecksum != compute_checksum(&buf_pkt,buf_pkt.rtp.length+sizeof (rtp_header_t))){
                    
                    continue;
                }
                else{//checksum正确
                    if(buf_pkt.rtp.seq_num<now_Iwant || buf_pkt.rtp.seq_num>=now_Iwant+receiver_window_size){
 
                        continue;
                    }//seq_num正确
                    if(buf_pkt.rtp.length<0||buf_pkt.rtp.length>PAYLOAD_SIZE){
 
                        continue;
                    }//length正确
                    int index=buf_pkt.rtp.seq_num%receiver_window_size;

                    //分别记录收到包 和 缓存下来包
                    recved_pkt[index]=1;
                    memcpy(cache_pkt+index,&buf_pkt,sizeof (buf_pkt));
                    total_recv_num+=buf_pkt.rtp.length;

                    int check_whatIwant_idx= now_Iwant % receiver_window_size;
                    while (recved_pkt[check_whatIwant_idx]){
                        recved_pkt[check_whatIwant_idx]=0;
                        int writenum=write(write_fd, cache_pkt[check_whatIwant_idx].payload, cache_pkt[check_whatIwant_idx].rtp.length);
 
                        now_Iwant++;
                        check_whatIwant_idx= now_Iwant % receiver_window_size;
                    }

                    rtp_header_t data_ack;
                    data_ack.type=RTP_ACK;
                    data_ack.length=0;
                    data_ack.seq_num=now_Iwant;
                    data_ack.checksum=0;
                    data_ack.checksum= compute_checksum(&data_ack,sizeof (data_ack));

                    send_num= sendto(receiver_socket_fd,&data_ack,sizeof (data_ack),0,(const struct sockaddr *)&senderaddr, sizeof (senderaddr));
 

                }
                break;
            }
            case RTP_END:{//TODO 不应该在这里
 
                int tempchecksum=buf_pkt.rtp.checksum;
                buf_pkt.rtp.checksum=0;
                if(tempchecksum!= compute_checksum(&buf_pkt.rtp,sizeof (rtp_header_t))){
 
                    continue;
                }
                if(buf_pkt.rtp.length!=0 ||buf_pkt.rtp.seq_num!=now_Iwant){
 
                    //continue还是break，感觉倒也不是太重要
                    continue;
                } else{
 
                    return total_recv_num;
                }
                break;
            }
            default:{
 
 
                continue;
            }
        }
    }
    return 0;
}



/**
 * @brief 用于接收数据并在接收完后断开RTP连接 (优化版本的RTP)
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */

int recvMessageOpt(char* filename){
    int write_fd,recv_num,total_recv_num=0,send_num;
    write_fd= open(filename,O_CREAT|O_RDWR|O_TRUNC,0777);
    rtp_packet_t buf_pkt;
    while (1){
        recv_num= recvfrom(receiver_socket_fd,&buf_pkt,sizeof (buf_pkt),0,NULL,NULL);
 
        if(recv_num<0){//应该是超时了
 
            return -1;
        }

        switch (buf_pkt.rtp.type) {
            case RTP_DATA:{
                //杂七杂八验证
                {
                    //type正确
                    int tempchecksum=buf_pkt.rtp.checksum;
                    buf_pkt.rtp.checksum=0;
                    if(tempchecksum != compute_checksum(&buf_pkt,buf_pkt.rtp.length+sizeof (rtp_header_t))){
                     
                        continue;
                    }//checksum正确
                    if(buf_pkt.rtp.length<0||buf_pkt.rtp.length>PAYLOAD_SIZE){
 
                        continue;
                    }//length正确
                }
                if(buf_pkt.rtp.seq_num>=now_Iwant+receiver_window_size){
 
                    continue;
                }//seq_num正确

                //****************加进来的nb思路，确实应该这么做且之前没想到
                if(buf_pkt.rtp.seq_num<now_Iwant){
                    rtp_header_t data_ack;
                    data_ack.type=RTP_ACK;
                    data_ack.length=0;
                    data_ack.seq_num=buf_pkt.rtp.seq_num;
                    data_ack.checksum=0;
                    data_ack.checksum= compute_checksum(&data_ack,sizeof (data_ack));
                    send_num= sendto(receiver_socket_fd,&data_ack,sizeof (data_ack),0,(const struct sockaddr *)&senderaddr, sizeof (senderaddr));
 
                    continue;
                }
                //****************


                int index=buf_pkt.rtp.seq_num%receiver_window_size;

                //分别记录收到包 和 缓存下来包
                recved_pkt[index]=1;
                memcpy(cache_pkt+index,&buf_pkt,sizeof (buf_pkt));
                total_recv_num+=buf_pkt.rtp.length;

                int check_whatIwant_idx= now_Iwant % receiver_window_size;
                while (recved_pkt[check_whatIwant_idx]){
                    recved_pkt[check_whatIwant_idx]=0;
                    int writenum=write(write_fd, cache_pkt[check_whatIwant_idx].payload, cache_pkt[check_whatIwant_idx].rtp.length);
 
                    now_Iwant++;
                    check_whatIwant_idx= now_Iwant % receiver_window_size;
                }

                rtp_header_t data_ack;
                data_ack.type=RTP_ACK;
                data_ack.length=0;
                data_ack.seq_num=buf_pkt.rtp.seq_num;
                data_ack.checksum=0;
                data_ack.checksum= compute_checksum(&data_ack,sizeof (data_ack));

                send_num= sendto(receiver_socket_fd,&data_ack,sizeof (data_ack),0,(const struct sockaddr *)&senderaddr, sizeof (senderaddr));
 

                break;
            }
            case RTP_END:{//TODO 不应该在这里
 
                int tempchecksum=buf_pkt.rtp.checksum;
                buf_pkt.rtp.checksum=0;
                if(tempchecksum!= compute_checksum(&buf_pkt.rtp,sizeof (rtp_header_t))){
 
                    continue;
                }
                if(buf_pkt.rtp.length!=0 ||buf_pkt.rtp.seq_num!=now_Iwant){
 
                    //continue还是break，感觉倒也不是太重要
                    continue;
                } else{
 
                    return total_recv_num;
                }
                break;
            }
            default:{
 
 
                continue;
            }
        }
    }
    return 0;
}



/**
 * @brief 用于接收数据失败时断开RTP连接以及关闭UDP socket
 */
void terminateReceiver() {
 
    rtp_header_t end_ack;
    end_ack.type=RTP_ACK;
    end_ack.length=0;
    end_ack.seq_num=now_Iwant;
    end_ack.checksum=0;
    end_ack.checksum= compute_checksum(&end_ack,sizeof (end_ack));

    int send_num=sendto(receiver_socket_fd, &end_ack, sizeof (end_ack), 0,
                    (const struct sockaddr *) &senderaddr, sizeof (senderaddr));
 
    receiver_freeall();
    return;
}

