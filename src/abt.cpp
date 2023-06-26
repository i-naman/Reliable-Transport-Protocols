#include "../include/simulator.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>

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
#define TIMEOUT_TIMEUNITS 15.0 // 10.0, 15.0, 20.0, 25.0

/* generic structure for entity (A/B) state */
struct entity
{
    int seq; // sequence number -> alt bit protocol implementation (usage values: 0,1) 
    int ack; // acknowledgment number
	bool readyReceiveLayer5; // ready to receive layer5 data
	struct pkt lastSentPacket; // store packet state for possible retransmission
};
struct entity entity_A; // A
struct entity entity_B; // B

// queue to store incoming msgs from layer 5
// (only 1 queue implemented for unidirectional transfer of data from the A-side to the B-side)
queue<struct msg> messagesQueue;

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

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	if (entity_A.readyReceiveLayer5 != true) {
		// if entity_A is waiting for ack, then queue/delay incoming message from layer5
		messagesQueue.push(message);
		return;
	} else {
		// if entity_A is ready to receive message from layer5, then create packet and pass to layer3
		struct pkt *curr_packet = create_data_packet(message, entity_A);
		entity_A.readyReceiveLayer5 = false;
		entity_A.lastSentPacket = *curr_packet;
		tolayer3(0, entity_A.lastSentPacket);
		starttimer(0, TIMEOUT_TIMEUNITS);
		return;
	}
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	if (packet.acknum == entity_A.seq && packet.checksum == get_checksum(&packet)) { // if packet is valid
		stoptimer(0);
		entity_A.seq = !(entity_A.seq); // toggle entity_A seq (alternating bit)
		entity_A.readyReceiveLayer5 = true; // set entity_A ready to recieve from layer5
		if (!messagesQueue.empty()) {
			//if queue not empty, pop front msg and pass for processing
			struct msg next_msg = messagesQueue.front();
			messagesQueue.pop();
			A_output(next_msg);
		}
	}
	return;
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	// restransmit last sent packet on timer interrupt
	// cout << "time" << time << "\n";
	entity_A.readyReceiveLayer5 = false;
	tolayer3(0, entity_A.lastSentPacket);
	starttimer(0, TIMEOUT_TIMEUNITS);
	return;
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
	// initialiaze state of entity_A to start receiving data from layer5
	entity_A.seq = 0;
	entity_A.ack = 0;
	entity_A.readyReceiveLayer5 = true;
	return;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
	if (packet.seqnum == entity_B.ack && packet.checksum == get_checksum(&packet)) { // if packet is valid
		tolayer5(1, packet.payload); // pass payload to layer5
		struct pkt *ack_packet = create_ack_packet(entity_B.ack); // create ack packet
		tolayer3(1, (*ack_packet)); // pass ack packet to layer3
		entity_B.ack = !(entity_B.ack); //toggle ack (alternating bit)
	} else {
		int nak = !(entity_B.ack); // nak is incorrect ack...
		struct pkt *nack_packet = create_ack_packet(nak); // create nak packet
		tolayer3(1, (*nack_packet)); // pass nak packet to layer3
	}
	return;
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	// initialiaze state of entity_B
	entity_B.seq = 0; // unused as B is not transmitting app layer data to A
	entity_B.ack = 0;
	entity_B.readyReceiveLayer5 = true; // unused as B is not transmitting app layer data to A
	return;
}
