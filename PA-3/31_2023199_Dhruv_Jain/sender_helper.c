
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "sender.h"
#include "config.h"

#define MAX_BUF 10000

typedef struct {
    unsigned long long seqno; // first byte of packet
    unsigned char *data; // full packet (header + payload)
    size_t len; // full packet length
} paket;

static paket arr[MAX_BUF] = {0};
static unsigned long long base = 0;
static unsigned long long nextseq = 0;
static int free_idx = 0; //can store new paket here.

// static unsigned long long last_acked = 0; // track last cumulative ACK
// send buffer, buf, of length len
void rdt_send(const void *buf, size_t len)
{
	// printf("base = %llu, nextseq = %llu\n", base, nextseq);
    make_pkt(buf, nextseq);
    udt_send(buf, len);

	assert(free_idx < MAX_BUF);
	arr[free_idx].seqno = nextseq;
	arr[free_idx].data = malloc(len);
	memcpy(arr[free_idx].data, buf, len);
	arr[free_idx].len = len;
    // start timer if this is the first unacked packet
    if (base == nextseq)
        start_timer();

    nextseq += (len - PACKET_HEADER_LEN);
	free_idx++;
}

// received an acknowledgment number, ackno
void rdt_recv_ack(unsigned long long ackno)
{
	if(ackno >= base){
		base = ackno;
		//delete acked packets
		int shift = 0;
		for(int i=0; i<free_idx; i++){
			if(arr[i].data && arr[i].seqno + (arr[i].len-PACKET_HEADER_LEN)<=ackno){
				free(arr[i].data);
				arr[i].data = NULL;
				arr[i].len =0;
				arr[i].seqno = 0;
				shift = 1;
			}
		}
		if(shift){
			int new_idx =0;
			for(int i=0; i<free_idx; i++){
				if(arr[i].data!=NULL){
					arr[new_idx++] = arr[i];
				}
			}
			free_idx = new_idx;
		}
		if(base!=nextseq){
			start_timer();
		}else{
			stop_timer();
		}
	}

	// assert(0 && "Not Yet Implemented");
}

// timeout event handler
void timeout()
{
	// printf("[SEND] timeout base=%llu next=%llu\n", base, nextseq);
	// if(free_idx > 0 && arr[0].data) udt_send(arr[0].data, arr[0].len);
	for(int i=0; i<free_idx; i++){
		if(arr[i].data && arr[i].seqno==base){
			udt_send(arr[i].data, arr[i].len);
			break;
		}
	}
	start_timer();
	// assert(0 && "Not Yet Implemented");
}
