#include "../include/simulator.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>
#include <vector>
#include <map>

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
#define TIMEOUT_TIMEUNITS 15.0 // 15.0, 20.0, 25.0, 30.0

/* generic structure for entity (A/B) state */
struct entity
{
    int seq; // sequence number -> alt bit protocol styles (usage values: 0,1) 
    int ack; // acknowledgment number
	int windowSize; // window size
    int baseIndex; // base index of window
};
struct entity entity_A; // A
struct entity entity_B; // B

/* custom structure msgs with isAckd flag */
struct buffer_msg
{
	char data[20];
	bool isAckd;
};
/* custom structure for logical timers */
struct seqtimers
{
	int seq; //seq num
	float time; //time of creation
};

// buffer to store incoming msgs from layer 5 for A
vector<struct buffer_msg> messagesBufferA;
// queue for logical seqtimers used by A
queue<struct seqtimers> seqTimersQueueA;
// map for packets in receiving window of B
map<int, struct pkt> receivedPacketsMapB;

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
struct pkt *create_data_packet(struct buffer_msg message, struct entity host, int seqnum)
{
    struct pkt *packet = new struct pkt;
    packet->seqnum = seqnum;
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

/* helper method to create custom buffer_msg struct with original msg data and a isAckd flag  */
/* INPUT: struct msg message */
/* OUTPUT: pointer to created buffer_msg */
struct buffer_msg *create_buffer_msg(struct msg message)
{
	struct buffer_msg *newMsg = new struct buffer_msg;
	newMsg->isAckd = false;
	strncpy(newMsg->data, message.data, sizeof(message.data));
    return newMsg;
}

/* helper method to hanlde logical seq timers */
void util_A_handle_logical_seqtimers()
{
    while (seqTimersQueueA.size() > 0 && seqTimersQueueA.size() <= entity_A.windowSize && messagesBufferA[seqTimersQueueA.front().seq].isAckd == true) {
        seqTimersQueueA.pop(); // clear corresponding seqTimers for ackd packets from queue
    }
    if (seqTimersQueueA.size() > 0 && seqTimersQueueA.size() <= entity_A.windowSize) {
        // start global timer if there are still logical timers to be handled
        starttimer(0, TIMEOUT_TIMEUNITS);
    }
	return;
}

/* helper method to access messagesBuffer and pass packets to layer3 */
/* INPUT: bool isInterrupt, int seq_num; seq_num to be passed along if isInterrupt */
void util_A_send_packets(bool isInterrupt, int seq_num)
{
	// if isInterrupt == true; use seq number passed with interrupt flag
	int localSeq = isInterrupt ? seq_num : entity_A.seq;
	if (!(localSeq >= entity_A.baseIndex && localSeq < entity_A.baseIndex + entity_A.windowSize)) {
		return; // do nothing and return if seq is out of sending window
	}

	// if seq is within sending window, create packet and pass to layer 3
	struct buffer_msg currMsg = messagesBufferA[localSeq];
	struct pkt *packet = create_data_packet(currMsg, entity_A, localSeq);
	tolayer3(0, *packet);

	if (!isInterrupt) entity_A.seq++; // increment seq num if not interrupt retrasnmit
	if (seqTimersQueueA.size() == 0) starttimer(0, TIMEOUT_TIMEUNITS); //  start global timer

	// create and push new logical seqTimer to queue
	struct seqtimers seqTimer;
	seqTimer.seq = localSeq;
	seqTimer.time = get_sim_time();
	seqTimersQueueA.push(seqTimer);
	return;
}

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	// create new buffer_msg time and add to buffer
	struct buffer_msg *newMsg = create_buffer_msg(message);
	messagesBufferA.push_back(*newMsg);
	// call utility function to check, process and send data if possible
	util_A_send_packets(false, 0);
	return;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	if (packet.checksum != get_checksum(&packet)) {
		return; // return if packet is NOT valid
	}
	int ack = packet.acknum;
	messagesBufferA[ack].isAckd = true; // set isAckd flag as true for corresponding msg
	if (ack == entity_A.baseIndex) {
    	// stoptimer(0);
		// loop to increment and move baseIndex to next unacked packet
		for (; entity_A.baseIndex < messagesBufferA.size() && messagesBufferA[entity_A.baseIndex].isAckd == true; entity_A.baseIndex++) {
			// seqTimersQueueA.pop(); // clear corresponding seqTimers from queue
		}
		// since packets have been ackd and baseIndex incremented, check if A is ready to send more msgs in the window range  
		while ((entity_A.seq < entity_A.baseIndex + entity_A.windowSize && entity_A.seq < messagesBufferA.size())) {
			util_A_send_packets(false, 0);
		}
	}
	if (ack == seqTimersQueueA.front().seq) {
    	stoptimer(0);
        seqTimersQueueA.pop();
        // clear corresponding seqTimers from queue for ackd packets
        util_A_handle_logical_seqtimers();
	}
	return;
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	// resend interrupted seq
    int seqToResend = seqTimersQueueA.front().seq;
    seqTimersQueueA.pop();
    util_A_handle_logical_seqtimers();
    util_A_send_packets(true, seqToResend);
    //
	return;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
	// initialiaze state of entity_A
	entity_A.seq = 0;
	entity_A.ack = 0;
	entity_A.baseIndex = 0;
	entity_A.windowSize = getwinsize();
	return;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    if (packet.checksum != get_checksum(&packet)) {
		return; // return if packet is NOT valid
	}
    // send ack back if packet is valid
    struct pkt *ack_packet = create_ack_packet(packet.seqnum);
    tolayer3(1, *ack_packet);

    // if received packet seq is in receiving window
	if (packet.seqnum >= entity_B.ack && packet.seqnum < entity_B.ack + entity_B.windowSize) {
		
		if (packet.seqnum == entity_B.ack) {
            // if received packet seq is base expected seq, pass to layer5
			tolayer5(1, packet.payload);
			entity_B.ack++;
			// check map recursively if next expected seq packet is present and pass to layer5
			map<int, struct pkt>::iterator iter;
			iter = receivedPacketsMapB.find(entity_B.ack);
			while (iter != receivedPacketsMapB.end()) {
				tolayer5(1, iter->second.payload);
				receivedPacketsMapB.erase(iter);
				entity_B.ack++;
				iter = receivedPacketsMapB.find(entity_B.ack);
			}
		} else {
			// else if received packet seq is base expected seq, add to received map to consume later
			receivedPacketsMapB[packet.seqnum] = packet;
		}
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
	entity_B.baseIndex = 0;
	entity_B.windowSize = getwinsize();
	return;
}
