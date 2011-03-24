include ../../Makedefs

OBJS = hal_exports.o hal_main.o hal_ports.o hal_config.o

OUTPUT = wrsw_hal

CC=$(CROSS_COMPILE_ARM)gcc 
CFLAGS=-I. -g -Wall -I../libwripc -I../include -I../3rdparty/include -DDEBUG
LDFLAGS=-L../libwripc -L../libswitchhw -lswitchhw -lwripc -L../3rdparty/lib -llua -lm

all: $(OBJS)
			make -C ../libswitchhw
			${CC} -o $(OUTPUT) $(OBJS) $(LDFLAGS)
			
%.o:  %.c
			${CC} -c $^ $(CFLAGS)
			
deploy: all
			mkdir -p $(WR_INSTALL_ROOT)/bin
			cp $(OUTPUT) $(WR_INSTALL_ROOT)/bin
			
upload: all
			scp $(OUTPUT) root@192.168.1.16:/wr/bin
#			scp $(OUTPUT) root@192.168.1.13:/wr/bin
			
run:	upload
			#- ssh -t root@$(T) "export LD_LIBRARY_PATH=/wr/lib && /wr/bin/$(OUTPUT)"

run:

clean:
			rm -f $(OUTPUT) $(OBJS)