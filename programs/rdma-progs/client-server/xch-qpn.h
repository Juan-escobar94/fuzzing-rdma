#ifndef __XCH_QPN_H_
#define __XCH_QPN_H_

#define GID_RAW_SIZE 16
#include <stdint.h>

struct conn_inf {
    uint8_t gid[GID_RAW_SIZE];
    int qpn;
};

int xch_conn_inf_server(char* bind_port, char *qpn, uint8_t *gid, struct conn_inf *cinf);

int xch_conn_inf_client(char* bind_port, \
                        char *qpn, uint8_t *gid, struct conn_inf *cinf);

#endif // __XCH-QPN_H_
