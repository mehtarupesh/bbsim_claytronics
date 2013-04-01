#include "myassert.h"
#include "block.h"
#include "config.h"
#include "sim.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char* configFile = NULL;
int   configCount = 0;

void randomConfig(int maxcount)
{
  bool grid[11][11][11];
  Block* stack[11*11*11];
  int stack_idx = 0;
  Block *b, *curr;
  int x, y, z, i;
  int count = 1;
  int prob = 30;

  memset(grid, 0, sizeof(grid));

  if (maxcount == 0) maxcount = 25;
  b = createBlock(0, 0, 0);
  grid[5][5][5] = true;

  srand( time(NULL) );

  stack[stack_idx++] = b;

  while (stack_idx > 0 && count < maxcount)
    {
      stack_idx--;
      curr = stack[stack_idx];
      for (i = 0; (i < 6) && (count < maxcount); i++) {
	  x = curr->x;
	  y = curr->y;
	  z = curr->z;
	  if (rand() % 100 > prob)
	    {
	      switch (i)
		{
		case North:
		  z--;
		  break;
		case East:
		  x++;
		  break;
		case South:
		  z++;
		  break;
		case West:
		  x--;
		  break;
		case Top:
		  y++;
		  break;
		case Down:
		  y--;
		  break;
		default:
		  break;
		}
	      if (x < -5 || y < -5 || z < -5 ||
		  x > 5 || y > 5 || z > 5)
		continue;
	      if (grid[5+x][5+y][5+z])
		continue;

	      grid[5+x][5+y][5+z] = true;
	      b = createBlock(x, y, z);

	      count++;
	      stack[stack_idx] = b;
	      stack_idx++;
	      if (prob > 16) prob--;
	    }
    	}
    }
}

void readConfig(char* name)
{
	FILE* f = fopen(name, "r");
	if (f == NULL)
		err("Can't open %s for reading", name, 0, 0);

	int line = 1;
	while (!feof(f))
	{
		char buffer[256];
		char* p = fgets(buffer, 256, f);
		if (p == NULL)
			break;

		int x, y, z, dir;
		//fprintf(stderr, "[%s]\n", buffer);
		int num = sscanf(buffer, "%d, %d, %d, %d", &x, &y, &z, &dir);
		if (num != 4)
		  err("Input from %s:%d expected 4, got %d [%s]", name, line, num, buffer);
		//fprintf(stderr, "create %d at: %d, %d, %d\n", line, x, y, z);
		createBlock(x, y, z);

		line++;
	}
	fclose(f);
}

static char* delim = ",()[] \t:\n";

static 
char** parseline(char* input)
{
  int wordcount = 0;
  int tlen = 0;

  int state = 0;
  int len = strlen(input);
  int i;
  for (i=0; i<len; i++) {
    int c = input[i];
    switch (state) {
    case 0:			/* getting delims */
      if (strchr(delim, c) == NULL) {
	state = 1;
	wordcount++;
	tlen += 2;
      }
      break;

    case 1:			/* getting a token */
      if (strchr(delim, c) != NULL) {
	state = 0;
      } else {
	tlen++;
      }
      break;
    }
  }
  if (wordcount == 0) return NULL;
  wordcount++;			/* to account for NULL at end of last token */
  char** tokens = malloc(wordcount*sizeof(char*)+tlen+3);
  char* string = (char*)(((char*)tokens)+(wordcount*sizeof(char*)));
  fprintf(stderr, "%p, %p, %d\n", tokens, string, (int)(wordcount*sizeof(char*)+tlen));
  state = 0;
  int tcnt = 0;
  for (i=0; i<len; i++) {
    int c = input[i];
    switch (state) {
    case 0:			/* getting delims */
      if (strchr(delim, c) == NULL) {
	state = 1;
	wordcount++;
	tokens[tcnt++] = string;
	*string++ = (char)c;
      }
      break;

    case 1:			/* getting a token */
      if (strchr(delim, c) != NULL) {
	*string++ = 0;
	state = 0;
      } else {
	*string++ = (char)c;
      }
      break;
    }
  }
  *string++ = 0;
  tokens[tcnt] = NULL;
  fprintf(stderr, "string = %p, tcnt=%d, %p\n", string, tcnt, &(tokens[tcnt]));

  for (i=0; tokens[i] != NULL; i++) {
    fprintf(stderr, "%d: %s %p\n", i, tokens[i], tokens[i]);
  }

  return tokens;
}

int** testData;
int testDataSize = 0;

void
dictAdd(int key, int* data)
{
  if (testDataSize == 0) {
    testDataSize = 8;
    testData = calloc(testDataSize, sizeof(int*));
  } else while (testDataSize <= key) {
    testData = realloc(testData, testDataSize*2*sizeof(int*));
    testDataSize = 2*testDataSize;
  }
  testData[key] = data;

  fprintf(stderr, "Node: %d ->", key);
  int i;
  for (i=0; i<9; i++) {
    fprintf(stderr, "\t%d", data[i]);
  }
  fprintf(stderr, "\n");
}

int 
configtest(char* name)
{
  int hasconf = 0;

  FILE* f = fopen(name, "r");
  if (f == NULL)
    err("Can't open %s for reading", name);

  int line = 1;
  while (!feof(f)) {
    char buffer[256];
    char* p = fgets(buffer, 256, f);
    if (p == NULL)
      break;
    char** tokens = parseline(p);
    if (tokens == NULL) continue;

    if (strcmp(tokens[0], "config")==0) {
      // check that config file exists
      FILE* f = fopen(tokens[1], "r");
      if (f == NULL) 
	err("Can't open %s for reading config", tokens[1]);
      fclose(f);
      configFile = strdup(tokens[1]);
      hasconf = 1;
    } else if (strcmp(tokens[0], "meld") == 0) {
      // ignore this
    } else if (isdigit(tokens[0][0])) {
      // this sets value of node
      // node, msgs, x, y, z, r, g, b, i
      int i;
      int* vals = calloc(9, sizeof(int));
      for (i=0; (tokens[i] != NULL)&&(i<9); i++)
	vals[i] = atoi(tokens[i]);
      if (i != 9)
	err("Illegal final spec line at %s:%d", name, line);
      dictAdd(vals[0], vals);
    } else
      err("Illegal syntax at %s:%d", name, line);
    line++;
    free(tokens);
  }
  fclose(f);
  return hasconf;
}
      
