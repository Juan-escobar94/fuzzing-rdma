#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include "xch-qpn.h"




static int prepare_msg(char* qpn, uint8_t* gid, char *msg, int msg_size);

static int parse_conn_inf(char* msg, int msg_size, struct conn_inf *cinf);
/**
 *
 * creates a socket bound on bind_port, which will exchange connection information with
 * the communication partner. the result will be inside of conn_inf, with length ci_len.
 *
 * */
int xch_conn_inf_server(char* bind_port, char *qpn, uint8_t *gid, struct conn_inf *cinf) {

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


    freeaddrinfo(res);
    status = getaddrinfo("10.0.2.10", "4791", &hints, &res);
    if (status != 0) {
        perror("getaddrinfo");
        goto err1;
    }

    int sd_partner = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd_partner == -1) {
        perror("socket");
        goto err2;
    }

    char msg_recv[100];
    memset(&msg_recv, 0, sizeof(msg_recv));
    struct sockaddr_in client;
    socklen_t len = sizeof client;
    // wait to receive qpn and guid information from the application
    int received_len = recvfrom(sd, &msg_recv, sizeof msg_recv, 0, (struct sockaddr*) &client, &len);

    // now send own qpn and guid to client
    char msg_send[100];
    status = prepare_msg(qpn, gid, msg_send, sizeof(msg_send));

    if(!status) {
        fprintf(stderr, "prepare_msg failed\n");
        goto err3;
    }

    sendto(sd, msg_send, sizeof msg_send, 0, (struct sockaddr*) res->ai_addr, sizeof *(res->ai_addr));

    status = parse_conn_inf(msg_recv, received_len, cinf);

    if(!status) {
        fprintf(stderr, "parse_conn_inf failed\n");
        goto err3;
    }

    shutdown(sd, SHUT_RDWR);
    freeaddrinfo(res);
    return 0;

err3:
    shutdown(sd, SHUT_RDWR);
err2:
    freeaddrinfo(res);
err1:
    return 1;
}

int xch_conn_inf_client(char* bind_port, \
                        char *qpn, uint8_t *gid, struct conn_inf *cinf) {
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

    freeaddrinfo(res);

    status = getaddrinfo("10.0.2.10", "4791", &hints, &res);
    if (status != 0) {
        perror("getaddrinfo");
        goto err1;
    }

    int sd_partner = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd_partner == -1) {
        perror("socket");
        goto err2;
    }



    char msg_recv[100];
    memset(&msg_recv, 0, sizeof(msg_recv));

    // struct sockaddr_in server;
    // socklen_t len = sizeof server;
    // now send own qpn and guid to client
    char msg_send[100];
    status = prepare_msg(qpn, gid, msg_send, sizeof(msg_send));

    if(!status) {
        fprintf(stderr, "prepare_msg failed\n");
        goto err3;
    }

    /**
     * first, send your information to the server
     **/
    sendto(sd_partner, msg_send, sizeof msg_send, 0, (struct sockaddr*) res->ai_addr, sizeof *(res->ai_addr));

    /**
     * await for servers response
     **/
    int received_len = recvfrom(sd, &msg_recv, sizeof msg_recv, 0, NULL, 0);


    status = parse_conn_inf(msg_recv, received_len, cinf);

    if(!status) {
        fprintf(stderr, "parse_conn_inf failed\n");
        goto err3;
    }


    shutdown(sd, SHUT_RDWR);
    freeaddrinfo(res);
    return 0;

err3:
    shutdown(sd, SHUT_RDWR);
err2:
    freeaddrinfo(res);
err1:
    return 1;
}




static int prepare_msg(char* qpn, uint8_t * gid, char *msg, int msg_size){
    if (msg_size < 20) return 1;

    memset(msg, 0, msg_size);
    const char *str_gid ="gid:";

    sprintf(msg, "%s", str_gid);

    char *p = msg;
    while (*p != 0x00) p++;

    memcpy(p, gid, GID_RAW_SIZE);

    p += GID_RAW_SIZE;

    sprintf(p, " qpn:%s", qpn);
    return 0;
}


static int parse_conn_inf(char* msg, int msg_size, struct conn_inf *cinf) {

    if (msg_size < 20) {
        fprintf(stderr, "msg received is too short!\n");
        return 1;

    }

    memset(cinf->gid, 0, GID_RAW_SIZE);
    uint8_t *msg_p = (uint8_t *)msg;
    msg_p += 4;
    uint8_t *gid_p = (cinf->gid);
    for (int i = 0; i < GID_RAW_SIZE; i++) {
        *gid_p = *msg_p;
        gid_p++;
        msg_p++;
    }

    sscanf(msg, "qpn:%d", &(cinf->qpn));

    return 0;
}
