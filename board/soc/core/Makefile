INCLUDE := -I../../../include/
CFLAGS := -Wno-stringop-truncation

SRCS := $(wildcard *.c)

all: $(SRCS)
	gcc -O2 -Wall -o core.o $(CFLAGS) $(INCLUDE) $(SRCS) -lpthread

clean:
	find ./ -name "*.o" | xargs rm -rf
