#include <stdlib.h>
#include "myassert.h"

#include "block.h"
#include "vm.h"
#include "sim.h"

static unsigned int nextBlockId = 1;
static BlockList blockList;  // = Q_STATIC_INIT;
static Block** allBlocks = NULL;
static int allBlockSize = 0;

// initializes the access mutex
void initBlockList()
{
  Q_INIT_HEAD(&blockList);
  allBlocks = (Block**)calloc(2, sizeof(Block*));
  allBlockSize = 2;
}


void registerBlock(Block* block)
{
  Q_INSERT_TAIL(&blockList, block, blockLink);
  if (block->id >= allBlockSize) {
    //fprintf(stderr, "Reallocing to %d\n", allBlockSize*2);
    allBlocks = (Block**)realloc(allBlocks, allBlockSize*2*sizeof(Block*));
    allBlockSize = allBlockSize*2;
  }
  allBlocks[block->id] = block;
}

void 
startBlock(Block *block)
{
  // start block off with no neighbors and all vacant
  msg2vm(block, CMD_ADD_NCOUNT, block->localTime, 0);
  int i;
  for (i=0; i<NUM_PORTS; i++) 
    msg2vm(block, CMD_ADD_VACANT, block->localTime, i);
  checkForNeighbors(block, NULL);
}

Block *
createBlock(int x, int y, int z)
{
  Block* newBlock;

  newBlock = calloc(1, sizeof(Block));
  if (newBlock == NULL)
    return newBlock;

  newBlock->id = nextBlockId++;
  newBlock->x = x;
  newBlock->y = y;
  newBlock->z = z;
  newBlock->simLEDr = 120;
  newBlock->simLEDg = 120;
  newBlock->simLEDb = 120;
  newBlock->simLEDi = 200;

  newBlock->msgTargetsDelta = 0;
  newBlock->msgTargets = 0;

  newBlock->destroyed = 0;
  newBlock->localTime = globalTimeStamp;
  newBlock->lastSimTime = globalTimeStamp-1;

  int i;
  for (i=0; i<NUM_PORTS; i++) {
    newBlock->neighborhood[i] = NULL;
  }

  //fprintf(stderr, "made block, inserting into Q\n");
  registerBlock(newBlock);

  msg2vm(newBlock, CMD_CREATE, newBlock->localTime, 1, newBlock->id);

  return newBlock;
}

typedef struct {
  Block* block;
  Face face;
} FaceBlock;
  
void  tellNeighborsDestroyed(Block *b, FaceBlock* list);

void destroyBlock(Block *block)
{
  FaceBlock neighbors[NUM_PORTS+1];
  tellNeighborsDestroyed(block, neighbors);
  Q_REMOVE(&blockList, block, blockLink);
  block->destroyed = 1;
  allBlocks[block->id] = NULL;
  free(block);
  // now set neighbor count
  int i;
  for (i=0; neighbors[i].block != 0; i++) {
    msg2vm(neighbors[i].block, CMD_ADD_NCOUNT, countNeighbors(neighbors[i].block));
    msg2vm(neighbors[i].block, CMD_ADD_VACANT, neighbors[i].face);
  }
}

BlockList *getBlockList(void)
{
	return &blockList;
}

Block* 
getBlock(NodeID id)
{
  if (id > nextBlockId) err("Asked for %d higher than num allocated blocks %d", id, nextBlockId);
  Block* block = allBlocks[id];
  if (block == NULL) err("Aksed for %d which has NULL entry in allBlocks", id);
  if (block->destroyed) err("Asked for %d which was destroyed", id);
  return block;
}

// format NodeID

char*
nodeIDasString(NodeID x, char* sid)
{
  static char ssid[10];
  if (sid == 0) sid = ssid;
  sprintf(sid, "%02u", x);
  return sid;
}

