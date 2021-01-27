#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

int open(const char *pathname, int flags) {
    int (*open_real)(const char*, int) = dlsym(RTLD_NEXT, "open");
    printf(ANSI_COLOR_GREEN "Opening file: %s" ANSI_COLOR_RESET "\n", pathname);
    int fd = open_real(pathname, flags);
    printf(ANSI_COLOR_GREEN "got file descriptor: %d" ANSI_COLOR_RESET  "\n", fd);
    return fd;
}


int ioctl(int fd, unsigned long request, char *argp) {
  int (*ioctl_real)(int, unsigned long, char*) = dlsym(RTLD_NEXT, "ioctl");
	printf(ANSI_COLOR_GREEN "Calling ioctl with fd:%d, request:%lu, argp: %d" ANSI_COLOR_RESET "\n", fd, request, argp);
	return ioctl_real(fd, request, argp);

}
