#include "sysinclude.h"
extern void rip_sendIpPkt(unsigned char *pData, UINT16 len, unsigned short dstPort, UINT8 iNo);
extern struct stud_rip_route_node *g_rip_route_table;
const unsigned short DSTPORT = 520;         //路由表项的返回端口
const int MAX_METRIC = 16;
struct RIPHeader
{
    UINT8 command;
    UINT8 version;
    UINT16 must_be_zero;
};
struct RIPEntry
{
    UINT16 addr_family_identifier;
    UINT16 route_tag;
    UINT32 ip_Addr;
    UINT32 subnet_mask;
    UINT32 next_hop;
    UINT32 metric;
    void copyFrom(RIPEntry tmp){
        addr_family_identifier = tmp.addr_family_identifier;
        route_tag = tmp.route_tag;
        ip_Addr = tmp.ip_Addr;
        subnet_mask = tmp.subnet_mask;
        next_hop = tmp.next_hop;
        metric = tmp.metric;
    }
};
struct RIPv2
{
    RIPHeader header;
    RIPEntry entries[25];
};

//将本地的路由表整理出来，response or 广播，返回包的大小
UINT16 select_routes(RIPv2* res, UINT8 iNo){
    (res->header).command = 2;
    (res->header).version = 2;
    (res->header).must_be_zero = 0;
    int cnt = 0;    //路由表项数
    struct stud_rip_route_node* p = g_rip_route_table;
    while (p != NULL){
        if (p->if_no != iNo){
            res->entries[cnt].addr_family_identifier = htons(2);
            res->entries[cnt].route_tag = 0;
            res->entries[cnt].ip_Addr = htonl(p->dest);
            res->entries[cnt].subnet_mask = htonl(p->mask);
            res->entries[cnt].next_hop = htonl(p->nexthop);
            res->entries[cnt].metric = htonl(p->metric);
            cnt ++;
        }
        p = p -> next;
    }
   
    return (4 + 20 * cnt);
}

//通过ip和掩码找路由信息
struct stud_rip_route_node* find(unsigned int ip_Addr, unsigned int mask){
    struct stud_rip_route_node* p = g_rip_route_table;
    bool flag = 0;
    while (p != NULL){
   if (p->dest == ip_Addr && p->mask == mask){
            return p;
        }
        p = p -> next;
    }
    return NULL;
}


int stud_rip_packet_recv(char *pBuffer, int bufferSize, UINT8 iNo, UINT32 srcAdd)
{
    //有效性检查
    RIPHeader* buffer = (RIPHeader*)pBuffer;
    if (buffer->version != 2){      //版本错误
        ip_DiscardPkt(pBuffer, STUD_RIP_TEST_VERSION_ERROR);
        return -1;
    }
    if (buffer->command != 1 && buffer->command != 2){      //命令错误
        ip_DiscardPkt(pBuffer, STUD_RIP_TEST_COMMAND_ERROR);
        return -1;
    }
   
    //收到一个request，返回response
    if (buffer->command == 1){
        RIPv2* res =  new RIPv2();
        UINT16 len = select_routes(res, iNo);
        rip_sendIpPkt((unsigned char*)res, len, DSTPORT, iNo);
    }
    //收到一个response
    else if (buffer->command == 2){
        RIPEntry* p = (RIPEntry*)(pBuffer + 4);
        for(int i = 0; i < 25; i ++){
            if (p[i].ip_Addr == 0) break;
            RIPEntry* entry = new RIPEntry();       //将网络字节序转变主机字节序
            entry->ip_Addr = ntohl(p[i].ip_Addr);
            entry->subnet_mask = ntohl(p[i].subnet_mask);
            entry->metric = ntohl(p[i].metric);
            stud_rip_route_node* tmp = find(entry->ip_Addr, entry->subnet_mask);  
            if (!tmp){    //该路由表项为新表项，将其加到队首
                tmp = new stud_rip_route_node();
                tmp->dest = entry->ip_Addr;
                tmp->mask = entry->subnet_mask;
                tmp->nexthop = srcAdd;
                tmp->metric = entry->metric + 1;
                if (tmp->metric > MAX_METRIC){
                    tmp->metric = MAX_METRIC;
                }
                tmp->if_no = iNo;
                tmp->next = g_rip_route_table;
                g_rip_route_table = tmp;
            }
            else if (tmp->nexthop == srcAdd) {
                tmp->metric = entry->metric + 1;
                if (tmp->metric > MAX_METRIC){
                    tmp->metric = MAX_METRIC;
                }
                tmp->if_no = iNo;
            }
            else {
                if (entry->metric < tmp->metric){
                    tmp->metric = entry->metric + 1;  //如果正好相等是会更新的
                    tmp->nexthop = srcAdd;
                    tmp->if_no = iNo;
                }
            }
        }
    }
                   
    return 0;
}
void stud_rip_route_timeout(UINT32 destAdd, UINT32 mask, unsigned char msgType)
{
    UINT16 len;
    switch(msgType){
    case RIP_MSG_SEND_ROUTE:    //广播路由信息
        for (int iNo = 1; iNo <= 2; iNo ++){
            RIPv2* tmp = new RIPv2();
            len = select_routes(tmp, iNo);
            rip_sendIpPkt((unsigned char*)tmp, len, DSTPORT, iNo);
        }
        break;
    case RIP_MSG_DELE_ROUTE:    //超时
    {
        stud_rip_route_node *p = find(destAdd, mask);
        if (p){
            p->metric = MAX_METRIC;
        }
        break;
    }
    default:
        break;
    }
}
 
