INCLUDE := -I../../../include/
CFLAGS :=

SRCS := $(wildcard *.c)

all: $(SRCS)
	gcc -o core.o $(CFLAGS) $(INCLUDE) $(SRCS) -lpthread

clean:
	find ./ -name "*.o" | xargs rm -rf
