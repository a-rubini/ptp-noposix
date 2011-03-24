include ../../Makedefs

CC=$(CROSS_COMPILE_ARM)gcc
AR=$(CROSS_COMPILE_ARM)ar
OBJS = hal_client.o ptpd_netif.o

CFLAGS = -I. -O2 -I../include -I../wrsw_hal -I../libwripc -I../../kernel/include
OUTPUT = libptpnetif.a

all:	$(OBJS) netif_test.o
	${AR} rc $(OUTPUT) $(OBJS)
	${CC} -o netif_test $(OBJS) netif_test.o -L../libwripc -lwripc -lm

%.o:	%.c
	${CC} -c $^ $(CFLAGS) -o $@

clean:
	rm -f $(OUTPUT) $(OBJS)
	
run: all
			- scp netif_test root@192.168.1.5:/tmp
			- scp netif_test root@192.168.1.16:/tmp
			
deploy:
	