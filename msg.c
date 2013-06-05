#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "myassert.h"
#include "block.h"
#include "sim.h"
#include "vm.h"

static int msgverbose = 0;

static int is_data_available(int sock);
static int force_read(int sock);

// convert # to str
char*
vmcmd2str(VMCommand cmd)
{
  static char buffer[128];

  switch(cmd) {
//  case CMD_CREATE:	return "create";
  case CMD_HAS_RUN:	return "ranfor";
  case CMD_RUN:	return "runtil";
  case CMD_ADD_NBR: return "addNbr";
  case CMD_DEL_NBR: return "delNbr";
  case CMD_COLOR: return "color";
  case CMD_TAP: return "tap";
  case CMD_MODE_ND: return "mode==nd";
  case CMD_ADD_NCOUNT: return "addNcnt";
  case CMD_DEL_NCOUNT: return "delNcnt";
  case CMD_ADD_VACANT: return "addVnct";
  case CMD_DEL_VACANT: return "delVnct";
  case STOP: return "stopvm";
  default:	sprintf(buffer, "??%d??", cmd); return buffer;
  }
  return "??";
}

static char*
msg2string(message* m, char* buffer)
{
  static char lbuffer[512];
  if (buffer == 0) buffer = lbuffer;
  sprintf(buffer, "%2d@%5d %6s %2d", (int)m->node, (int)m->timestamp, vmcmd2str(m->command), (int)m->size);
  if (m->size == 3*sizeof(message_type)) return buffer;
  char* p = buffer+strlen(buffer);
  switch (m->command) {
  case CMD_CREATE:
    sprintf(p, " num=%d id=%d", (int)m->data.create.num, (int)m->data.create.start);
    break;

  case CMD_HAS_RUN:
    {
      int numother = (int)m->data.runtil.num;
      sprintf(p, "comm with:%d%s", numother, numother == 0 ? "" : ":");
      p = p + strlen(p);
      int i;
      for (i=0; i<numother; i++) {
        sprintf(p, "%d%s", (int)m->data.runtil.other[i], (i+1)==numother?"":", ");
        p = p + strlen(p);
      }
    }
    break;

  case CMD_RUN:
    break;
    
  case CMD_ADD_VACANT:
  case CMD_DEL_VACANT:
    sprintf(p, " face=%s", faceToString((Face)m->data.vacant.face));
    break;

  case CMD_ADD_NCOUNT:
  case CMD_DEL_NCOUNT:
    sprintf(p, " count=%2d", (int)m->data.neighborCount.total);
    break;

  case CMD_ADD_NBR:
    sprintf(p, " neighbor=%2d @ face=%s", (int)m->data.addNeighbor.nid, faceToString((Face)m->data.addNeighbor.face));
    break;

  case CMD_DEL_NBR:
    sprintf(p, " face=%s", faceToString((Face)m->data.delNeighbor.face));
    break;

  case CMD_COLOR:
    sprintf(p, " (%02x,%02x,%02x) @ %02x", (unsigned)m->data.color.r, (unsigned)m->data.color.g, (unsigned)m->data.color.b, (unsigned)m->data.color.i);
    break;

  default: 
    {
      int i;
      int end=(m->size/sizeof(message_type))-3;
      for (i=0; i<end; i++) {
        sprintf(p, " %04x", (unsigned)m->data.units[i]);
        p = p + strlen(p);
      }
    }
  }
  return buffer;
}

static struct sockaddr_in servaddr, cliaddr;
static socklen_t clilen;
static int listenfd;
static int connfd;
static int haveConnection = 0;

// 1 -> connection closed, 0 -> ok
int
drain_incoming(void)
{
  while (is_data_available(connfd)) {
    if (!force_read(connfd)) {
      fprintf(stderr, "Connection closed\n");
      return 1;
    }
  }
  return 0;
}

typedef struct _msgContainer MsgList;
struct _msgContainer {
  message* content;
  MsgList* next;
};
static MsgList* mhead = NULL;
static MsgList* mtail = NULL;

