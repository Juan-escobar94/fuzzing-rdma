#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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

/* all bth headers were defined with 1 byte diff, meaning this enum covers all scenarios */
enum bth_header_offsets {
IB_OPCODE, // = IB_OPCODE_OFFSET - IB_BTH_OFFSET,
IB_SMPT, // = IB_SMPT_OFFSET - IB_BTH_OFFSET,
IB_PKEY_B1, // = IB_PKEY_OFFSET - IB_BTH_OFFSET,
IB_PKEY_B2, // = IB_PKEY_OFFSET - IB_BTH_OFFSET,
IB_FR_BR_RESERVED , //= IB_FR_BR_RESERVED_OFFSET - IB_BTH_OFFSET,
IB_QP_B1, // = IB_DEST_QP_B1_OFFSET - IB_BTH_OFFSET,
IB_QP_B2, // = IB_DEST_QP_B2_OFFSET - IB_BTH_OFFSET,
IB_QP_B3, // = IB_DEST_QP_B3_OFFSET - IB_BTH_OFFSET,
IB_AR, // = IB_AR_OFFSET - IB_BTH_OFFSET,
IB_PSN_B1, // = IB_PSN_B1_OFFSET - IB_BTH_OFFSET,
IB_PSN_B2, // = IB_PSN_B2_OFFSET - IB_BTH_OFFSET,
IB_PSN_B3, // = IB_PSN_B3_OFFSET - IB_BTH_OFFSET
};


void fuzz_atomic_eth(char* atomic_eth) {
  const int atomic_eth_len = 28;
  printf("fuzzing atomic extension headers\n");
  int rand_hdr_offset  = rand() % atomic_eth_len;
  unsigned char random_byte = (unsigned char) rand() % (0xFF + 1);
  atomic_eth[rand_hdr_offset] = atomic_eth[rand_hdr_offset] ^ random_byte;
}

void fuzz_rdma_eth(char* rdma_eth) {
  const int rdma_eth_len = 16;
  printf("fuzzing rdma extension headers\n");
  int rand_hdr_offset  = rand() % rdma_eth_len;
  unsigned char random_byte = (unsigned char) rand() % (0xFF + 1);
  rdma_eth[rand_hdr_offset] = rdma_eth[rand_hdr_offset] ^ random_byte;
}



void fuzz_ib_bth(char* ib_bth, int bth_size, int rounds) {
  for (int i = 0; i < rounds; i++) {
    enum bth_header_offsets rand_ib_hdr_offset = (enum bth_header_offsets) rand() % bth_size;
    /* Normally, fuzzers do mutations incrementally */
    /* but since this application relies on output */
    /* being produces by another machine, it arguably */
    /* better to just XOR a 'truly' random byte */
    unsigned char random_byte = (unsigned char) rand() % (0xFF + 1);
    switch(rand_ib_hdr_offset) {
      case IB_OPCODE:
        ib_bth[IB_OPCODE] = ib_bth[IB_OPCODE] ^ random_byte;
        break;
      case IB_SMPT:
        /* 0xF8 = 0b11111000 */
        /* tver is bits 5-8, should not be modified */
        ib_bth[IB_SMPT] = ib_bth[IB_SMPT] ^ (random_byte & 0xF0);
        break;
      case IB_PKEY_B1:
        ib_bth[IB_PKEY_B1] = ib_bth[IB_PKEY_B1] ^ random_byte;
        break;
      case IB_PKEY_B2:
        ib_bth[IB_PKEY_B2] = ib_bth[IB_PKEY_B2] ^ random_byte;
        break;
      case IB_FR_BR_RESERVED:
        ib_bth[IB_FR_BR_RESERVED] = ib_bth[IB_FR_BR_RESERVED] ^ random_byte;
        break;
      case IB_QP_B1:
        /* changing QPN to a non valid number does not trigger new behaviour */
        break;
      case IB_QP_B2:
        /* changing QPN to a non valid number does not trigger new behaviour */
        break;
      case IB_QP_B3:
        /* changing QPN to a non valid number does not trigger new behaviour */
        break;
      case IB_AR:
        ib_bth[IB_AR] = ib_bth[IB_AR] ^ (random_byte);
        break;
      case IB_PSN_B1:
        ib_bth[IB_PSN_B1] = ib_bth[IB_PSN_B1] ^ (random_byte);
        break;
      case IB_PSN_B2:
        ib_bth[IB_PSN_B2] = ib_bth[IB_PSN_B2] ^ (random_byte);
        break;
      case IB_PSN_B3:
        ib_bth[IB_PSN_B3] = ib_bth[IB_PSN_B3] ^ (random_byte);
        break;
    }

    u_int8_t opcode = ib_bth[IB_OPCODE];
    char op_mask = 0x1f;

    if ((opcode & op_mask) == 0x13 || (opcode & op_mask) == 0x14) {
      /* Atomic-ETH */
      char* a_eth = (char *) (ib_bth + IB_PSN_B3 + 1);
      fuzz_atomic_eth(a_eth);
    } else if ((opcode & op_mask) == 0x07 || (opcode & op_mask) == 0x0A || (opcode & op_mask) == 0x0B || (opcode & op_mask) == 0x0C) {
      /* RDMA-ETH */
      char* rdma_eth = (char *) (ib_bth + IB_PSN_B3 + 1);
      fuzz_rdma_eth(rdma_eth);
    }
  }
}


struct config_params {
  char* client_dev_name;
  char* server_dev_name;
  char* log_file;
};

struct config_params config = {
 .client_dev_name = "tap2",
 .server_dev_name = "tap3",
 .log_file = "./packet_dump.log"
};

int main(int argc, char* argv[]) {

  srand(time(NULL));
  int opt;
  char client_dev_name[IFNAMSIZ];
  char server_dev_name[IFNAMSIZ];

  memset(client_dev_name, 0, IFNAMSIZ);
  memset(server_dev_name, 0, IFNAMSIZ);

  int client_tap_fd = -1;
  int server_tap_fd = -1;

  while((opt = getopt(argc, argv, "c:s:")) != -1)
    {
        switch(opt)
        {
          case 'c':
            if (strlen(optarg) < IFNAMSIZ) {
              printf("read arg %s for client\n", optarg);
              strcpy(client_dev_name, optarg);
            } else {
                exit(EXIT_FAILURE);
            }
            break;
            case 's':
            if (strlen(optarg) < IFNAMSIZ) {
              printf("read arg %s for server\n", optarg);
              strcpy(server_dev_name, optarg);
            } else {
                exit(EXIT_FAILURE);
            }
            break;
            default:
                printf("unknown option: %c\n", optopt);
                exit(EXIT_FAILURE);
                break;
        }
   }

  if (strlen(client_dev_name) < 3) {
    strcpy(client_dev_name, config.client_dev_name);
  }

  if (strlen(server_dev_name) < 3) {
    strcpy(server_dev_name, config.server_dev_name);
  }

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

      // dump_buffer_bytes(buffer_client, nread_from_client);

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
          /* TODO: add a random number of rounds as third param instead of 1 */
          fuzz_ib_bth(ib_bth, IB_PSEUD_PAYLOAD_OFFSET - IB_BTH_OFFSET, 1);
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
