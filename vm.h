#ifndef _VM_H_
#define _VM_H_

#include "vm2sim.h"
#include "block.h"

#define MAX_MSG 128
typedef uint64_t message_type;
typedef struct _message_format {
  message_type size;
  message_type command;
  message_type timestamp;
  message_type node;
  union {
    // CMD_CREATE
    struct {
      message_type num;
      message_type start;
    } create;
	//CMD_SEND_MESSAGE
	struct{
		message_type face;
		message_type dest_node;
	}send_message;
	
    // CMD_COLOR
    struct {
      message_type r;
      message_type g;
      message_type b;
      message_type i;
    } color;

    // CMD_ADD_NBR
    struct {
      message_type nid;
      message_type face;
    } addNeighbor;

    // CMD_ADD_NCOUNT
    // CMD_DEL_NCOUNT
    struct {
      message_type total;
    } neighborCount;

    // CMD_HAS_RUN
    struct {
      message_type num;
      message_type other[];
    } runtil;

    // CMD_ADD_VACANT
    // CMD_DEL_VACANT
    struct {
      message_type face;
    } vacant;

    // CMD_NBR_DEL
    struct {
      message_type face;
    } delNeighbor;

    message_type units[MAX_MSG];

  } data;
} message;  

void vmInit(char* port);
void msg2vm(Block* dest, VMCommand cmd, Time timestamp, ...);

#endif


// Local Variables:
// tab-width: 4
// indent-tabs-mode: nil
// End:
