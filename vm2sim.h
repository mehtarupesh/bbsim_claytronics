#ifndef _VM2SIM_H_
#define _VM2SIM_H_

#define SETID 1
#define RUN_NODE 2
#define NODE_RUN 3
#define STOP 4
#define ADD_NEIGHBOR 5
#define REMOVE_NEIGHBOR 6
#define TAP 7
#define SET_COLOR 8
#define USE_THREADS 9
#define CREATE_N_NODES 18
#define SEND_MESSAGE 12
#define RECEIVE_MESSAGE 13


// My names for the vm<->sim msgs

#define CMD_SETID SETID 
#define CMD_CREATE CREATE_N_NODES
#define CMD_HAS_RUN NODE_RUN
#define CMD_RUN RUN_NODE
#define CMD_TAP	TAP
#define CMD_ADD_NBR ADD_NEIGHBOR
#define CMD_COLOR SET_COLOR
#define CMD_DEL_NBR REMOVE_NEIGHBOR
#define CMD_MODE_ND USE_THREADS
#define CMD_SENDMSG SEND_MESSAGE 
#define CMD_RECVMSG RECEIVE_MESSAGE
#define CMD_ADD_NCOUNT 10
#define CMD_DEL_NCOUNT 11
#define CMD_ADD_VACANT 16
#define CMD_DEL_VACANT 17
// types?

typedef int VMCommand;

#endif

// Local Variables:
// tab-width: 4
// indent-tabs-mode: nil
// End:
