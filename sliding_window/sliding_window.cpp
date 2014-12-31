#include <cstdlib>
#include <string.h>
#include "sysinclude.h"
#include <deque>
using namespace std;


extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

typedef enum {data, ack, nak} frame_kind;
typedef struct frame_head
{
	frame_kind kind;
	unsigned int seq;
	unsigned int ack;
	unsigned char data[100];
};

typedef struct frame
{
	frame_head head;
	unsigned int size;
};

struct Node
{
	frame* cont;
	unsigned int size;
};

deque<Node> mQue;
bool isFull = false;

int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType)
{
	if (messageType == MSG_TYPE_TIMEOUT){
		unsigned int num = ntohl(*(unsigned int*)pBuffer);
		if (isFull){
			Node cur = mQue.front();
			int seq = cur.cont->head.seq;
			if (num == (unsigned int)seq){
				SendFRAMEPacket((unsigned char*)(cur.cont), cur.size);
			}
		}
	}
	else if (messageType == MSG_TYPE_SEND){
		Node p;
		p.cont = new frame;
		*(p.cont) = *(frame*)pBuffer;
		p.size = bufferSize;
		mQue.push_back(p);
		if (!isFull){
			Node first = mQue.front();
			SendFRAMEPacket((unsigned char*)(first.cont), first.size);
			isFull = true;
		}
	}
	else if (messageType == MSG_TYPE_RECEIVE){
		unsigned int ack = ntohl(((frame*)pBuffer)->head.ack);
		if (isFull){
			Node cur = mQue.front();
			unsigned int num = ntohl(cur.cont->head.seq);
			if (num == ack){
				mQue.pop_front();
				if (mQue.size() != 0){
					Node first = mQue.front();
					SendFRAMEPacket((unsigned char*)(first.cont), first.size);
				}
				else {
					isFull = false;
				}
			}
		}
	}
	return 0;
}


deque<Node> mQue2;
int counter = 0;	
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType)
{
	if (messageType == MSG_TYPE_TIMEOUT){
		int num = ntohl(*(unsigned int*)pBuffer);
		int j = 0;
		for (j = 0; j < mQue2.size() && j < WINDOW_SIZE_BACK_N_FRAME; j ++){
			Node p = mQue2[j];
			if ((*p.cont).head.seq == num)
				break;
		}
		for (int k = 0; k < WINDOW_SIZE_BACK_N_FRAME && k < mQue2.size(); k ++){
			Node p = mQue2[k];
			SendFRAMEPacket((unsigned char*)(p.cont), p.size);
		}
	}

	else if (messageType == MSG_TYPE_SEND){
		Node p;
		p.cont = new frame;
		*(p.cont) = *(frame*)pBuffer;
		p.size = bufferSize;
		mQue2.push_back(p);
		if (counter < WINDOW_SIZE_BACK_N_FRAME){		
			Node cur = mQue2.back();	
			SendFRAMEPacket((unsigned char*)(cur.cont), cur.size);
			counter ++;
		}
	}
	else if (messageType == MSG_TYPE_RECEIVE){
		int ack = ((frame*)pBuffer)->head.ack;
		int j = 0;
		for (j = 0; j < mQue2.size() && j < WINDOW_SIZE_BACK_N_FRAME; j ++){
			Node p = mQue2[j];
			int num = p.cont->head.seq;
			if (ack == num){
				break;
			}
		}
		if (j < mQue2.size() && j < WINDOW_SIZE_BACK_N_FRAME){
			for (int k = 0; k <= j; k ++){
				mQue2.pop_front();
				counter --;
			}
		}
		for (int i = counter; i < mQue2.size()&& i < WINDOW_SIZE_BACK_N_FRAME &&
			counter < WINDOW_SIZE_BACK_N_FRAME; i ++){
				Node p = mQue2[i];
				SendFRAMEPacket((unsigned char*)(p.cont), p.size);
				counter ++;
		}
	}
	return 0;
}

/*
* 选择性重传测试函数
*/
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType)
{
	return 0;
}





