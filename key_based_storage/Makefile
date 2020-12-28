IBVERBS_INSTALL := ${shell readlink -f ../rdma-core/build}
CFLAGS := -I${IBVERBS_INSTALL}/include -L${IBVERBS_INSTALL}/lib -Wl,-rpath=${IBVERBS_INSTALL}/lib
LDLIBS := -libverbs
LDFLAGS := -L${IBVERBS_INSTALL}/lib ${LDLIBS}
CC := gcc
DEBUG := -g

all: client server

server: server.o
	${CC} ${CFLAGS}  -o server.out server.o ${LDFLAGS}

server.o: server.c
	${CC} ${CFLAGS}  -c server.c ${LDLIBS}

client: client.o
	${CC} ${CFLAGS}  -o client.out client.o ${LDFLAGS}

client.o: client.c
	${CC} ${CFLAGS}  -c client.c ${LDLIBS}

#debug: dlist_devices.o
#  ${CC} ${CFLAGS} -g -o list_devices.out list_devices.o ${LDFLAGS}

clean:
	rm -f *.o *.out
