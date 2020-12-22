#IBVERBS_INSTALL=$(shell readlink -f ../rdma-core-install)
IBVERBS_INSTALL=/opt/vagrant/rdma-core/build
CFLAGS=-I${IBVERBS_INSTALL}/include -L${IBVERBS_INSTALL}/lib -Wl,-rpath=${IBVERBS_INSTALL}/lib
LDLIBS=-libverbs
LDFLAGS=-L${IBVERBS_INSTALL}/lib -libverbs
all: dev_info