// send msg if connection open, otherwise queue it up
void
static sendit(message* msg)
{
  int size = msg->size + sizeof(message_type);			/* in bytes */
  if (!haveConnection) {
    // must queue
    MsgList* ml = calloc(1, sizeof(MsgList));
    ml->content = calloc(1, size);
    memcpy(ml->content, msg, size);
    ml->next = NULL;
    if (mhead) {
      mtail->next = ml;
      mtail = ml;
    } else {
      mhead = mtail = ml;
    }
    return;
  }
  // we have a connection

  // first drain incoming if necessary
  drain_incoming();
  // now send everything in que
  while (mhead != NULL) {
    message* m = mhead->content;
    int bytes = m->size + sizeof(message_type);
    fprintf(stderr, "Sending: %s\n", msg2string(m, 0));
    int status = sendto(connfd, 
			m,
			bytes, 
			0, 
			(struct sockaddr *)&cliaddr, 
			sizeof(cliaddr));
    if (status != bytes) err("Error writing to socket: %s\n", strerror(errno));
    free(m);
    MsgList* next = mhead->next;
    free(mhead);
    mhead = next;
    if (drain_incoming()) return;
  }
  mhead = mtail = NULL;

  // finally, send this msg
  int bytes = msg->size + sizeof(message_type);
  fprintf(stderr, "Sending: %s\n", msg2string(msg, 0));
  int status = sendto(connfd, 
		      msg, 
		      bytes, 
		      0, 
		      (struct sockaddr *)&cliaddr, 
		      sizeof(cliaddr));
  if (status != bytes) err("Error writing to socket: %s\n", strerror(errno));
}

// send msg to vm
void
msg2vm(Block* dest, VMCommand cmd, Time timestamp, ...)
{
  switch (cmd) {
  case CMD_ADD_NCOUNT:
  case CMD_ADD_VACANT:
  case CMD_DEL_VACANT:
  case CMD_DEL_NCOUNT:
    //fprintf(stderr, "No longer using %s\n", vmcmd2str(cmd));
    return;
  }
  static message buffer;

  va_list ap;
  va_start(ap,timestamp);

  // now send
  if (msgverbose)
    fprintf(stderr, 
            "sending %s -> %s @ %d\n", 
            vmcmd2str(cmd), (dest == NULL) ? "VM" : nodeIDasString(dest->id, 0), timestamp);

  int size = 3;
  switch (cmd) {
  case CMD_CREATE:
    buffer.data.create.num = va_arg(ap, message_type);
    buffer.data.create.start = va_arg(ap, message_type);
    size += 2;
    break;
  case CMD_DEL_NBR:
    buffer.data.delNeighbor.face = va_arg(ap, message_type);
    size += 1;
    break;

  case CMD_ADD_NBR:
    buffer.data.addNeighbor.nid = va_arg(ap, message_type);
    buffer.data.addNeighbor.face = va_arg(ap, message_type);
    size += 2;
    break;

  case CMD_ADD_VACANT:
  case CMD_DEL_VACANT:
    buffer.data.vacant.face = va_arg(ap, message_type);
    size += 1;
    break;

  case CMD_ADD_NCOUNT:
  case CMD_DEL_NCOUNT:
    buffer.data.neighborCount.total = va_arg(ap, message_type);
    size += 1;
    break;

  }

  buffer.size = size*sizeof(message_type);
  buffer.command = cmd;
  if (dest)
    buffer.node = dest->id;
  else
    buffer.node = -1;
  buffer.timestamp = timestamp;

  sendit(&buffer);
  va_end(ap);
  // now deal with time update
  if (dest && !vmUseThreads) {
    updateTime(dest, timestamp+1, 0);
  }
}

static message_type
handle_data(message *msg)
{
  NodeID nodeid = msg->node;
  Block* block = getBlock(nodeid);
  Time ts = (Time)msg->timestamp;

  block->msgTargetsDelta++;
  if (1 || msgverbose) fprintf(stderr, "Got message of %d from %u @ %u\n", (int)msg->size, (int)nodeid, (int)ts);
  if (block == NULL) err("unknown block with id %d in msg\n", nodeid);
  switch(msg->command) {
  case SET_COLOR:
    printf("SET_COLOR received for block : %d | connfd : %d",block->id,block->connfd);
 
    fprintf(stderr, "%02u <- LEDS = (%d, %d, %d, %d)\n",
	    (int)nodeid, (int)msg->data.color.r, (int)msg->data.color.g, (int)msg->data.color.b, (int)msg->data.color.i);
    block->simLEDr=msg->data.color.r;
    block->simLEDg=msg->data.color.g;
    block->simLEDb=msg->data.color.b;
    block->simLEDi=msg->data.color.i;
    break;

  case CMD_HAS_RUN:
    {
      int numother = (int)msg->data.runtil.num;
      fprintf(stderr, "%02u til:%d comm with:%d%c", (int)nodeid, ts, numother, (numother == 0 ? '\n' : ':'));
      updateTime(block, ts, 0);
      int i;
      for (i=0; i<numother; i++) {
        fprintf(stderr, "%d%s", (int)msg->data.runtil.other[i], (i+1)==numother?"\n":", ");
        needsSchedule(getBlock(msg->data.runtil.other[i]), ts);
      }
   }
    break;

  default:
    fprintf(stderr, "Unknown msg: %d\n", (int)msg->command);
  }
  return msg->command;
}

