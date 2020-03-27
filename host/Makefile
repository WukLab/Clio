INCLUDE := -I../include
CLIBS := -libverbs -pthread

SRCS := board.c \
	mgmt.c \
	util.c \
	api.c \
	context.c \
	session.c \
	net/core.c \
	net/raw_verbs.c \
	net/raw_socket.c \
	net/raw_udp_socket.c \
	net/transport_bypass.c \
	net/transport_gbn.c \
	test_session.c \
	test_migration.c

# or -g
DEBUG := -g

all: $(SRCS) 
	gcc $(DEBUG) -Wall -o host.o $(INCLUDE) main_host.c test.c $(SRCS) $(CLIBS)
	gcc $(DEBUG) -Wall -o monitor.o $(INCLUDE) main_monitor.c $(SRCS) $(CLIBS)
	gcc $(DEBUG) -Wall -o board_emulator.o $(INCLUDE) main_board.c $(SRCS) $(CLIBS)

clean:
	rm *.o
