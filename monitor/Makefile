INCLUDE := -I../include/
CFLAGS :=

SRCS := $(wildcard *.c)

all: $(SRCS)
	gcc -o monitor.o $(CFLAGS) $(INCLUDE) $(SRCS) -lpthread

clean:
	rm -f *.o
