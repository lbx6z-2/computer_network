#include "sysinclude.h"
extern void rip_sendIpPkt(unsigned char *pData, UINT16 len, unsigned short dstPort, UINT8 iNo);
extern struct stud_rip_route_node *g_rip_route_table;

const unsigned short DSTPORT = 520;         //路由表项的返回端口
const int ENTRY_MAX = 25;

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
    RIPEntry* entries;
};


UINT16 select_routes(RIPv2* res, UINT8 iNo){
    res.command = (UINT8)2;
    res.version = (UINT8)2;
    res.must_be_zero = (UINT16)0;

    int cnt = 0;    //路由表项数
    struct stud_rip_route_node* p = g_rip_route_table;

    RIPEntry tmp[25];   //因为不确定路由项到底有多少

    while (p != NULL && cnt < ENTRY_MAX){
        if (p->if_no != iNo){
            tmp[cnt].addr_family_identifier = htons(2);
            tmp[cnt].route_tag = 0;
            tmp[cnt].ip_Addr = htonl(p->dest);
            tmp[cnt].subnet_mask = htonl(p->mask);
            tmp[cnt].next_hop = htonl(p->nexthop);
            tmp[cnt].metric = htonl(p->metric);
            cnt ++;
        }
        p = p -> next;
    }
    
    //将tmp copy到res中
    res->entries = new RIPEntry[cnt];
    for (int i = 0; i < cnt; i ++){
        res->entries[i].copyFrom(tmp[i]);
    }
    
    return (4 + 20 * cnt);
}


stud_rip_route_node* find(UINT32 ip_Addr){
    struct stud_rip_route_node* p = g_rip_route_table;
    bool flag = 0;
    while (p != NULL){
        if (ip_addr == p->dest){
            return p;
        }
        p = p -> next;
        cnt ++;
    }
    return null;
}

void del(stud_rip_route_node* tmp){
    struct stud_rip_route_node* p = g_rip_route_table;
    if (p->dest == tmp->dest){
        g_rip_route_table = g_rip_route_table->next;
    }
    while (p->next != NULL){
        if (tmp->dest == (p->next)->dest){
            p->next = (p->next)->next;
            break;
        }
        p = p -> next;
    }
}


int stud_rip_packet_recv(char *pBuffer, int bufferSize, UINT8 iNo, UINT32 srcAdd)
{
    //有效性检查
    RIPHeader* buffer = (RIPHeader*)pBuffer;
    if (buffer.version != 2){
        rip_sendIpPkt(pBuffer, STUD_RIP_TEST_VERSION_ERROR);
        return -1;
    }
    if (buffer.command != 1 || buffer.command != 2){
        rip_sendIpPkt(pBuffer, STUD_RIP_TEST_COMMAND_ERROR);
        return -1;
    }
    
    //收到一个request，返回response
    if (buffer.command == 1){
        RIPv2* res =  new RIPv2();
        UINT16 len = select_routes(res, iNo);
        rip_sendIpPkt((unsigned char*)res, len, DSTPORT, iNo);
    }
    //收到一个response
    else if (buffer.command == 2){
        RIPEntry* p = (RIPEntry*)(pBuffer + 4);
        int len = bufferSize - 4;
        int cnt = 0;
        while (cnt < len){
            RIPEntry* entry = new RIPEntry();       //将网络字节序转变主机字节序
            entry->ip_Addr = p[cnt].ip_Addr;
            entry->subnet_mask = p[cnt].subnet_mask;
            entry->next_hop = p[cnt].next_hop;
            entry->metric = p[cnt].metric;
            stud_rip_route_node* tmp = find(entry->ip_addr);   

            if (!tmp){    //该路由表项为新表项，将其加到队首
                tmp = new stud_rip_route_node();
                tmp->dest = entry->ip_Addr;
                tmp->mask = entry->subnet_mask;
                tmp->nexthop = entry->next_hop;
                tmp->metric = entry->metric + 1;
                tmp->if_no = iNo;
                if (tmp->metric < 16){
                    tmp->next = g_rip_route_table;
                    g_rip_route_table = tmp;
                }
            }
            else if (tmp->nexthop == entry->next_hop){
                tmp->metric = entry->metric + 1;
                if (tmp->metric >= 16){
                    del(tmp);
                }
            }
            else if (tmp->nexthop != entry->next_hop){
                if (entry->metric < tmp->metric){
                    tmp->metric = entry->metric + 1;
                    tmp->nexthop = entry->next_hop;
                }
            }              
            cnt += 20;
        }
    }
                    
    return 0;
}








