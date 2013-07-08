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
#include "csapp.h"

static int msgverbose = 0;
static bool allConnected=false;

static int is_data_available(int sock);
static int force_read(int sock);

// convert # to str
char*
vmcmd2str(VMCommand cmd)
{
  static char buffer[128];

  switch(cmd) {
  case CMD_CREATE:	return "create";
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
  case CMD_SETID: return "setid";
  case CMD_SENDMSG: return "sendMessage";
  case CMD_RECVMSG: return "receiveMessage";
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

static struct sockaddr_in cliaddr;
static socklen_t clilen;


// 1 -> connection closed, 0 -> ok
int
drain_incoming(int connfd)
{
  while (is_data_available(connfd)) {
    if (!force_read(connfd)) {
     // fprintf(stderr, "Connection closed\n");
     // return 1;
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
  NodeID nodeid = msg->node;
  Block* block = getBlock(nodeid);
  if(block==NULL){
    printf("Message not sent to block %d \n",nodeid);
    return;
  }
  int connfd=block->connfd;
  int size = msg->size + sizeof(message_type);			/* in bytes */
  if (connfd==-1) {
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
  drain_incoming(connfd);
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
    if (drain_incoming(connfd)) return;
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
  case CMD_DEL_NBR
  :
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
	
	case CMD_SETID:
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
  if(block!=NULL)
  block->msgTargetsDelta++;
  if (1 || msgverbose) fprintf(stderr, "Got message of %d from %u @ %u\n", (int)msg->size, (int)nodeid, (int)ts);
  if (block == NULL) err("unknown block with id %d in msg\n", nodeid);
  switch(msg->command) {
  case SET_COLOR:
    printf("SET_COLOR received for block : %d | connfd : %d\n",block->id,block->connfd);
 
    fprintf(stderr, "%02u <- LEDS = (%d, %d, %d, %d)\n",
	    (int)nodeid, (int)msg->data.color.r, (int)msg->data.color.g, (int)msg->data.color.b, (int)msg->data.color.i);
    block->simLEDr=msg->data.color.r;
    block->simLEDg=msg->data.color.g;
    block->simLEDb=msg->data.color.b;
    block->simLEDi=msg->data.color.i;
    break;
	
	case SEND_MESSAGE:
	printf("SEND_MESSAGE received\n");
	NodeID tempID=msg->node;
	msg->node=msg->data.send_message.dest_nodeID;
	msg->data.send_message.dest_nodeID=tempID;
	msg->command=RECEIVE_MESSAGE;
	sendit(msg);
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

   if (sock==-1) return 0; /* no connection, so no data */

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
	int port=atoi(portname);
	unsigned int sock, s, maxsock;
    fd_set socks;
    fd_set readsocks;
  	   
	sock=Open_listenfd(port);

	printf("Listening on port:%d\n",port);
    /* Set up the fd_set */
    FD_ZERO(&socks);
    FD_SET(sock, &socks);
    maxsock = sock;
	int nodeID=0;
	/* Main loop */
    while (1) {
        
        readsocks = socks;
        if (select(maxsock + 1, &readsocks, NULL, NULL, NULL) == -1) {
            perror("select");
            return (void*)1;
        }
        for (s = 0; s <= maxsock; s++) {
			    if (FD_ISSET(s, &readsocks)) {
            if (s == sock) {
                    /* New connection */
//					printf("Shall be accepting the connection.\n");
                    int newsock;
                    struct sockaddr_in their_addr;
                    unsigned int size = sizeof(struct sockaddr_in);
                    newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
					nodeID++;
					Block* b=getBlock(nodeID);
					b->connfd=newsock;
					msg2vm(b,CMD_SETID,b->localTime);
          if(!allConnected&&(nodeID==numberOfRobots)){
					vmStarted();
					allConnected=true;
					}
          if(allConnected){
            startBlock(b);
          }
					printf("Accepted a connection.\n");
				   	if (newsock == -1) {
                        perror("accept");
                    }
					else {
                        printf("Got a connection from %s on port %d\n", inet_ntoa(their_addr.sin_addr), htons(their_addr.sin_port));
						FD_SET(newsock, &socks);
                        if (newsock > maxsock) {
                            maxsock = newsock;
                        }
                    }
                }else// read from already accepted socket
                {
//					printf("Reading from an accepted socket:%d\n", s);
                   int check = (force_read(s));
				   if(check==0);
				  // if(check == 0) //sock closed, remove from ur list 
				  // FD_CLR(s,&socks);    
                }
            }
        }

    }
    close(sock);
	
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

