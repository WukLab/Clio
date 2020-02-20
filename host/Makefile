INCLUDE := -I../include
CFLAGS := -libverbs -lpthread

SRCS := core.c \
	net/core.c \
	net/raw_verbs.c \
	net/raw_socket.c \
	net/transport_bypass.c \
	net/transport_gbn.c

all: $(SRCS) 
	gcc -Wall -o host.o $(CFLAGS) $(INCLUDE) $(SRCS)

clean:
	rm *.o
