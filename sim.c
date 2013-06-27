#define DEFINE_GLOBALS

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "block.h"
#include "viewer.h"
#include "sim.h"
#include "config.h"
#include "variable_queue.h"

#include "vm.h"

char* progname;			/* name of this program */
char* configname = 0;		/* name of config file */
bool debug = false;             /* debug-mode */

int lastBlockId = 0;
Time maxGlobalSimTime = 0;
Time globalTimeStamp = 0;
int vmUseThreads = true;
char* testfile = NULL;
int stopwhenidle = -1;

void __myassert(char* file, int line, char* exp)
{
    fprintf(stderr, "%s:%d: ASSERTION FAILED: %s\n", file, line, exp);
    *(int *)0 = 1;
    exit(-1);
}

void err(char* prompt, ...)
{
   va_list ap;
   va_start(ap,prompt);

   fprintf(stderr, "%s: Error: ", progname);
   vfprintf(stderr, prompt, ap);
   fprintf(stderr, "\n");
   exit(-1);
}

void help(void)
{
  fprintf(stderr, "%s: simulate blocks\n", progname);
  fprintf(stderr, "\t-c <name>\tfile with initial configuration\n");
  fprintf(stderr, "\t-i <count>\tIn ND mode, terminate after <count> cycles with no msgs from meldvm\n");
  fprintf(stderr, "\t-p <num>\tuse port <num> for meldvm\n");
  fprintf(stderr, "\t-r\t\tgenerates a random block configuration\n");
  fprintf(stderr, "\t-D\t\tuse deterministic sim mode with meldvm\n");
  fprintf(stderr, "\t-R <num>\tgenerates a random block configuration of <num> blocks\n");
  fprintf(stderr, "\t-d\t\tdebug statements enabled\n");
  fprintf(stderr, "\t-n\t\tdisables graphics\n");

  exit(0);
}

void blockprint(Block* me, FILE* f, char* fmt, ...)
{
  va_list ap;
  char buffer[128];

  va_start(ap,fmt);
//  debuginfo(this(), buffer);
  fprintf(f, "%s:(%s) ", nodeIDasString(me->id, 0), buffer);
  vfprintf(f, fmt, ap);
  fflush(f);
  va_end(ap);
}

int main(int argc, char** argv)
{
   // save argv, argc for initialization of GLUT
   char** orig_argv = argv;
   int    orig_argc = argc;
   bool   configured = false;
   bool   graphics = true;
   char* port = "5000";
   numberOfRobots = 0;		/* number of blinkblocks in system */
   --argc;
   progname = *argv++;
   while (argc > 0 && (argv[0][0] == '-')) {
      switch (argv[0][1]) {
      case 'c':
    	  if (configured)
    		  help();
	  configFile = argv[1];
    	  argc--;  argv++;
    	  configured = true;
    	  break;
      case 'p':
	port = argv[1];
    	  argc--;  argv++;
	  break;

      case 'i':
	stopwhenidle = atoi(argv[1]);
    	  argc--;  argv++;
          break;
      case 'd':
    	  debug = true;
          break;
      case 't':
	graphics = false;
	testfile = argv[1];
    	  argc--;  argv++;
          break;
      case 'n':
   	  graphics = false;
    	  break;
      case 'D':
   	  vmUseThreads = false;
    	  break;
      case 'r':
    	  if (configured) help();
	  configCount = 0;
    	  configured = true;
    	  break;
      case 'R':
    	  if (configured) help();
	  configCount = atoi(argv[1]);
    	  argc--;  argv++;
    	  configured = true;
    	  break;
      default:
    	  help();
      }
      argc--; argv++;
   }

   if (debug) fprintf(stdout, "initial configuration\n");

   // start timer
   
   initTimeKeeping();

   // vm initialization
    // starts a thread listener
    //listener accepts a connection, calls vmStarted which initializes blocks
   vmInit(port); 
   fprintf(stderr, "Listening on %s\n", port);

   //allocate message buffer and set command,size ..sending msg to vm via datagram 
  // if (vmUseThreads) msg2vm(NULL, CMD_MODE_ND, 0);

   // create blocklist and initialize mutex
   initBlockList();

   // see if we are running a test
   if (testfile) configured = configtest(testfile);

   if (!configured) {
     help();
   } else {
     if (configFile != NULL)
       readConfig(configFile);
     else
       randomConfig(configCount);
   }

   if (graphics) {
     // initialize viewer
     viewer_init(orig_argc, orig_argv);

     // GL loop indefinitely
     event_loop();
   } else {
     fprintf(stderr, "Running without graphics\n");
     int idle = 0;
     while (1) {
       Block *block;
       sleep(1);
       int changes = 0;
       ForEachBlock(block) {
	 if (block->msgTargetsDelta > 0) changes++;
       }
       if (changes == 0) {
	 idle++;
	 if ((stopwhenidle > 0) && (idle > stopwhenidle)) {
	   printf("All Done\n");
	   msg2vm(NULL, STOP, 0);
	 }
	 continue;
       }
       idle = 0;
       fprintf(stderr, "\n---- %d\n", changes);

       // something changed
       ForEachBlock(block) {
	 if (block->msgTargetsDelta > 0) {
	   block->msgTargets += block->msgTargetsDelta;
	   block->msgTargetsDelta = 0;
	   showBlock(stderr, block);
	 }
	 if (checkTest(0)) {
	   msg2vm(NULL, STOP, 0);
	 }
       }
       fprintf(stderr, "\n");
     }
   }

   // we are all done
   return 0;
}

int
checkTest(alldone)
{
  Block* block;
  if (testfile == NULL) return 0;
  // see if we are done
  int waiting = 0;
  int failed = 0;
  ForEachBlock(block) {
    int* tdata = testData[block->id];
    if (alldone || (block->msgTargets >= tdata[1])) {
      if ((block->x != tdata[2])||(block->y != tdata[3])||(block->z != tdata[4])) 
	err("Block %d in wrong position", block->id);
      if ((block->simLEDr == tdata[5])&&
	  (block->simLEDg == tdata[6])&&
	  (block->simLEDb == tdata[7])&&
	  (block->simLEDi == tdata[8])) printf("%d passed\n", block->id);
      else
	failed++;
    } else {
      waiting++;
    }
  }
  if ((failed == 0)&&(waiting==0)) {
    fprintf(stdout, "Test Passed\n");
    testfile = NULL;		/* don't check again */
    return 1;
  }
  return 0;
}

// will return 0 if never called before, other return 1
int 
alreadyExecuted(int enter)
{
  static int count = 0;
  int r;

  if (enter == 0) {
    r = count++;
    if (r == 0) return 0;
    // we aren't first AND vm_init has run
    return r;
  } else {
    // we ran vm_init
    return 0;
  }
}

// for now ignore x and just yield this thread
void yieldTil(Time x)
{
  sched_yield();
}

void
vmStarted()
{
  // tell vm what mode we are in, if necessary

  // get blocks started
  Block *block;

  Q_FOREACH(block, getBlockList(), blockLink) {
    startBlock(block);
  }
  Q_ENDFOREACH(getBlockList());
}
