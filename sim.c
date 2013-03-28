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
int numberOfRobots = 0;		/* number of blinkblocks in system */
bool debug = false;             /* debug-mode */

int lastBlockId = 0;
Time maxGlobalSimTime = 0;
Time globalTimeStamp = 0;
int vmUseThreads = true;

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
  fprintf(stderr, "\t-c <name>:\tfile with initial configuration\n");
  fprintf(stderr, "\t-r generates a random block configuration\n");
  fprintf(stderr, "\t-d debug statements enabled\n");
  fprintf(stderr, "\t-n disables graphics\n");

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
   char* configFile = NULL;
   int   configCount = 0;

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

      case 'd':
    	  debug = true;
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
   vmInit(port);
   fprintf(stderr, "Listening on %s\n", port);
   if (vmUseThreads) msg2vm(NULL, CMD_MODE_ND, 0);

   // create blocklist and initialize mutex
   initBlockList();

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
     fprintf(stderr, "Running without graphics??\n");
     while (1) {
       Block *block;
       sleep(1);
       fprintf(stderr, "\n----\n");
       Q_FOREACH(block, getBlockList(), blockLink) {
	   fprintf(stderr, "%s:%d (%2d,%2d,%2d) [%3d,%3d,%3d,%3d]\n", 
		   nodeIDasString(block->id, 0),
		   block->blockReady,
		   block->x,
		   block->y,
		   block->z,
		   block->simLEDr,
		   block->simLEDg,
		   block->simLEDb,
		   block->simLEDi);
       }
       Q_ENDFOREACH(getBlockList());
       fprintf(stderr, "\n");
     }
   }

   // we are all done
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
