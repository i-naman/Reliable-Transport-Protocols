#include "../include/simulator.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
/* @author Naman Agrawal <namanagr@buffalo.edu> */

using namespace std;

/* TIMEOUT_TIMEUNITS is constant for a set of expirements */
#define TIMEOUT_TIMEUNITS 30.0 // 20.0, 30.0, 40.0, 50.0, 100.0

/* generic structure for entity (A/B) state */
struct entity
{
	int seq; // sequence number
	int ack; // acknowledgment number
	int baseIndex; // base index of window
	int windowSize; // window size
	int idx; //
};
struct entity entity_A; // A
struct entity entity_B; // B

// buffer to store incoming msgs from layer 5
// (only 1 buffer implemented for unidirectional transfer of data from the A-side to the B-side)
vector<struct msg> messagesBuffer;

/* helper method to compute checksum */
/* INPUT: requires pointer to packet for which checksum is to be computed */
/* OUTPUT: computed checksum */
int get_checksum(struct pkt *packet)
{
    int localsum = 0;
	int seqnum = packet->seqnum;
	int acknum = packet->acknum;
	int payloadsize = sizeof(packet->payload);

    localsum += seqnum;
    localsum += acknum;
    for (int i = 0; i < payloadsize; i++) {
		localsum += (packet->payload)[i];
	}
	// localsum = ~localsum; //bitswise NOT (optional logic)
    return localsum;
}

/* helper method to create data packet from message received from layer5 */
/* INPUT: message from layer5, entity which requested (A or B) */
/* OUTPUT: pointer to created packet */
struct pkt *create_data_packet(struct msg message, struct entity host)
{
    struct pkt *packet = new struct pkt;
    packet->seqnum = host.seq;
    packet->acknum = host.ack;
    strcpy(packet->payload, message.data);
    packet->checksum = get_checksum(packet);
    return packet;
}

/* helper method to create ACK / NAK packets */
/* INPUT: ack/nak number */
/* OUTPUT: pointer to created packet */
struct pkt *create_ack_packet(int ack_number)
{
    struct pkt *packet = new struct pkt;
    packet->acknum = ack_number;
    packet->checksum = get_checksum(packet);
    return packet;
}

/* helper method to access messagesBuffer and pass packets to layer3 */
/* INPUT: pointer to incoming messsage or isInterrupt flag */
void util_A_send_packets(struct msg *message, bool isInterrupt)
{
	if (isInterrupt) {
		// reset seq to base index of window
		entity_A.seq = entity_A.baseIndex;
	} else {
		// add incoming message to buffer
		messagesBuffer.push_back(*message);
	}
	// loop to send next seq if seq is in window and is messagesBuffer
	for (; entity_A.seq < (entity_A.baseIndex + entity_A.windowSize) && entity_A.seq < messagesBuffer.size(); entity_A.seq++) {
		struct msg message = messagesBuffer[entity_A.seq];
		struct pkt *packet = create_data_packet(message, entity_A);
		tolayer3(0, *packet);
		if(entity_A.baseIndex == entity_A.seq) starttimer(0, TIMEOUT_TIMEUNITS);
	}
	return;
}


/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	util_A_send_packets(&message, false);
	return;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	if (packet.checksum == get_checksum(&packet)) { // if valid
		entity_A.baseIndex = packet.acknum + 1;
		stoptimer(0);
		if(entity_A.baseIndex != entity_A.seq) {
			starttimer(0, TIMEOUT_TIMEUNITS);
		}
	}
	return;
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	util_A_send_packets(NULL, true);
	return;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
	entity_A.seq = 0;
	entity_A.ack = 0;
	entity_A.baseIndex = 0;
	entity_A.windowSize = getwinsize();
	entity_A.idx = 0;
	return;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{	
	if (packet.seqnum == entity_B.ack && packet.checksum == get_checksum(&packet)) { // if valid
		tolayer5(1, packet.payload);
		struct pkt *ack_packet = create_ack_packet(entity_B.ack);
		tolayer3(1, *ack_packet);
		entity_B.ack++;
	}
	return;
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	entity_B.seq = 0; // unused as B is not transmitting app layer data to A
	entity_B.ack = 0;
	entity_B.baseIndex = 0;
	entity_B.windowSize = getwinsize();
	entity_B.idx = 0;
	return;
}
