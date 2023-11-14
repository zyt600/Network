#include "sender_def.h"
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
#include <sys/time.h>
#include <signal.h>

#define MYTEST 0
#define NOT_IMPORTANT_TEST 0
#define SPECIFFIC 0

static struct itimerval break_100ms,init_break_time;
static struct timeval timeout,zerotime;
static int sender_succ_connect=0,windows_size/**窗口大小*/,next_seq_num=0/**下一个包（如果存在）的序号*/;
static int sender_socket_fd;
static struct sockaddr_in receiver_addr;
static rtp_packet_t *all_pkt=NULL;
static int *recv_ack;
static int win_start_position=0/**最近一个没有收到的包*/;

/**
 * @brief 定时处理函数，到时间重发所有窗口内的包裹
 */
void send_all_pkt(){
    for(int i=0; i < windows_size; i++) {
        int send_num=sendto(sender_socket_fd,all_pkt+i, sizeof(rtp_header_t)+all_pkt[i].rtp.length, 0,(const struct sockaddr *) &receiver_addr, sizeof (receiver_addr));
    }
}

/**
 * @brief 优化的定时处理函数，到时间重发窗口内没有被ack的包裹
 */
void send_opt_pkt(){
    for(int i=0; i < windows_size; i++) {
        if(recv_ack[i]==0){
            int send_num=sendto(sender_socket_fd,all_pkt+i, sizeof(rtp_header_t)+all_pkt[i].rtp.length, 0,(const struct sockaddr *) &receiver_addr, sizeof (receiver_addr));
        }
    }
}


void initialize(){
    init_break_time.it_interval.tv_sec=0;
    init_break_time.it_interval.tv_usec=0;
    init_break_time.it_value.tv_usec=100000;
    init_break_time.it_value.tv_sec=0;

    break_100ms.it_value.tv_sec=0;
    break_100ms.it_value.tv_usec=100000;
    break_100ms.it_interval.tv_sec=0;
    break_100ms.it_interval.tv_usec=100000;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    zerotime.tv_usec=0;
    zerotime.tv_sec=0;
}

/**
 * 释放所有malloc的内存，关闭socket
 */
void sender_freeall(){
    sleep(3);
    free(all_pkt);
    free(recv_ack);
    close(sender_socket_fd);
}

int initSender_timebreak_handler_seqnum;
void initSender_timebreak_handler(){
    rtp_header_t end_mesg;
    end_mesg.type=RTP_END;
    end_mesg.length=0;
    end_mesg.seq_num=initSender_timebreak_handler_seqnum;
    end_mesg.checksum=0;
    end_mesg.checksum= compute_checksum(&end_mesg,sizeof end_mesg);
    int sendnum=sendto(sender_socket_fd, &end_mesg, sizeof (end_mesg), 0, (const struct sockaddr *)&receiver_addr, sizeof (receiver_addr));
    return;
}


