#ifndef _SIM_H_
#define _SIM_H_

void err(char *prompt, ...);
void vmStarted(void);
Time globalTimeStamp;
int vmUseThreads;
int checkTest(int alldone);
int numberOfRobots;		/* number of blinkblocks in system */

#endif
