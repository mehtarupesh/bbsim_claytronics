CC=gcc
CFLAGS = -Wall -Werror -g -pthread

ifeq ($(ARCH)x,x)
 X := $(shell uname -m)
 Y := $(shell uname -s)
 Y := $(subst /,-,$Y)
 ARCH := $X-$Y
 $(info Setting ARCH to $(ARCH))
endif

TARGET := sim
SOURCES := csapp.c block.c click.c config.c menu.c render.c sim.c viewer.c world.c msg.c mem.c heap.c
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

top:	$(TARGET)

-include Makefile.externs
Makefile.externs:	Makefile
	$(CC) -MM $(CFLAGS) $(SOURCES) > Makefile.externs

$(TARGET): $(OBJECTS)
ifeq ($Y,Darwin)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) -framework OpenGL -framework GLUT -lpthread
else
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) -lglut -lGLU -lGL -lpthread
endif

%.o:	%.c
	$(CC) -c $(CFLAGS) -o $@ $<

install:	$(TARGET)
	cp $(TARGET) bin/arch-$(ARCH)

.PHONY: clean
clean:
	/bin/rm -f *.o

.PHONY: reallyclean
reallyclean:	clean
	/bin/rm -f *.o $(TARGET)
	/bin/rm -f bin/arch-$(ARCH)/$(TARGET)