/**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size){
    initialize();
 
    windows_size=(int)window_size;
    all_pkt= malloc(sizeof (rtp_packet_t)*window_size);
    memset(all_pkt,0,sizeof (rtp_packet_t)*window_size);
    for(int i=0;i<window_size;i++){
        all_pkt[i].rtp.type=RTP_DATA;
    }


    sender_socket_fd= socket(AF_INET, SOCK_DGRAM, 0);


    bzero(&receiver_addr, sizeof (receiver_addr));
    receiver_addr.sin_family=AF_INET;
    receiver_addr.sin_port= htons(receiver_port);
    inet_pton(AF_INET,receiver_ip,&receiver_addr.sin_addr);

    rtp_header_t try_connect,receive_head;
    try_connect.type=RTP_START;
    try_connect.seq_num=rand();//虽然是没用时间初始化的伪随机数，不过倒也无所谓
    initSender_timebreak_handler_seqnum=try_connect.seq_num;
    try_connect.length=0;
    try_connect.checksum=0;
    try_connect.checksum= compute_checksum(&try_connect,sizeof (try_connect));
 

    //尝试不用setitimer(),而是用改变socket状态，从阻塞到定时阻塞
    int set_ret=setsockopt(sender_socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 
    //嗷，不出意外的话，此后的sender_socket_fd都是定时阻塞了

    int send_num=sendto(sender_socket_fd, &try_connect, sizeof (try_connect), 0, (struct sockaddr *)&receiver_addr, sizeof (receiver_addr));

 


    int n =recvfrom(sender_socket_fd, &receive_head, sizeof (receive_head), 0, NULL, NULL);


 
    if(n<0){
        initSender_timebreak_handler();
        return -1;
    }
    if(receive_head.length==0&&receive_head.type==RTP_ACK&&receive_head.seq_num==try_connect.seq_num){
        //我编的checksum，不知道对不对
        int temp_checksum=receive_head.checksum;
        receive_head.checksum=0;
        if(temp_checksum != compute_checksum(&receive_head,sizeof(receive_head))){
 
            return -1;
        }
        else{//一切ok，连接建立
 
            sender_succ_connect=1;
            return 0;
        }
    } else{
 
        return -1;
    }

}



/**
 * @brief 用于发送数据
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessage(const char* message){

    next_seq_num=0;
    if(sender_succ_connect==0){
 
        return -1;
    }
    char tmpcmd[1000];
    memset(tmpcmd,0,sizeof (tmpcmd));
    memcpy(tmpcmd,"du -b ",6);
    memcpy(tmpcmd+6,message, strlen(message));
    FILE *tmpfp;
    tmpfp= popen(tmpcmd,"r");
    long filelen=-999;
    fscanf(tmpfp,"%ld",&filelen);
 
    fclose(tmpfp);

    char cmd[1000];
    memset(cmd,0,sizeof (cmd));
    memcpy(cmd,"cat ",4);
    memcpy(cmd+4,message, strlen(message));


    FILE *fp;
    fp= popen(cmd,"r");
    char read_buf[PAYLOAD_SIZE];
    int readnum,finish_reading=0,finish_ack=0,total_send_num=0;

    int whether_print=0;
    int temp=0;
    for(int i=0; i < windows_size; temp++,i++)//初次填充所有包并发送
    {
        readnum=fread(read_buf,1,PAYLOAD_SIZE,fp);
        if(readnum<0){
 
            break;
        }else if(readnum==0){
            finish_reading=1;
 
            break;
        }
        total_send_num+=readnum;
        if(((whether_print++)%30==0)&&MYTEST){
 
        }
        if(MYTEST){("sss::first readnum:%d,seqnum:%d\n",readnum,next_seq_num);}
        int index= next_seq_num % windows_size;
        memset(all_pkt[index].payload,0,PAYLOAD_SIZE);
        memcpy(all_pkt[index].payload,read_buf,readnum);

        all_pkt[index].rtp.seq_num=next_seq_num;
        all_pkt[index].rtp.length=readnum;
        all_pkt[index].rtp.checksum=0;
        all_pkt[index].rtp.checksum= compute_checksum(all_pkt+index,sizeof(rtp_header_t)+all_pkt[index].rtp.length);
        next_seq_num++;

        int send_num=sendto(sender_socket_fd,all_pkt+index,sizeof(rtp_header_t)+all_pkt[index].rtp.length, 0, (const struct sockaddr *)&receiver_addr,sizeof (receiver_addr));
 
    }

 
 
    recv_ack= malloc(sizeof (int) * windows_size);
    memset(recv_ack , 0, sizeof (int) * windows_size);


    //在这里把状态从  定时阻塞  改回  阻塞
    int set_ret=setsockopt(sender_socket_fd,SOL_SOCKET,SO_RCVTIMEO,&zerotime,sizeof (zerotime));
 
    //此后的sender_socket_fd都是 阻塞 了


//    signal(SIGVTALRM,send_all_pkt);//并安装信号处理函数
//    setitimer(ITIMER_VIRTUAL, &break_100ms, NULL);//将计时器的形式改为每100ms发一个SIGALARM
    signal(SIGALRM,send_all_pkt);//并安装信号处理函数
    setitimer(ITIMER_REAL, &break_100ms, NULL);//将计时器的形式改为每100ms发一个SIGALARM

    while (finish_ack==0){
        rtp_header_t recvhead;
 
        int recvnum= recvfrom(sender_socket_fd,&recvhead,sizeof (recvhead),0,NULL,NULL);
   
        if(recvnum<0){//超时了，执行超时处理（全部重发
 
            send_all_pkt();
            continue;
        } else if(recvnum!=sizeof (recvhead))//recvnum异常
        {
 
            continue;
        }

 
        int temp_checksum= recvhead.checksum;
        recvhead.checksum=0;
        if(temp_checksum!= compute_checksum(&recvhead,sizeof (recvhead))){
 
            continue;
        } else if(recvhead.length!=0||recvhead.type!=RTP_ACK){
 
            continue;
        } else if(recvhead.seq_num<win_start_position||recvhead.seq_num>next_seq_num){
 
            continue;
        }


        for(int i=win_start_position;i<recvhead.seq_num;i++){
            recv_ack[i%windows_size]=1;
        }
        //所有发送都包都被ack，都结束了
        if(recvhead.seq_num==next_seq_num && finish_reading==1){
 
            finish_ack=1;
            setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
            break;
        }

        while (1){
                //这个idx决定了哪个包应当被重新填充并发送
                int idx= win_start_position % windows_size;

                if(recv_ack[idx]==1 && win_start_position<next_seq_num){
                    setitimer(ITIMER_REAL, &break_100ms, NULL);//重置计时器
 
                    recv_ack[idx]=0;

                    if(finish_reading==0)//填充第idx个packet并发送
                    {
                        readnum = fread(read_buf,1,PAYLOAD_SIZE,fp);
                        if(readnum<0){
 
                            return -1;
                        }else if(readnum==0){
 
                            finish_reading=1;
                            /////////////////////////
                            /////////////////////////
                            /////////////////////////
                            /////////////////////////
                            if(recvhead.seq_num==next_seq_num && finish_reading==1){
                                finish_ack=2;
                                setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
 
                                break;
                            }
                            break;
                        }

                        memset(all_pkt[idx].payload,0,PAYLOAD_SIZE);
                        memcpy(all_pkt[idx].payload,read_buf,readnum);
                        all_pkt[idx].rtp.seq_num=next_seq_num;
                        all_pkt[idx].rtp.length=readnum;
                        all_pkt[idx].rtp.checksum=0;
                        all_pkt[idx].rtp.checksum= compute_checksum(all_pkt+idx,sizeof (rtp_header_t )+all_pkt[idx].rtp.length);
                        next_seq_num++;

                        int send_num=sendto(sender_socket_fd,all_pkt+idx, all_pkt[idx].rtp.length+sizeof (rtp_header_t), 0, (const struct sockaddr *)&receiver_addr,sizeof (receiver_addr));
 
                        total_send_num+=send_num;
                        if(((whether_print++)%30==0)&&SPECIFFIC){
 
                        }
                    }

                    win_start_position++;
                    if(win_start_position==next_seq_num){
                        finish_ack=3;
 
                        break;
                    }
                } else{
                    break;
                }
            }

    }
 

    setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
    fclose(fp);
    return 0;
}



/**
 * @brief 用于发送数据 (优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/

int sendMessageOpt(const char* message){
    next_seq_num=0;
    if(sender_succ_connect==0){
 
        return -1;
    }
    char tmpcmd[1000];
    memset(tmpcmd,0,sizeof (tmpcmd));
    memcpy(tmpcmd,"du -b ",6);
    memcpy(tmpcmd+6,message, strlen(message));
    FILE *tmpfp;
    tmpfp= popen(tmpcmd,"r");
    long filelen=-999;
    fscanf(tmpfp,"%ld",&filelen);
 
    fclose(tmpfp);

    char cmd[1000];
    memset(cmd,0,sizeof (cmd));
    memcpy(cmd,"cat ",4);
    memcpy(cmd+4,message, strlen(message));


    FILE *fp;
    fp= popen(cmd,"r");
    char read_buf[PAYLOAD_SIZE];
    int readnum,finish_reading=0,finish_ack=0,total_send_num=0;

    int whether_print=0;
    int temp=0;
    for(int i=0; i < windows_size; temp++,i++)//初次填充所有包并发送
    {
        readnum=fread(read_buf,1,PAYLOAD_SIZE,fp);
        if(readnum<0){
 
            break;
        }else if(readnum==0){
            finish_reading=1;
 
            break;
        }
        total_send_num+=readnum;
        if(((whether_print++)%30==0)&&MYTEST){
 
        }
 
        int index= next_seq_num % windows_size;
        memset(all_pkt[index].payload,0,PAYLOAD_SIZE);
        memcpy(all_pkt[index].payload,read_buf,readnum);

        all_pkt[index].rtp.seq_num=next_seq_num;
        all_pkt[index].rtp.length=readnum;
        all_pkt[index].rtp.checksum=0;
        all_pkt[index].rtp.checksum= compute_checksum(all_pkt+index,sizeof(rtp_header_t)+all_pkt[index].rtp.length);
        next_seq_num++;

        int send_num=sendto(sender_socket_fd,all_pkt+index,sizeof(rtp_header_t)+all_pkt[index].rtp.length, 0, (const struct sockaddr *)&receiver_addr,sizeof (receiver_addr));
 
    }

 
 
    recv_ack= malloc(sizeof (int) * windows_size);
    memset(recv_ack , 0, sizeof (int) * windows_size);

    //在这里把状态从  定时阻塞  改回  阻塞
    int set_ret=setsockopt(sender_socket_fd,SOL_SOCKET,SO_RCVTIMEO,&zerotime,sizeof (zerotime));
 
    //此后的sender_socket_fd都是 阻塞 了

//    signal(SIGVTALRM,send_opt_pkt);//并安装信号处理函数
//    setitimer(ITIMER_VIRTUAL, &break_100ms, NULL);//将计时器的形式改为每100ms发一个SIGALARM
    signal(SIGALRM,send_opt_pkt);//并安装信号处理函数
    setitimer(ITIMER_REAL, &break_100ms, NULL);//将计时器的形式改为每100ms发一个SIGALARM

    while (finish_ack==0){//在循环内发完所有包，收ack

        rtp_header_t recvhead;
 
        int recvnum= recvfrom(sender_socket_fd,&recvhead,sizeof (recvhead),0,NULL,NULL);

        //杂七杂八验证
        {
            if(recvnum<0){//超时了，执行超时处理（全部重发
                send_opt_pkt();
                continue;
            } else if(recvnum!=sizeof (recvhead))//recvnum异常
            {
                continue;
            }

 
            int temp_checksum= recvhead.checksum;
            recvhead.checksum=0;
            if(temp_checksum!= compute_checksum(&recvhead,sizeof (recvhead))){
 
                continue;
            } else if(recvhead.length!=0||recvhead.type!=RTP_ACK){
 
                continue;
            } else if(recvhead.seq_num<win_start_position||recvhead.seq_num>next_seq_num){
 
                continue;
            }
        }

        recv_ack[recvhead.seq_num%windows_size]=1;

        //****************这里错了，有可能还有中间的包没ack呢
//        //所有发送都包都被ack，都结束了
//        if(recvhead.seq_num==next_seq_num-1 && finish_reading==1){
 
//            finish_ack=1;
//            setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
//            break;
//        }
        if(finish_reading==1){
            int i=win_start_position;
            for(;i<next_seq_num;i++){
                if(recv_ack[i%windows_size]==0)
                    break;
            }
            if(i==next_seq_num){
 
                finish_ack=1;
                setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
                break;
            }
        }
 

        int idx= win_start_position % windows_size; //这个idx决定了哪个包应当被重新填充并发送

        while (recv_ack[idx]==1 && win_start_position<next_seq_num){//在循环内做到：把收到的能连成一片的都++
            setitimer(ITIMER_REAL, &break_100ms, NULL);//重置计时器
 
            recv_ack[idx]=0;

            if(finish_reading==0)//填充第idx个packet并发送
            {
                readnum = fread(read_buf,1,PAYLOAD_SIZE,fp);
                if(readnum<0){
 
                    return -1;
                }else if(readnum==0){
 
                    finish_reading=1;

                    if(recvhead.seq_num==next_seq_num-1/*显而易见：&& finish_reading==1*/){
                        finish_ack=2;
                        setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
 
                    }
                    break;
                }

                memset(all_pkt[idx].payload,0,PAYLOAD_SIZE);
                memcpy(all_pkt[idx].payload,read_buf,readnum);
                all_pkt[idx].rtp.seq_num=next_seq_num;
                all_pkt[idx].rtp.length=readnum;
                all_pkt[idx].rtp.checksum=0;
                all_pkt[idx].rtp.checksum= compute_checksum(all_pkt+idx,sizeof (rtp_header_t )+all_pkt[idx].rtp.length);
                next_seq_num++;

                int send_num=sendto(sender_socket_fd,all_pkt+idx, all_pkt[idx].rtp.length+sizeof (rtp_header_t), 0, (const struct sockaddr *)&receiver_addr,sizeof (receiver_addr));
 
                total_send_num+=send_num;
                if(((whether_print++)%30==0)&&SPECIFFIC){
 
                }
            }

            win_start_position++;
            idx= win_start_position % windows_size;
            if(win_start_position==next_seq_num){
                finish_ack=3;
 
                break;
            }
        }

    }
 

    setitimer(ITIMER_REAL, NULL, NULL);//取消计时器
    fclose(fp);
    return 0;
}

