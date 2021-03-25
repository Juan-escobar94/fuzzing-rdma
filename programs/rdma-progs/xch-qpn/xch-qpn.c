#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

int xch_conn_inf_server(char* bind_port, char *qpn, char *guid, char *conn_inf, int ci_len) {
    //char* conn_inf = malloc(100);
    //memset(&conn_inf, 0, sizeof conn_inf);

    int status;
    int sd;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, bind_port, &hints, &res);
    if (status != 0) {
        perror("getaddrinfo");
        goto err1;
    }

    sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        perror("socket");
        goto err2;
    }

    status = bind(sd, res->ai_addr, res-> ai_addrlen);
    if (status != 0) {
        perror("bind");
        goto err3;
    }

    char buf[500];
    memset(&buf, 0, sizeof(buf));
    int received;
    struct sockaddr_in client;
    socklen_t len = sizeof client;
    // wait to receive qpn and guid information from the application
    received = recvfrom(sd, &buf, sizeof buf, 0, (struct sockaddr*) &client, &len);

    strncpy(conn_inf, buf, ci_len);

    // now send own qpn and guid to client
    char msg[100];
    sprintf(msg, "qpn:%s giud:%s", qpn, guid);

    sendto(sd, msg, strlen(msg), 0, (struct sockaddr*) &client, len);


    return 0;

err3:
    shutdown(sd, SHUT_RDWR);
err2:
    freeaddrinfo(res);
err1:
    return 1;
}

int xch_conn_inf_client(char* serv_port, char *serv_name,  char* bind_port, \
                        char *qpn, char *guid, char *conn_inf, int ci_len) {
    int status;
    int sd;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, bind_port, &hints, &res);
    if (status != 0) {
        perror("getaddrinfo");
        goto err1;
    }

    sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        perror("socket");
        goto err2;
    }

    status = bind(sd, res->ai_addr, res-> ai_addrlen);
    if (status != 0) {
        perror("bind");
        goto err3;
    }

    char buf[500];
    memset(&buf, 0, sizeof(buf));
    int received;
    struct sockaddr_in client;
    socklen_t len = sizeof client;
    // wait to receive qpn and guid information from the application
    received = recvfrom(sd, &buf, sizeof buf, 0, (struct sockaddr*) &client, &len);

    strncpy(conn_inf, buf, ci_len);

    // now send own qpn and guid to client
    char msg[100];
    sprintf(msg, "qpn:%s giud:%s", qpn, guid);

    sendto(sd, msg, strlen(msg), 0, (struct sockaddr*) &client, len);


    return 0;

err3:
    shutdown(sd, SHUT_RDWR);
err2:
    freeaddrinfo(res);
err1:
    return 1;
}