int nothingHappened = 0;

// reads from socket until something is available
static int
force_read(int sock)
{
  message data;
  size_t mlength;
  char *buffer = (char*)(&(data.command));
  int offset = 0;

  if (msgverbose) fprintf(stderr, "Enter force_read %d\n", sock);
  nothingHappened = 0;

  mlength = recvfrom(sock, (void*)&data, sizeof(message_type), MSG_WAITALL, (struct sockaddr *)&cliaddr, &clilen);
  if (mlength == -1) {
    if (errno == 104) return 0;
    err("Failed to read1:%d:%s\n", errno, strerror(errno));
  }
  if (mlength == 0) return 0;	/* socket is closed */
  assert(mlength == sizeof(message_type)); /* this is bad form, but will leave for now */
  //fprintf(stderr, "%d\n", (int)mlength);

  while (offset < data.size) {
    int length = recvfrom(sock, buffer+offset, data.size-offset, MSG_WAITALL, (struct sockaddr *)&cliaddr, &clilen);
    if (length == -1) err("Failed to read2:%d:%s\n", errno, strerror(errno));
    offset += length;
  }
  assert(offset == (size_t)data.size);

  fprintf(stderr, "Got Msg: %s\n", msg2string(&data, 0));
  //message_type ret = 
  handle_data(&data);
  return 1;
}

static int
is_data_available(int sock)
{
   fd_set sready;
   struct timeval nowait;

   if (!haveConnection) return 0; /* no connection, so no data */

   if (msgverbose) fprintf(stderr, "ida\n");
   FD_ZERO(&sready);
   FD_SET((unsigned int)sock,&sready);
   memset((char *)&nowait,0,sizeof(nowait));

   select(1,&sready,NULL,NULL,&nowait);

   return FD_ISSET(sock, &sready);
}

static char* portname;

static void*
listener(void* ignoreme)
{
  int yes = 1;

   haveConnection = 0;
   listenfd = socket(AF_INET, SOCK_STREAM, 0);
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servaddr.sin_port = htons(atoi(portname));
   int status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
   if (status != 0) err("setsockopt failed %d:%s\n", errno, strerror(errno));
   
   status = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
   if (status != 0) err("bind failed %d: %s\n", errno, strerror(errno));

   status = listen(listenfd, 1024);
   if (status != 0) err("listen failed %d: %s\n", errno, strerror(errno));

   clilen = sizeof(cliaddr);
   connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
   if (connfd == -1) err("accept failed %d: %s\n", errno, strerror(errno));

   fprintf(stderr, "Got a connection!\n");
   haveConnection = 1;

   vmStarted();
   if (vmUseThreads) {
     // just read message and let vm schedule nodes
     while (force_read(connfd)) {
       if (msgverbose) fprintf(stderr, "got message\n");
     }
   } else {
     // we have to manage running of nodes
     int nothingHappened = 0;
     while (1) {
       int incoming = 0;
       while (is_data_available(connfd)) {
         incoming++;
         nothingHappened = 0;
         if (!force_read(connfd)) {
           fprintf(stderr, "Connection closed\n");
           break;
         }
       }
       if (!incoming) {
         // nothing was there, sleep for a bit
         usleep(10000);
       }
       if (checkSchedule()) {
         // we sheduled something
         nothingHappened = 0;
       } else if (nothingHappened++ > 10) {
         fprintf(stderr, "Increment gts: %d\n", globalTimeStamp);
         Block* b;
         ForEachBlock(b) {
           showBlock(stderr, b);
         }
         globalTimeStamp++;
       }
     }
   }
   // all done?
   // final status report
   printf("Final Status\n");
   Block* block;
   ForEachBlock(block) {
     showBlock(stdout, block);
   }
   checkTest(1);
   exit(0);
   return (void*)0;
}

// get started, make connection, spawn thread
void 
vmInit(char* port)
{
  portname = port;
  int status;

  // start thread to listen to the socket
  pthread_attr_t attr;
  status = pthread_attr_init(&attr);
  if (status != 0) err("pai failed %d: %s\n", errno, strerror(errno));

  status = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  if (status != 0) err("pasd failed %d: %s\n", errno, strerror(errno));

  pthread_t id;
  status = pthread_create(&id, &attr, &listener, 0);
  if (status != 0) err("pc failed %d: %s\n", errno, strerror(errno));
}

// Local Variables:
// tab-width: 4
// indent-tabs-mode: nil
// End:

