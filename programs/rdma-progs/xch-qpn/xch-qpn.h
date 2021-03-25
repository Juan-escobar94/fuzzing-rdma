#ifndef __XCH_QPN_H_
#define __XCH_QPN_H_



int xch_conn_inf_server(char* bind_port, char *qpn, char *guid, char *conn_inf, int ci_len);

int xch_conn_inf_client(char* serv_port, char *serv_name,  char* bind_port, \
                        char *qpn, char *guid, char *conn_inf, int ci_len);

#endif // __XCH-QPN_H_
