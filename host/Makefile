INCLUDE := -I../include
CLIBS := -libverbs -pthread

SRCS := board.c \
	api.c \
	context.c \
	session.c \
	net/core.c \
	net/raw_verbs.c \
	net/raw_socket.c \
	net/udp_socket.c \
	net/transport_bypass.c \
	net/transport_gbn.c

all: $(SRCS) 
	gcc -Wall -o host.o $(INCLUDE) main_host.c test.c $(SRCS) $(CLIBS)
	gcc -g -Wall -o monitor.o $(INCLUDE) main_monitor.c $(SRCS) $(CLIBS)

clean:
	rm *.o
