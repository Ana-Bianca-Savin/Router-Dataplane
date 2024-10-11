#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include "trie.h"

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <math.h>

//  Search routing table to get LPM
struct route_table_entry *get_best_route(TTrie trie, uint32_t ip_dest) {
	return trie_search(trie, ip_dest);
}

//  Get arp entry
struct arp_table_entry *get_arp_entry(struct arp_table_entry *arp_table, int arp_table_len, uint32_t given_ip) {
	for (int i = 0; i < arp_table_len; i++)
		if (arp_table[i].ip == given_ip)
			return &arp_table[i];
	return NULL;
}

//  Send ICMP
void send_icmp_packet(int type, void *buf, int interface) {	
	//  Allocate a new packet
	void *packet = malloc(sizeof(struct ether_header) + 2 * sizeof(struct iphdr) + sizeof(struct icmphdr) + 8);

	//  Get information to edit
	struct ether_header *eth_hdr = (struct ether_header *)packet;
	struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));
	struct icmphdr *icmp_hdr = (struct icmphdr *)(packet + sizeof(struct ether_header) + sizeof(struct iphdr));

	//  Get useful information
	struct ether_header *eth_hdr_to_extract = (struct ether_header *)buf;
	struct iphdr *ip_hdr_to_extract = (struct iphdr *)(buf + sizeof(struct ether_header));


	//  Edit ether header

	//  Source address and destination address switch places
	//  in ether header
	eth_hdr->ether_type = htons(0x0800);
	memcpy(eth_hdr->ether_shost, eth_hdr_to_extract->ether_dhost, 6);
	memcpy(eth_hdr->ether_dhost, eth_hdr_to_extract->ether_shost, 6);

	//  Ip header
	ip_hdr->saddr = inet_addr(get_interface_ip(interface));
	ip_hdr->daddr = ip_hdr_to_extract->saddr;
	ip_hdr->ttl = 64;
	ip_hdr->ihl = 5;
	ip_hdr->version = 4;
	ip_hdr->tos = 0;
	ip_hdr->tot_len = htons(2 * sizeof(struct iphdr) + sizeof(struct icmphdr) + 8);
	ip_hdr->id = htons(1);
	ip_hdr->frag_off = 0;
	ip_hdr->protocol = 1;

	//  Prepare icmp header
	icmp_hdr->type = type;
	icmp_hdr->code = 0;

	memcpy((void *)icmp_hdr + sizeof(struct icmphdr), ip_hdr_to_extract, sizeof(struct iphdr) + 8);

	//  Calculate checksum in icmp
	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr) + sizeof(struct iphdr) + 8));

	//  Calculate checksum in ip header
	ip_hdr->check = 0;
	ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, 2 * sizeof(struct iphdr) + sizeof(struct icmphdr) + 8));

	//  Send icmp packet
	send_to_link(interface, (char *)packet, sizeof(struct ether_header) + 2 * sizeof(struct iphdr) + sizeof(struct icmphdr) + 8);
}

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argc - 2, argv + 2);

	// Allocate route table
	struct route_table_entry *rtable = malloc(sizeof(struct route_table_entry) * 100000);
	DIE(rtable == NULL, "memory");

	//  Allocate MAC table
	struct arp_table_entry *arp_table = malloc(sizeof(struct arp_table_entry) * 100000);
	DIE(arp_table == NULL, "memory");

	// Read the static routing table and arp table
	int rtable_len = read_rtable(argv[1], rtable);

	//  Initialize and build the prefix trie
	TTrie trie = create_node();
	for (int i = 0; i < rtable_len; i++) {
		trie_insert(trie, rtable + i);
	}

	int arp_table_len = 0;

	//  Queue for packages that are waiting for arp operations
	struct queue *q = queue_create();
	int nr_elem_queue = 0;

	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buf;
		/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
		if (ntohs(eth_hdr->ether_type) == 0x0800) {
			// IPv4
			printf("IPv4 packet\n");

			//  Get ip header from buf
			struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));

			//  Tranform ip_hrd->daddr to char*
			uint8_t *ip_as_bytes = (uint8_t *)&ip_hdr->daddr;
			char ip_as_string[20];
			snprintf(ip_as_string, 20, "%d.%d.%d.%d", ip_as_bytes[0], ip_as_bytes[1], ip_as_bytes[2], ip_as_bytes[3]);

			//  Determine if the packet is for the router or not
			int is_for_router = 0;
			char *interface_ip = get_interface_ip(interface);

			if (strcmp(interface_ip, ip_as_string) == 0)
				is_for_router = 1;

			if (is_for_router) {
				//  ICMP
				printf("Package is for this router\n");

				//  Initialize ICMP header
				struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));

				//  Source address and destination address switch places
				//  in ether header
				char mac[6];
				memcpy(mac, eth_hdr->ether_shost, 6);
				memcpy(eth_hdr->ether_shost, eth_hdr->ether_dhost, 6);
				memcpy(eth_hdr->ether_dhost, mac, 6);

				//  and in ip header
				uint32_t aux = ip_hdr->saddr;
				ip_hdr->saddr = ip_hdr->daddr;
				ip_hdr->daddr = aux;

				//  Prepare icmp header
				icmp_hdr->type = 0;
				icmp_hdr->code = 0;
				//  Calculate checksum
				icmp_hdr->checksum = 0;
				icmp_hdr->checksum = htons(checksum((uint16_t *)icmp_hdr, sizeof(struct icmphdr)));

				//  Calculate checksum
				ip_hdr->check = 0;
				ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

				//  Send icmp packet
				send_to_link(interface, (char *)buf, len);

				continue;
			} else {
				//  Recalculate the checksum to see if it is correct or not
				uint16_t temp_checksum = ip_hdr->check;
				ip_hdr->check = 0;

				if (temp_checksum != ntohs(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)))) {
					//  Checksum is wrong
					printf("Wrong checksum\n");
					continue;
				} else {
					printf("Correct checksum\n");
				}

				//  Check and update TTL
				if (ip_hdr->ttl == 0 || ip_hdr->ttl == 1) {
					//  TTL exceeded
					printf("TTL exceeded\n");

					//  Send time exceeded icmp
					send_icmp_packet(11, buf, interface);

					continue;
				} else {
					//  Update the TTL
					printf("TTL was updated\n");
					ip_hdr->ttl--;

					//  Recalculate checksum
					ip_hdr->check = ~(~temp_checksum + ~(ip_hdr->ttl + 1) + (uint16_t)ip_hdr->ttl) - 1;
				}

				//  Search in the routing table for the entry with LPM
				struct route_table_entry *entry = get_best_route(trie, ip_hdr->daddr);
				if (entry == NULL) {
					//  No match in routing table
					printf("Destination unreachable\n");

					//  Send destination unreachable icmp
					send_icmp_packet(3, buf, interface);

					continue;
				}


				//  We need to modify source and destination addresses ethernet header

				//  Modify the source address to the one that belongs to the correct
				//  interface of the router
				get_interface_mac(entry->interface, eth_hdr->ether_shost);

				//  Search in arp table
				struct arp_table_entry *arp_entry = get_arp_entry(arp_table, arp_table_len, entry->next_hop);
				if (arp_entry == NULL) {
					//  If arp_entry is null, add the package to queue
					void *buf_copy = malloc(len);
					memcpy(buf_copy, buf, len);
					queue_enq(q, buf_copy);

					//  Increment number of packages
					nr_elem_queue++;

					//  Send an ARP request
					void *packet = malloc(sizeof(struct ether_header) + sizeof(struct arp_header));
					DIE(packet == NULL, "memory");

					//  Get ether header and initialize it
					struct ether_header *eth_hdr_send = (struct ether_header *) packet;
					memcpy(eth_hdr_send->ether_shost, eth_hdr->ether_shost, 6);
					memset(eth_hdr_send->ether_dhost, 0xff, 6);
					eth_hdr_send->ether_type = htons(0x0806);

					//  Get arp header and initialize it
					struct arp_header *arp_hdr_send = (struct arp_header *)(packet + sizeof(struct ether_header));
					arp_hdr_send->htype = htons(1);
					arp_hdr_send->ptype = htons(0x0800);
					arp_hdr_send->hlen = 6;
					arp_hdr_send->plen = 4;
					arp_hdr_send->op = htons(1);
					memcpy(arp_hdr_send->sha, eth_hdr_send->ether_shost, 6);
					arp_hdr_send->spa = inet_addr(get_interface_ip(entry->interface));
					memset(arp_hdr_send->tha, 0x00, 6);
					arp_hdr_send->tpa = entry->next_hop;

					//  Send arp request
					send_to_link(entry->interface, (char *)eth_hdr_send, sizeof(struct ether_header) + sizeof(struct arp_header));

					continue;
				}


				//  Modify the destination address to the new mac
				memcpy(eth_hdr->ether_dhost, arp_entry->mac, 6);

				//  Send the packet to the next hop
				send_to_link(entry->interface, (char *)buf, len);
			}

		} else if (ntohs(eth_hdr->ether_type) == 0x0806) {
			//  ARP
			printf("ARP packet\n");

			//  Get arp header from buf
			struct arp_header *arp_hdr = (struct arp_header *)(buf + sizeof(struct ether_header));

			//  Check if the operation is an arp request or reply
			if (ntohs(arp_hdr->op) == 1) {
				//  Request
				printf("Received ARP request\n");

				//  ARP header
				//  Target address is now source address
				memcpy(arp_hdr->tha, arp_hdr->sha, 6);

				//  Source address will be the mac address of
				//  interface where request was received
				get_interface_mac(interface, arp_hdr->sha);

				//  Reverse ips
				uint32_t aux = arp_hdr->spa;
				arp_hdr->spa = arp_hdr->tpa;
				arp_hdr->tpa = aux;

				//  We need to send a reply
				arp_hdr->op = htons(2);


				//  Ethernet header
				//  We need to send the new packet back to the sender
				//  Therefore, the destionation mac will be the original sender's one
				memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);

				//  Sender mac will be mac of interface where request was received
				memcpy(eth_hdr->ether_shost, arp_hdr->sha, 6);

				//  Send reply
				send_to_link(interface, (char *)eth_hdr, sizeof(struct ether_header) + sizeof(struct arp_header));

			} else if (ntohs(arp_hdr->op) == 2) {
				//  Reply
				printf("Received ARP reply\n");

				//  Initialize a new entry for arp table
				struct arp_table_entry new_arp_entry;
				new_arp_entry.ip = arp_hdr->spa;
				memcpy(new_arp_entry.mac, arp_hdr->sha, 6);

				//  Append new entry to table and increment number of elements
				arp_table[arp_table_len++] = new_arp_entry;

				//  Iterate over all elements in queue
				int final_cnt = nr_elem_queue;
				for (int i = 0; i < nr_elem_queue; i++) {
					//  Extract an element
					void *packet = queue_deq(q);

					//  Get ether header
					struct ether_header *eth_hdr2 = (struct ether_header *) packet;

					//  Get ip header from eather header
					struct iphdr *ip_hdr2 = (struct iphdr *)(packet + sizeof(struct ether_header));

					//  Search in the routing table for the entry with LPM
					struct route_table_entry *entry2 = get_best_route(trie, ip_hdr2->daddr);

					//  Search in the routing table for the entry with LPM
					struct arp_table_entry *arp_entry2 = get_arp_entry(arp_table, arp_table_len, entry2->next_hop);

					if (arp_entry2 == NULL) {
						//  Mac is unknown
						//  Adding packet back to queue
						queue_enq(q, packet);
					} else {
						//  Decrement number of final packets that are in queue
						final_cnt--;

						//  Modify the destination address to the mac found in arp table
						memcpy(eth_hdr2->ether_dhost, arp_entry2->mac, 6);

						//  Modify the source address to the mac address of next hop
						get_interface_mac(entry2->interface, eth_hdr2->ether_shost);

						//  Send the packet to the next hop
						send_to_link(entry2->interface, packet, sizeof(struct ether_header) + ntohs(ip_hdr2->tot_len));
					}
				}

				nr_elem_queue = final_cnt;
			}
		}


	}
}

