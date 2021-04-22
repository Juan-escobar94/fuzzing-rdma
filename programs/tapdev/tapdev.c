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


int main() {
    char dev_name[IFNAMSIZ];
    int tapfd = -1;
    strcpy(dev_name, "tap2");
    int nread;
    char buffer[4096];
    memset(buffer, 0, 4096);
   tapfd = tun_alloc(dev_name, IFF_TAP);
   if (tapfd < 0 ) {
        puts("error");
        return 1;
    }

    printf("connected to device with name %s\n", dev_name);
    while(1) {
        /* spinlock */
      nread = read(tapfd, buffer, sizeof(buffer));
      printf("read %d from %s\n", nread, dev_name);
    }

   return 0;
}
