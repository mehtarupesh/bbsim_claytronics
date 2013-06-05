/*
vm_client.c : test program for forked vm block


*/
#include "csapp.h"
#include "vm.h"

static unsigned int port = 0;
static unsigned int block_id= 0;
static char host[] = "localhost";
static char block_header[] = "block_id";
static int server_fd = 0;

static struct sockaddr_in cliaddr;
static socklen_t clilen;


int main (int argc , char *argv[])
{
	int clientfd;
	rio_t rio;
	char buf[MAXLINE];
	int nobytes;
    size_t mlength;
    message data;
    char *buffer = (char *)(&(data.command));
    	
	if(argc != 2){
		printf("error in format : %s <port> <block_id>\n",argv[0]);
		exit(0);
	}

	// get server port and id
	port = atoi(argv[0]);
//	block_id = atoi(argv[1]);
//	printf("port=%d\nid=%d\n",port,block_id);
	// communicate with server
	clientfd = Open_clientfd(host,port);

    printf("clientfd = %d\n");

//	printf("Clientfd=%d for id:%d\n",clientfd,block_id);
//	Rio_readinitb(&rio,clientfd);

	printf("Connection Established : block_id : %d\n",block_id);
	
	// receive reply
//	nobytes = Rio_readlineb(&rio,buf,MAXLINE);		
//	printf("Received from server :%s\n",buf); 
    mlength = recvfrom(clientfd, (void*)&data, sizeof(message_type), MSG_WAITALL,(struct sockaddr *)&cliaddr, &clilen);
 
    if (mlength == -1) {
        if (errno == 104) return 0;
        err("Failed to read1:%d:%s\n", errno, strerror(errno));
    }
    if (mlength == 0) return 0;   /* socket is closed */
    assert(mlength == sizeof(message_type)); /* this is bad form, but will leave
                                                for now */
    //fprintf(stderr, "%d\n", (int)mlength);

    while (offset < data.size) {
        int length = recvfrom(sock, buffer+offset, data.size-offset, MSG_WAITALL,
                (struct sockaddr *)&cliaddr, &clilen);
        if (length == -1) err("Failed to read2:%d:%s\n", errno, strerror(errno));
        offset += length;
    }

    assert(offset == (size_t)data.size);

    printf("Got Msg: data->command = %d | data->node = %d\n", data->command,data->node);
    block_id = data->node;

	//send SET_COLOR message
    message *m = get_message(128,128,128);
    send_message(m,clientfd);

//	sprintf(buf,"%s : %d says hello\n",block_header,block_id);
//	Rio_writen(clientfd,buf,strlen(buf) + 1);

	Close(clientfd);
	exit(0);

}

message* get_message(const int r,const int g,const int b)
{
    
    message *m = malloc(sizeof(message));
 
    m->size  = 7 * sizeof(message_type);
    m->command = SET_COLOR;
    m->timestamp = 0;
    m->node = (message_type)block_id;
    (m->color).r = (message_type)r;
    (m->color).g = (message_type)g;
    (m->color).b = (message_type)b;
    (m->color).i = 0; // intensity

    return m;
}


void send_message(message* m, int clientfd)
{
    int bytes = m->size + sizeof(message_type);
    int status = sendto(clientfd, 
            m,
            bytes, 
            0, 
            (struct sockaddr *)&cliaddr, 
            sizeof(cliaddr));

    if (status != bytes) err("Error writing to socket: %s\n", strerror(errno));

}
