#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

void print_usage(char *p) {
    printf("Usage: %s port\n", p);
}



int main (int argc, char** argv) {

    if (argc != 2) {
        print_usage(argv[0]);
        exit(1);
    }

    int status;
    int sd_listen, sd_forward;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    // AF_UNSPEC: use either IPv4 or IPv6
    // AF_INET: use IPv4
    // AF_INET6: use IPv6
    hints.ai_family = AF_INET;
    // use UDP
    hints.ai_socktype = SOCK_DGRAM;
    // fill in my own IP for me
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, argv[1], &hints, &res);
    if (status != 0) {
      perror("getaddrinfo");
      exit(1);
    }

    sd_listen = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd_listen == -1) {
      perror("socket");
      exit(1);
    }

    int optval = 1;
    setsockopt(sd_listen, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);


    status = bind(sd_listen, res->ai_addr, res->ai_addrlen);
    if (status != 0) {
      perror("bind");
      exit(1);
    }

    printf("ready to receive on port %s\n", argv[1]);

    /*
     *
     * now prepare the data structures to forward the message
     *
     **/
    freeaddrinfo(res);

    fflush(stdout);
    char buf[1024];
    char* msg;
    memset(&buf, 0, sizeof(buf));
    int received;

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    struct sockaddr_storage sender;
    unsigned int sender_len = sizeof(struct sockaddr_storage);


    while ((received = recvfrom(sd_listen, &buf, sizeof buf, 0, (struct sockaddr*) &sender, &sender_len)) != -1) {
      char* p = &buf[0];
      while(*p != '\0') {
        printf(" %c ", *p);
        p++;
      }
      fflush(stdout);

  
      getnameinfo((struct sockaddr*)&sender, sender_len, host, sizeof host, service, sizeof service, NI_NUMERICHOST | NI_NUMERICSERV);
       
      printf("got from host: %s, port: %s\n", host, service); 

      /**
       *

       *
       **/

      sendto(sd_forward, &buf, sizeof buf, 0, (struct sockaddr*) res->ai_addr, sizeof *(res->ai_addr));
      memset(&buf, 0, sizeof(buf));
      memset(&sender, 0, sizeof(sender));
      sender_len = sizeof(struct sockaddr_storage);
    }

    return 0;
}
