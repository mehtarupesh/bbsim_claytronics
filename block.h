#ifndef _BLOCK_H_
#define _BLOCK_H_

#include "mymalloc.h"
#include "defs.h"
#include "variable_queue.h"

typedef char bool;
#define false 0
#define true 1

typedef uint16_t  Uid;

typedef int Time;
#define NUM_PORTS 6
typedef struct _neighbor_t { Uid n[NUM_PORTS]; }  Neighborhood;

typedef struct _Block Block;

#include "world.h"

#include <stdint.h>

/* define a new BlockList type */
Q_NEW_HEAD(BlockList, Block);

struct _Block
{
  /* unique block indentification */
  NodeID id;

  /* block physical properties */
  int x;
  int y;
  int z;

  // track activity in vm
  int msgTargetsDelta;
  int msgTargets;

  /* Current Time */
  Time localTime;
  Time lastSimTime;
  
  int simLEDr, simLEDg, simLEDb, simLEDi;
  
  Block* neighborhood[NUM_PORTS];

  //flag to prevent access to this block until it has completed startup
  int blockReady;

  int tapBuffer;

  // when we destroy a block we mark it as such
  int destroyed;

  /* linked list */
  Q_NEW_LINK(Block) blockLink;
};

void err(char* prompt, ...);

void initBlockList(void);
Block *createBlock(int x, int y, int z);
void destroyBlock(Block *block);
void startBlock(Block *block);
BlockList *getBlockList(void);
Block* getBlock(NodeID id);

char* nodeIDasString(NodeID x, char* sid);
void checkForNeighbors(Block* b, Block* other);
int countNeighbors(Block* b);

void updateTime(Block* block, Time ts, int delta);
int checkSchedule(void);
void needsSchedule(Block* block, Time ts);
void initTimeKeeping();
void showBlock(FILE* f, Block* block);

// quick iterator for all blocks

#define ForEachBlock(ptr)  for ((ptr)=Q_GET_FRONT(getBlockList()); (ptr) != NULL; (ptr)=Q_GET_NEXT((ptr), blockLink))

#endif
