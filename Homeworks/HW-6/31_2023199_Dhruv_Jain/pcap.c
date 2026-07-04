// Source: https://www.devdungeon.com/content/using-libpcap-c

#include <pcap.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <string.h>
#include <assert.h>

#include <stdio.h>
#include <netinet/ip.h>


// options points to the first byte of TCP header options
// len is the length of the TCP header options
// print TCP SACK acknowledgements in this routine
void print_tcp_options(unsigned char *options, size_t len)
{	
           size_t offset=0;
	unsigned char *ptr=options;
	uint8_t kind, lend;

	
	if (len==0) return;   // cant be empty 

	kind = (uint8_t)*ptr;       
	if (kind==0) return; // if it starts with 0 

	
	while (kind==1) {           // could just be a bunch of 0s
		if (offset+1>=len) return; 
		ptr++;
		offset++;
		kind=(uint8_t)*ptr;
	}
	if (kind==0) return; // could be 1s followed by 0

	
	if (offset+2>len) return;  // there should be stuff to read 
	lend =(uint8_t)*(ptr+1);

	
	while (kind!=0) {
		
		if (offset+1>len) break;   // always check if we are within len

		// if its 1 just go next byete
		if (kind==1) {
			
			if (offset+1>len) break;    // always check size
			ptr++;
			offset+=1;
			if (offset>=len) break;
			kind =(uint8_t)*ptr;



		      
			continue;
		}

	
		if (offset+ 2 >len) break; 
		  lend =(uint8_t)*(ptr+ 1); // get len if its in size 

		
		if(lend<2) break;                 // invalid length
		if (offset+lend>len) break;      // uk how it is 

		if (kind==5) {  // main stuff
			
			int num_blocks =(lend-2)/8;
			if (num_blocks > 0) {
				printf("SACK: ");
				for (int i = 0; i < num_blocks; i++) {
					uint32_t left_raw, right_raw;
					
					memcpy(&left_raw,  ptr + 2 + i * 8,     sizeof(uint32_t));
					memcpy(&right_raw, ptr + 2 + i * 8 + 4, sizeof(uint32_t));
					unsigned int left_edge  = ntohl(left_raw);
					unsigned int right_edge = ntohl(right_raw);
					printf("[%u, %u] ", left_edge, right_edge);
				}
				printf("\n");
			}
			
			ptr +=lend;  // go next kind 
			offset+= lend;

			// allways with the len checks
			if (offset >= len) break;
			kind = (uint8_t)*ptr;
			
			continue;
		} else {
			
			printf("Option kind=%u, len=%u\n", kind, lend);  // for printing all other kinds which were 2 3 4 8

			ptr+=lend;  // go next 
			offset+=lend;

		
			if (offset>=len)break;  // never forget about within len 
			kind =(uint8_t)*ptr;
			
			continue;
		}
	}
}

void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
	struct ethhdr *eth;
	eth = (struct ethhdr *) packet;
	if (ntohs(eth->h_proto) != ETHERTYPE_IP) {
		return;
	}

	struct iphdr *iph = (struct iphdr*)((char*)eth + sizeof(struct ethhdr));

	if (iph->protocol != IPPROTO_TCP) {
		// not a TCP packet
		return;
	}
	struct tcphdr *tcp = (struct tcphdr*)((char*)iph + (iph->ihl * 4));
	int length = tcp->doff * 4;
	if (length > 20) {
		unsigned char *options = (((unsigned char*)tcp) + 20);
		print_tcp_options(options, length - 20);
	}
}

int main(int argc, char *argv[])
{
	pcap_t *handle;			/* Session handle */
	char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
	pcap_if_t *alldevs;
	pcap_if_t *d;

	// Find all devices
	if (pcap_findalldevs(&alldevs, errbuf) == -1) {
		fprintf(stderr, "Error finding devices: %s\n", errbuf);
		return 1;
	}

	if (alldevs == NULL) {
		fprintf(stderr, "no available device\n");
		return 1;
	}

	// Print the list
	printf("Available network interfaces:\n");
	for (d = alldevs; d != NULL; d = d->next) {
		printf("%s", d->name);
		if (d->description) {
			printf(" - %s\n", d->description);
		}
		else {
			printf(" - No description available\n");
		}
	}
	/* Define the device */
	handle = pcap_open_live(alldevs->name, BUFSIZ, 1, 1000, errbuf);
	if (handle == NULL) {
		fprintf(stderr, "Couldn't open default device: %s\n", alldevs->name);
		pcap_freealldevs(alldevs);
		return(2);
	}
	printf("Could open: %s\n", alldevs->name);

	pcap_loop(handle, 0, packet_handler, NULL);

	/* And close the session */
	pcap_close(handle);
	pcap_freealldevs(alldevs);
	return(0);
}
