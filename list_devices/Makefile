IBVERBS_INSTALL := ${shell readlink -f ../rdma-core/build}
CFLAGS := -I${IBVERBS_INSTALL}/include -L${IBVERBS_INSTALL}/lib -Wl,-rpath=${IBVERBS_INSTALL}/lib
LDLIBS := -libverbs
LDFLAGS := -L${IBVERBS_INSTALL}/lib ${LDLIBS}
CC := gcc
DEBUG := -g
list_devices: list_devices.o
	@echo "call ${CC} to build $@ from $<"
	${CC} ${CFLAGS}  -o list_devices.out list_devices.o ${LDFLAGS} 

list_devices.o: 
	${CC} ${CFLAGS}  -c list_devices.c ${LDLIBS}

dlist_devices.o:
	${CC} ${CFLAGS} -g -c list_devices.c ${LDLIBS}

debug: dlist_devices.o
	${CC} ${CFLAGS} -g -o list_devices.out list_devices.o ${LDFLAGS}

clean:
	rm -f *.o *.out list_devices 
