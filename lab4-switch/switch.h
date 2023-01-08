#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include <set>
#include <vector>
#include<iostream>

class SwitchBase {
 public:
  SwitchBase() = default;
  ~SwitchBase() = default;

  virtual void InitSwitch(int numPorts) = 0;
  virtual int ProcessFrame(int inPort, char* framePtr) = 0;
};

extern SwitchBase* CreateSwitchObject();

// TODO : Implement your switch class.
class switch_entry{
  public:
    int port,counter;
    mac_addr_t mac_addr;
    switch_entry(int p,mac_addr_t m,int c=10){
      port=p;
      counter=c;
      memcpy(mac_addr,m,sizeof(mac_addr_t));
    }
    bool operator==(mac_addr_t m){
      if(memcmp(m,mac_addr,sizeof(mac_addr_t))==0)
        return true;
      return false;  
    }
    bool operator==(switch_entry b){
      if(memcmp(mac_addr,b.mac_addr,sizeof(mac_addr_t))==0)
        return true;
      return false;
    }
    bool operator<(mac_addr_t m){
      return memcmp(mac_addr,m,ETH_ALEN);
    }
    bool operator<(switch_entry b){
      return memcmp(mac_addr,b.mac_addr,ETH_ALEN);
    }
};

class my_switch:public SwitchBase{
private:
  /* data */
  //注意：与Lab3一致，我们约定端口号从1开始进行编号，特别地，每个交换机的1号端口均与Controller相链接。
  int port_num;
  // set<switch_entry> switch_table;
  std::vector<switch_entry> switch_table;
public:
  // my_switch(/* args */);
  // ~my_switch();
  void InitSwitch(int numPorts){
    port_num=numPorts;
  }
  /*
  inPort:表示收到的帧的入端口号
  framePrt:是一个指针，表示收到的帧
  ProcessFrame的返回值表示的是这个帧应当被转发到的端口号
  */
  int ProcessFrame(int inPort, char* framePtr){
    ether_header_t head;
    memcpy(&head,framePtr,sizeof(ether_header_t));
    switch (head.ether_type)
    {
      case ETHER_CONTROL_TYPE:{
        /* code */
        for(auto i=switch_table.begin();i!=switch_table.end();++i)
          i->counter--;
        while (1){
          for(auto i=switch_table.begin();i!=switch_table.end();++i)
            if(i->counter<=0){
              switch_table.erase(i);
              break;
            }
          break;    
        }
        return -1;
        // break;
      }
      case ETHER_DATA_TYPE:{
        int success_find=0;
        for(auto i=switch_table.begin();i!=switch_table.end();++i){
          if(*i==head.ether_src){
            i->counter=ETHER_MAC_AGING_THRESHOLD;
            success_find=1;
            break;
          }
        }
        if(!success_find){
          switch_entry entryy(inPort,head.ether_src);
          switch_table.push_back(entryy);
        }
        for(auto i=switch_table.begin();i!=switch_table.end();++i){
          if(*i==head.ether_dest){
            return i->port;
          }
        }
        return 0;
        // break;
      }
      default:
        std::cout<<"type errorrr";
        break;
    }
  }
};

// my_switch::my_switch(/* args */)
// {
// }
// my_switch::~my_switch()
// {
// }



#endif  // ! COMPNET_LAB4_SRC_SWITCH_H
