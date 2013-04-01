#ifndef _CONFIG_H_
#define _CONFIG_H_

char* configFile;
int   configCount;
int** testData;


void randomConfig(int);
void readConfig(char *name);
int configtest(char* name);

#endif
