INCLUDE := -I../include
CLIBS := -libverbs -lrt -pthread

TEST_SRCS := test_session.c \
	     test_migration.c \
	     test_raw_net.c \
	     test_board.c

C_SRCS := board.c \
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

# Default sed regexp - multiline due to syntax constraints
define sed-y
	"/^->/{s:->#\(.*\):/* \1 */:; \
	s:^->\([^ ]*\) [\$$#]*\([-0-9]*\) \(.*\):#define \1 \2 /* \3 */:; \
	s:^->\([^ ]*\) [\$$#]*\([^ ]*\) \(.*\):#define \1 \2 /* \3 */:; \
	s:->::; p;}"
endef

# Use filechk to avoid rebuilds when a header changes,
# but the resulting file does not
define filechk_offsets
	(set -e; \
	 echo "#ifndef $2"; \
	 echo "#define $2"; \
	 echo "/*"; \
	 echo " * DO NOT MODIFY."; \
	 echo " *"; \
	 echo " * This file was auto-generated"; \
	 echo " */"; \
	 echo ""; \
	 sed -ne $(sed-y); \
	 echo ""; \
	 echo "#endif" )
endef

# filechk is used to check if the content of a generated file is updated.
# Sample usage:
# define filechk_sample
#	echo $KERNELRELEASE
# endef
# version.h : Makefile
#	$(call filechk,sample)
# The rule defined shall write to stdout the content of the new file.
# The existing file will be compared with the new one.
# - If no file exist it is created
# - If the content differ the new file is used
# - If they are equal no change, and no timestamp update
# - stdin is piped in from the first prerequisite ($<) so one has
#   to specify a valid file as first prerequisite (often the kbuild file)
define filechk
	echo '  CHK     $@';		        \
	mkdir -p $(dir $@);			\
	$(filechk_$(1)) < $< > $@.tmp;		\
	if [ -r $@ ] && cmp -s $@ $@.tmp; then	\
		rm -f $@.tmp;			\
	else					\
		echo '  UPD     $@';	        \
		mv -f $@.tmp $@;		\
	fi
endef

bounds.h: bounds.s
	$(call filechk,offsets,__LEGOMEM__)
