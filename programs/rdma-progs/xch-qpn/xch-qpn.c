#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>


/**
 *
 * creates a socket bound on bind_port, which will exchange connection information with
 * the communication partner. the result will be inside of conn_inf, with length ci_len.
 *
 * */
int xch_conn_inf_server(char* bind_port, char *qpn, char *guid, char *conn_inf, int ci_len) {

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

    sendto(sd, msg, strlen(msg), 0, (struct sockaddr*) res->ai_addr, sizeof *(res->ai_addr));

    freeaddrinfo(res);
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



    char buf[500];
    memset(&buf, 0, sizeof(buf));
    int received;
    // struct sockaddr_in server;
    // socklen_t len = sizeof server;
    // now send own qpn and guid to client
    char msg[100];
    sprintf(msg, "qpn:%s giud:%s", qpn, guid);

    /**
     * first, send your information to the server
     **/
    sendto(sd_partner, msg, sizeof msg, 0, (struct sockaddr*) res->ai_addr, sizeof *(res->ai_addr));

    /**
     * await for servers response
     **/
    received = recvfrom(sd, &buf, sizeof buf, 0, NULL, 0);

    strncpy(conn_inf, buf, ci_len);




    freeaddrinfo(res);
    return 0;

err3:
    shutdown(sd, SHUT_RDWR);
err2:
    freeaddrinfo(res);
err1:
    return 1;
}
