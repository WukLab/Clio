INCLUDE := -I../../include
CFLAGS := -libverbs -lpthread -lm

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

all: $(OBJS)

%.o: %.c
	gcc -o $@ $(CFLAGS) $(INCLUDE) $<

clean:
	rm *.o
