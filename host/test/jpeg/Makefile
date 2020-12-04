.PHONY: clean

# NETLIB  ?= net-nng.o
RMEMIMPL ?= rmem-legomem.c

CFLAGS  := -Werror -g -I../../../include -DCONFIG_ARCH_X86 -O2
LD      := gcc
LDLIBS  := ${LDLIBS} -libverbs -lpthread -ljpeg

APPS    := arrayclient arrayserver
COMMON  := config.o common.o rarray.o $(NETLIB) $(RMEMIMPL)

TP:=../..
CLIOSRCS := $(TP)/board.c \
	$(TP)/cpu.c \
	$(TP)/util.c \
	$(TP)/api.c \
	$(TP)/context.c \
	$(TP)/session.c \
	$(TP)/common.c \
	$(TP)/watchdog.c \
	$(TP)/net/core.c \
	$(TP)/net/raw_verbs.c \
	$(TP)/net/transport_bypass.c \
	$(TP)/net/transport_gbn.c \
	$(TP)/net/transport_rpc.c \
	$(TP)/api_kvs.c \
	$(TP)/memory_model.c \
	$(TP)/lib/bitmap.c

all: ${APPS}

arrayclient: arrayclient.c $(COMMON)
	${LD} ${CFLAGS} -o $@ $^ ${LDLIBS}

arrayserver: arrayserver.c $(CLIOSRCS) $(COMMON)
	${LD} ${CFLAGS} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o ${APPS}