/**
 * @brief 用于断开RTP连接以及关闭UDP socket
 **/
void terminateSender(){
 
    rtp_header_t end_head,end_ack;
    end_head.type=RTP_END;
    end_head.length=0;
    end_head.seq_num=next_seq_num;
    end_head.checksum=0;
    end_head.checksum= compute_checksum(&end_head,sizeof (end_head));

    int send_num=sendto(sender_socket_fd, &end_head, sizeof (end_head), 0, (const struct sockaddr *)&receiver_addr, sizeof (receiver_addr));
 

    //重设收到END的计时器
    int set_ret=setsockopt(sender_socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 
    int read_num=recvfrom(sender_socket_fd,&end_ack,sizeof (end_ack),0,NULL,NULL);
 


    sender_freeall();


    if(read_num<0){//可谓是END超时
 
        return;
    }
 
    return;
//    if(end_ack.length==0&&end_ack.type==RTP_ACK&&end_ack.seq_num==next_seq_num){
//        int temp_checksum=end_ack.checksum;
//        end_ack.checksum=0;
//        if(temp_checksum!= compute_checksum(&end_ack,sizeof (end_ack))){
 
//            //TODO:怎么后续处理，真的要处理吗
//            return;
//        } else{//一切ok
//
//        }
//    } else{
 
//        //TODO:怎么后续处理
//        return;
//    }
}

