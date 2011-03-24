include ../../Makedefs

OBJS_LIB = wr_ipc.o
OBJS_EXAMPLES = test_srv.o test_cli.o

OUTPUT = libwripc.so

ifeq ($(TARGET),ARM)
			CC=$(CROSS_COMPILE_ARM)gcc 
			AR=$(CROSS_COMPILE_ARM)ar 
			OBJS_LIB+=helper_arm.o 
else 
			CC=gcc 
			AR=ar 
endif

CFLAGS=-I. -g -Wall -fPIC 
LDFLAGS=-fPIC

all: libwripc examples

libwripc: $(OBJS_LIB)
			${CC} -shared -o $(OUTPUT) $(OBJS_LIB) $(LDFLAGS)
#			- scp libwripc.so root@192.168.1.6:/wr/lib
#			- scp libwripc.so root@192.168.1.13:/wr/lib

examples: $(OBJS_EXAMPLES) $(OBJS_LIB)
			${CC} -g -o test_srv test_srv.o wr_ipc.o helper_arm.o -L.
			${CC} -g -o test_cli test_cli.o wr_ipc.o helper_arm.o -L.
#			scp test_srv test_cli root@192.168.1.13:/tmp

%.o:  %.c
			${CC} -c $^ $(CFLAGS)
#			${CC} -S $^ $(CFLAGS)
			
deploy:all
			mkdir -p $(WR_INSTALL_ROOT)/lib
			cp $(OUTPUT) $(WR_INSTALL_ROOT)/lib

clean:
			rm -f $(OUTPUT) $(OBJS_EXAMPLES) $(OBJS_LIB)
			
run: libwripc
			- scp libwripc.so root@$(T):/wr/lib
