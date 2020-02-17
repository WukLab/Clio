INCLUDE := -I../include
CFLAGS := -libverbs -lpthread

SRCS := core.c \
	net/core.c \
	net/raw_verbs.c \
	net/raw_socket.c

all: $(SRCS) 
	gcc -o host.o $(CFLAGS) $(INCLUDE) $(SRCS)

clean:
	rm *.o
