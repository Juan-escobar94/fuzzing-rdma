#include <linux/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>

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

void print_buf_bytes(char *buf, int buf_size) {

  // dont modify the actual buf.
  char *p = buf;
  int bytes_range = 0x0000;
  printf("received: \n");
  int k = 0;
  while ( k <= buf_size){
    printf("%04X: ", bytes_range & 0xFFFF);
    for (int i = 1; i <= 16; i++){
      if (k >= buf_size) return;

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

int main() {
  char dev2_name[IFNAMSIZ];
  char dev3_name[IFNAMSIZ];
  int tap2fd = -1;
  int tap3fd = -1;
  strcpy(dev2_name, "tap2");
  strcpy(dev3_name, "tap3");

  int nread;
  int nread2;
  char buffer[4096];
  memset(buffer, 0, 4096);
  char buffer2[4096];
  memset(buffer2, 0, 4096);


  tap2fd = tun_alloc(dev2_name, IFF_TAP);
  if (tap2fd < 0) {
    puts("error");
    return 1;
  }

  printf("connected to device with name %s\n", dev2_name);

  tap3fd = tun_alloc(dev3_name, IFF_TAP);
  if (tap3fd < 0) {
    puts("error");
    return 1;
  }

  printf("connected to device with name %s\n", dev3_name);

  int pid = fork();

  if (pid) {
    while (1) {
    nread = read(tap3fd, buffer2, sizeof(buffer));
    printf("read %d from %s\n", nread, dev3_name);

    write(tap2fd, buffer2, nread);
    memset(buffer2, 0, 4096);
    }
  }
  else {
    while (1) {
      nread = read(tap2fd, buffer, sizeof(buffer));
      printf("read %d from %s\n", nread, dev2_name);
      print_buf_bytes(buffer, nread);
      write(tap3fd, buffer, nread);
      memset(buffer, 0, 4096);
    }
  }
  return 0;
}