// b: block to check
// other: if set, then we only tell b about other's face
void
checkForNeighbors(Block* b, Block* other)
{
  int i;
  for(i = 0; i < NUM_PORTS; ++i) {
    // see if we have a neighbor at port i
    Block* d = seeIfNeighborAt(b, i);
    if ((d != 0)&&((other==NULL)||(other==d))) {
      if (d->neighborhood[i] == NULL) {
        // we have a neighbor. tell them
        msg2vm(d, CMD_DEL_VACANT, b->localTime, i);
        msg2vm(d, CMD_ADD_NBR, b->localTime, b->id, i);
        if (other == NULL) checkForNeighbors(d, b);
        msg2vm(d, CMD_ADD_NCOUNT, b->localTime, countNeighbors(d));
        d->neighborhood[i] = b;
      }
      assert(d->neighborhood[i] == b);
    }
  }
}

int
countNeighbors(Block* b)
{
  int cnt = 0;
  int i;
  for(i = 0; i < NUM_PORTS; ++i) {
    // see if we have a neighbor at port i
    Block* d = seeIfNeighborAt(b, i);
    if (d != NULL) cnt++;
  }
  return cnt;
}

void 
tellNeighborsDestroyed(Block *b, FaceBlock* list)
{
  int i;
  int j = 0;

  for(i = 0; i < NUM_PORTS; ++i) {
    // see if we have a neighbor at port i
    Block* d = seeIfNeighborAt(b, i);
    if (d != 0) {
      assert(d->neighborhood[i] == b);
      d->neighborhood[i] = NULL;
      // if so, then eliminate it from that block
      msg2vm(d, CMD_DEL_NBR, b->localTime, i);
      if (list != NULL) {
        list[j].block = d;
        list[j].face = i;
        j++;
      }
    }
  }
  if (list != NULL) list[j].block = NULL;
}

#include "heap.h"

int
bLocalTimeCompare(Block* a, Block* b)
{
  return a->localTime - b->localTime;
}

int
bLastSimTimeCompare(Block* a, Block* b)
{
  return a->lastSimTime - b->lastSimTime;
}

Heap* gheap;                    /* blocktime vrs. globaltime */
Heap* dheap;                    /* blocktime vrs. last sim time */

void
initTimeKeeping(void)
{
  gheap = newHeap(10, (ComparisonFunction)bLocalTimeCompare);
  dheap = newHeap(10, (ComparisonFunction)bLastSimTimeCompare);
  globalTimeStamp = 0;
}

void
updateTime(Block* block, Time ts, int delta)
{
  fprintf(stderr, "Updatetime: %d <- %d was:%d, %d\n", block->id, ts, block->localTime, block->lastSimTime);
  if (block->localTime <= ts) {
    block->localTime = ts;
    if (heapUpdate(gheap, block) == NULL) {
      heapAdd(gheap, block);
    }
  }
  Block* minBlock = (Block*)heapPeek(gheap);
  if (minBlock->localTime > globalTimeStamp) 
    globalTimeStamp = minBlock->localTime;
}

static int
checkGlobalTime(Block* b)
{
  return b->localTime < globalTimeStamp;
}

static int
checkSimTime(Block* b)
{
  return b->lastSimTime < b->localTime;
}

static int
checkASched(Heap* heap, int (*ok)(Block*))
{
  int cnt = 0;
  Block* b = heapPeek(heap);
  if (b == NULL) return 0;
  while ((b != NULL) && ok(b)) {
    heapRemove(heap);
    b->localTime = globalTimeStamp;
    cnt++;
    msg2vm(b, CMD_RUN, b->localTime);
    b->lastSimTime = b->localTime;
    b = heapPeek(heap);
  }
  return cnt;

}

int
checkSchedule(void)
{
  int cnt;
  cnt = checkASched(gheap, checkGlobalTime);
  if (cnt == 0) cnt = checkASched(dheap, checkSimTime);
  return cnt;
}

void
needsSchedule(Block* block, Time ts)
{
}

void
showBlock(FILE* f, Block* block)
{
  fprintf(f, "%s:%d @%d,%d (%2d,%2d,%2d) [%3d,%3d,%3d,%3d]\n", 
          nodeIDasString(block->id, 0),
          block->msgTargets,
          block->localTime,
          block->lastSimTime,
          block->x,
          block->y,
          block->z,
          block->simLEDr,
          block->simLEDg,
          block->simLEDb,
          block->simLEDi);
}

// Local Variables:
// tab-width: 4
// indent-tabs-mode: nil
// End:
