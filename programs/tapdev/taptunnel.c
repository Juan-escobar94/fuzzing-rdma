#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "net_types.h"

#define MAX_ETH_LEN 1542

int tun_alloc(char* dev, int flags) {

    struct ifreq ifr;
    int fd, err;
    char* clonedev = "/dev/net/tun";

    if ( (fd = open(clonedev, O_RDWR)) < 0 ) {
        perror("open");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
        perror("ioctl");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}

void dump_buffer_bytes(char *buf, int buf_size) {

  /* dont move the actual buf pointer.*/
  char *p = buf;
  int bytes_range = 0x0000;
  printf("received: \n");
  int k = 0;
  while ( k <= buf_size){
    printf("%04X: ", bytes_range & 0xFFFF);
    for (int i = 1; i <= 16; i++){
      if (k >= buf_size) {
        printf("\n");
        return;
      }
      printf("%02X", (*p) & 0xFF);
      if (i % 2 == 0 ) printf(" ");
      k++;
      p++;
    }
    printf("\n");
    bytes_range += 16;
  }
  printf("\n");
}

enum buffer_type {
IP_PKT,
UDP_SEG,
IB_TRAFFIC,
OTHER
};

enum buffer_type check_buffer_type(char *buffer, int buffer_size) {

  /* dont move the actual pointer */
  char *p_buffer_copy = buffer;
  if (buffer_size < IB_BTH_OFFSET)
    return OTHER;

  /* test wether eth frame is IPv4 */
  if (p_buffer_copy[ETH_TYPE_OFFSET] == 0x08 &&
      p_buffer_copy[ETH_TYPE_OFFSET + 1] == 0x00) {
    /* test ip packet is udp  and IHL is 5*/
    if (p_buffer_copy[PROTOCOL_OFFSET] == 0x11 &&
        (p_buffer_copy[VER_IHL_OFFSET] & 0x0F) == 0x05) {
      /* test wether udp segment is ib traffic (port 4791 == 0x12B7) */
      if (p_buffer_copy[DEST_PORT_OFFSET] == 0x12 &&
          (p_buffer_copy[DEST_PORT_OFFSET + 1] & 0xFF) == 0xB7) {
        return IB_TRAFFIC;
      } else {
        return UDP_SEG;
      }
    } else {
      return IP_PKT;
    }
  } else {
    return OTHER;
  }
}

void fuzz_ib_bth(char* ib_bth, int bth_size) {

}


struct config_params {
  char* client_dev_name;
  char* server_dev_name;
};

struct config_params config = {
 .client_dev_name = "tap2",
 .server_dev_name = "tap3"
};

int main() {

  printf("eth type offset: %d\n", ETH_TYPE_OFFSET);
  printf("ip ver offset: %d\n", VER_IHL_OFFSET);
  printf("ip dscp offset: %d\n", DSCP_ECN_OFFSET);
  printf("ip total len offset: %d\n", TOTAL_LEN_OFFSET);
  printf("ip identification offset: %d\n", IDENTIFICATION_OFFSET);
  printf("ip flags fragoffset offset: %d\n", FLAGS_FRAG_OFFSET_OFFSET);
  printf("ip ttl offset: %d\n", TTL_OFFSET);
  printf("ip protocol offset: %d\n", PROTOCOL_OFFSET);
  printf("dest port offset: %d\n", DEST_PORT_OFFSET);
  int a;
  scanf("%d", &a);

  char client_dev_name[IFNAMSIZ];
  char server_dev_name[IFNAMSIZ];
  int client_tap_fd = -1;
  int server_tap_fd = -1;
  strcpy(client_dev_name, config.client_dev_name);
  strcpy(server_dev_name, config.server_dev_name);

  int nread_from_client, nread_from_server;

  char buffer_client[MAX_ETH_LEN];
  memset(buffer_client, 0, MAX_ETH_LEN);
  char buffer_server[MAX_ETH_LEN];
  memset(buffer_server, 0, MAX_ETH_LEN);


  client_tap_fd = tun_alloc(client_dev_name, IFF_TAP);
  if (client_tap_fd < 0) {
    puts("error");
    return 1;
  }

  printf("connected to device with name %s\n", client_dev_name);

  server_tap_fd = tun_alloc(server_dev_name, IFF_TAP);
  if (server_tap_fd < 0) {
    puts("error");
    return 1;
  }

  printf("connected to device with name %s\n", server_dev_name);

  int pid = fork();

  /* server to client communication channel, no buffer alterations */
  if (pid) {
    while (1) {
      nread_from_server = read(server_tap_fd, buffer_server, sizeof(buffer_server));
      printf("read %d from %s\n", nread_from_server, server_dev_name);

      write(client_tap_fd, buffer_server, nread_from_server);
      memset(buffer_server, 0, nread_from_server);
    }
  }
  /* client to server communication channel, fuzzed infiniband bth(s) */
  else {
    while (1) {
      nread_from_client = read(client_tap_fd, buffer_client, sizeof(buffer_client));
      printf("read %d from %s\n", nread_from_client, client_dev_name);

      dump_buffer_bytes(buffer_client, nread_from_client);

      enum buffer_type buf_type = check_buffer_type(buffer_client, nread_from_client);
      switch(buf_type) {
        case IP_PKT:
          printf("buffer is IPv4 packet\n");
          break;
        case UDP_SEG:
          printf("buffer is IPv4 and contains UDP Segment\n");
          break;
        case IB_TRAFFIC:
          printf("buffer contains IB traffic\n");
          char* ib_bth = &buffer_client[IB_BTH_OFFSET];
          fuzz_ib_bth(ib_bth, IB_PSEUD_PAYLOAD_OFFSET - IB_BTH_OFFSET);
          break;
        default:
          printf("traffic is irrelevant\n");
          break;
      }
      write(server_tap_fd, buffer_client, nread_from_client);
      memset(buffer_client, 0, nread_from_client);
    }
  }
  return 0;
}

/**
 *
 *
 *  FRIENDLY REMINDER:  XX = 1 byte, XXXX = 2 bytes
 *
 *
 *
 *         4 byte      6 byte        6 byte
 *        | ----- | |  dst mac   | |   src mac  |
 *  0000: 0000 0800 DEAD BEEF 9756 DEAD BEEF 7EA2
 *
 *        eth: ipv4    IP hdr
 *                     v:4 ihl:4
 *                     dscp-ecn:0      tlen id   flags
 *        2 byte                                /froff   ttl/prot  crc  src ip
 *  0010: 0800         4500            002F 3E04 4000      4011     A49A AC00
 *
 *                         udp
 *                         src  dsr
 *        addr dst ip ad   prt  prt  len  chk
 *  0020: 000F AC00 0010   B2AC 0FC8 001B 6EB7 6161
 *
 *  0000: 0000 0800 DEAD BEEF 9756 DEAD BEEF 7EA2
 *  0010: 0800 4500 002F 3E04 4000 4011 A49A AC00
 *  0020: 000F AC00 0010 B2AC 0FC8 001B 6EB7 6161
 *  0030: 6161 6161 6161 6161 6161 6161 6161 6161
 *  0040: 0A
 *
 *  0010: 4500 002F 3E04 4000 4011 A49A AC00 000F
 *  0020: AC00 0010 B2AC 0FC8 001B 6EB7 6161 6161
 *  0030: 6161 6161 6161 6161 6161 6161 6161
 *  0040: 0A
 *
 **/
