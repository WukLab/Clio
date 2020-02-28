INCLUDE := -I../include
CFLAGS := -libverbs -lpthread

SRCS := core.c \
	net/core.c \
	net/raw_verbs.c \
	net/raw_socket.c \
	net/udp_socket.c \
	net/transport_bypass.c \
	net/transport_gbn.c

all: $(SRCS) 
	gcc -Wall -o host.out $(CFLAGS) $(INCLUDE) $(SRCS)

clean:
	rm *.out
