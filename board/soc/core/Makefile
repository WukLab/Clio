#
# Copyright (c) 2020, Wuklab, UCSD. All rights reserved.
#
INCLUDE := -I../../../include/

CFLAGS := -Wno-stringop-truncation
#CFLAGS += -DCONFIG_ZCU106_SOC -DCONFIG_ARCH_ARM64
CFLAGS += -DCONFIG_ARCH_X86
CFLAGS += -rdynamic

SRCS := $(wildcard *.c)
SRCS += $(wildcard test/*.c)

#CC := aarch64-linux-gnu-gcc
CC := gcc

all: $(SRCS)
	$(CC) -O2 -Wall -o core.o $(CFLAGS) $(INCLUDE) $(SRCS) -lpthread

clean:
	find ./ -name "*.o" | xargs rm -rf
