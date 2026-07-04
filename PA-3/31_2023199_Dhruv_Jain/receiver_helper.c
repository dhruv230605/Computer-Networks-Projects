
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "receiver.h"
#include "config.h"

#define MAX_BUF 10000
size_t old_app_len = 0

typedef struct {
    unsigned long long seqno; // first byte of packet
    unsigned char *data; // full packet (header + payload)
    size_t len; // full packet length
} paket;

static paket arr[MAX_BUF] = {0};
static unsigned long long recv_base = 0; //next expected byte, in order
static unsigned char *app_buf = NULL;
static size_t app_len = 0;

static int first_pkt = 1;
// received a packet, pkt
// len, length of the paket
void rdt_recv(const unsigned char *pkt, size_t len)
{
	// printf("pkt received seq=%llu len=%zu\n", get_seqno(pkt), len);
	unsigned long long seqno = get_seqno(pkt);
	if(first_pkt){
		recv_base = seqno;
		first_pkt =0 ;
	}
	const unsigned char* data = get_data(pkt);
	size_t data_len = len - PACKET_HEADER_LEN;
	if(seqno < recv_base){
		send_ack(recv_base);
		return;
	}
	int alr_have = 0;
	//check if we already have this packet
	for(int i=0; i<MAX_BUF; i++){
		if (arr[i].data!=NULL && arr[i].seqno==seqno){
			alr_have = 1;
			break;
		}
	}
	if(alr_have) goto deliver;

	//store anywhere you get space
	for(int i=0; i<MAX_BUF; i++){
		if(!arr[i].len){
			arr[i].seqno = seqno;
			arr[i].len = data_len;
			arr[i].data = malloc(data_len);
			memcpy(arr[i].data, data, data_len);
			break;
		}
	}
	size_t old_app_len = app_len;

	int yo;
	deliver:
		do{
			yo = 0;
			for(int i=0; i<MAX_BUF; i++){
				if(arr[i].data!=NULL && arr[i].seqno==recv_base){
					app_buf = realloc(app_buf, app_len+arr[i].len);
					memcpy(app_buf+app_len, arr[i].data, arr[i].len);
					app_len+=arr[i].len;
					recv_base+=arr[i].len;
					free(arr[i].data);
					arr[i].data = NULL;
					arr[i].len = 0;
					yo = 1;
				}
			}
		}while(yo);

	send_ack(recv_base);
	// printf("recv seq=%llu base=%llu app_len=%zu\n", seqno, recv_base, app_len);

	if (app_len > old_app_len) notify_app();

	// assert(0 && "Not Yet Implemented");
}

// app requested a data of length len
// buf is len bytes long
// returns the number of bytes copied to buf
size_t app_recv(unsigned char *buf, size_t len)
{
	size_t n = (len < app_len) ? len : app_len;
    if (n == 0)
        return 0;

    memcpy(buf, app_buf, n);
    memmove(app_buf, app_buf + n, app_len - n);
    app_len -= n;
    app_buf = realloc(app_buf, app_len);
    return n;
}
