INCLUDE := -I../include
CLIBS := -libverbs -lrt -pthread

TEST_SRCS := test_session.c \
	     test_migration.c \
	     test_raw_net.c \
	     test_board.c

C_SRCS := board.c \
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
	net/transport_gbn.c

SRCS := $(C_SRCS) $(TEST_SRCS)

# or -g
DEBUG := -g

all: $(SRCS) 
	gcc $(DEBUG) -Wall -o host.o $(INCLUDE) main_host.c $(SRCS) $(CLIBS)
	gcc $(DEBUG) -Wall -o monitor.o $(INCLUDE) main_monitor.c $(SRCS) $(CLIBS)
	gcc $(DEBUG) -Wall -o board_emulator.o $(INCLUDE) main_board.c $(SRCS) $(CLIBS)

clean:
	rm *.o
