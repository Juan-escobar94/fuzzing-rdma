#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

struct conn_resources {
  int sd_xch;                        /* socket used for listening information exchange */
  int sd_rdma;                       /* socket for listening rdma comms */
  struct addrinfo* res_client_xch;   /* resolved address for client conn information exchange: gid, etc. */
  struct addrinfo* res_server_xch;   /* resolved server for client conn information exchange: gid, etc. */
  /* TODO: define addrinfos for client and server for rdma traffic. */
};



/* struct that holds all config parameters */
struct config_params {
  char* rdma_port;              /* host binds this port, forwards rdma traffic using it */
  char* exchange_port;          /* host binds this port, forwards connection information traffic using it */
  char* client_address;         /* IP address at which the client is available: eth0 inside client VM */
  char* server_address;         /* IP address at which the server is available: eth0 inside client VM */
  char* proxy_address_client;   /* IP address at which tap interface is available at the host, connected to eth0 on client vm */
  char* proxy_address_server;   /* IP address at which tap interface is available at the host, connected to eth0 on server vm */
  unsigned char verbose;        /* set verbose = 1 for verbose mode */
};

struct config_params config = {
.rdma_port = "4791",
.exchange_port = "4793",
.client_address = "172.0.0.15",
.server_address = "172.0.0.16",
.proxy_address_client = "172.0.0.1", /* note to self: add this addr to tap0 */
.proxy_address_server = "172.0.0.2", /* note to self: add this addr to tap1 */
.verbose = 1
};

/* initialize conn_resources struct to predictable values */
void init_conn_resources(struct conn_resources* conn_res) {
  memset(conn_res, 0, sizeof(struct conn_resources));
  conn_res->sd_xch = -1;
  conn_res->sd_rdma = -1;
  conn_res->res_client_xch = NULL;
  conn_res->res_server_xch = NULL;
}

/**
 *  prepare_conn_resources
 *  fill conn_res with socket file descriptors used for
 *  rdma traffic and connection information interchange, respectively
 *
 *  resolves client and server addresses and stores
 *  them in conn_res addrinfo pointers.
 *
 **/
int prepare_conn_resources(struct conn_resources* conn_res) {
  int sd_xch = -1;
  int sd_rdma = -1;
  int status = -1;
  int rc = 0;

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_INET;

  /* set up the information exchange socket */
  status = getaddrinfo(config.proxy_address_client, config.exchange_port, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    rc = 1;
    goto prepare_sock_exit;
  }

  sd_xch = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sd_xch < 0) {
    fprintf(stderr, "failed to create exchange socket: %s\n", strerror(errno));
    rc = 1;
    goto prepare_sock_exit;
  }

  status = bind(sd_xch, res->ai_addr, res->ai_addrlen);
  if (status != 0) {
    fprintf(stderr, "failed to bind exchange socket: %s\n", strerror(errno));
    rc = 1;
    goto prepare_sock_exit;
  }

  freeaddrinfo(res);

  memset(&hints, 0, sizeof hints);
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_INET;

  /* set up rdma traffic socket */
  status = getaddrinfo(config.proxy_address_client, config.rdma_port, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    rc = 1;
    goto prepare_sock_exit;
  }

  sd_rdma = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sd_rdma < 0) {
    fprintf(stderr, "failed to create rdma traffic socket: %s\n", strerror(errno));
    rc = 1;
    goto prepare_sock_exit;
  }

  status = bind(sd_rdma, res->ai_addr, res->ai_addrlen);
  if (status != 0) {
    fprintf(stderr, "failed to bind rdma traffic socket: %s\n", strerror(errno));
    rc = 1;
    goto prepare_sock_exit;
  }

  struct addrinfo* client_res;
  struct addrinfo* server_res;

  /* resolve xch address for client */
  status = getaddrinfo(config.client_address, config.exchange_port, &hints, &client_res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    rc = 1;
    goto prepare_sock_exit;
  }

 status = getaddrinfo(config.server_address, config.exchange_port, &hints, &server_res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    rc = 1;
    goto prepare_sock_exit;
  }


prepare_sock_exit:
  if (rc == 0) {
    conn_res->sd_rdma = sd_rdma;
    conn_res->sd_xch = sd_xch;
    conn_res->res_client_xch = client_res;
    conn_res->res_server_xch = server_res;
  } else {
    freeaddrinfo(client_res);
    freeaddrinfo(server_res);
  }
  freeaddrinfo(res);
  return rc;
}

int fwd_exchange_data(struct conn_resources* conn_res) {
  char buf[4096];
  memset(buf, 0, 4096);
  int rc = 0;
  int i = 0;
  int received_bytes = 0;

  char sender_hostname[NI_MAXHOST];
  char sender_port[NI_MAXSERV];
  struct sockaddr_storage sender;
  socklen_t sender_len = sizeof(struct sockaddr_storage);

  /* forward exactly 2 packets of connection information  */
  for (;i < 2;) {
    received_bytes = recvfrom(conn_res->sd_xch, buf, 4096, 0, (struct sockaddr*)&sender, &sender_len);
    getnameinfo((struct sockaddr*)&sender, sender_len, sender_hostname, sizeof sender_hostname,
                sender_port, sizeof sender_port, NI_NUMERICHOST | NI_NUMERICSERV);

    if (strncmp(config.client_address, sender_hostname,
                strlen(config.client_address)) == 0) {
      if (config.verbose) printf("[v]: client exchange information received, forwarding...\n");
      sendto(conn_res->sd_xch, buf, received_bytes, 0,
             conn_res->res_server_xch->ai_addr,
             conn_res->res_server_xch->ai_addrlen);
      i += 1;
      continue;
    } else if (strncmp(config.server_address, sender_hostname,
                       strlen(config.server_address)) == 0) {
      if (config.verbose) printf("[v]: server exchange information received, forwarding...\n");
      sendto(conn_res->sd_xch, buf, received_bytes, 0,
             conn_res->res_server_xch->ai_addr,
             conn_res->res_server_xch->ai_addrlen);
      i +=1;
      continue;
    } else {
      if(config.verbose){
        printf("[v]: unknown sender: %s:%s\n", sender_hostname, sender_port);
        printf("[v]: dropping packet…\n");
      }
    }
  }

  close(conn_res->sd_xch);
  conn_res->sd_xch = -1;
  return rc;
}

int main(int argc, char* argv[]) {
  int rc;
  struct conn_resources conn_res;

  /* prepare sockets and resolve client and server addresses. */
  init_conn_resources(&conn_res);
  rc = prepare_conn_resources(&conn_res);
  if (rc != 0) {
    printf("\nfailed to prepare connection resources\n");
    exit(1);
  }

  if(config.verbose) printf("[v]: resources ready, awaiting information exchange...\n");
  /* forward exchange information */
  fwd_exchange_data(&conn_res);

  /* forward normal rdma traffic */
  //TODO
  return 0;
}
