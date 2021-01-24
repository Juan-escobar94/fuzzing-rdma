#include <infiniband/verbs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

void print_device_information(struct ibv_device **device_list, int num_devices) {
  int ret = 0;

  printf("Found %d device(s):\n", num_devices);

  for (int i = 0; i < num_devices; i++) {
    const char *device_name;
    device_name = ibv_get_device_name(device_list[i]);
    printf("- %s\n", device_name);

    struct ibv_device *device = device_list[i];
    struct ibv_context *context = ibv_open_device(device);

    if (context == NULL) {
      fprintf(stderr, "Failed to open device. %d:%s\n", errno, strerror(errno));
      // could also be continue
      exit(1);
    }

    const int port_num = 1;
    const int gid_index = 0;
    union ibv_gid gid;
    struct ibv_port_attr port_attr;

    ret = ibv_query_port(context, port_num, &port_attr);

    if (ret != 0) {
      fprintf(stderr, "Failed to query port. %d:%s\n", errno, strerror(errno));
      exit(1);
    }

    ret = ibv_query_gid(context, port_num, gid_index, &gid);
    if (ret != 0) {
      fprintf(stderr, "Failed to query gid. %d:%s\n", errno, strerror(errno));
      exit(1);
    }

    printf("\t");
    int j = 0;
    for (; j< sizeof(gid.raw) - 1; j++) {
      printf("%X:", gid.raw[j]);
    }
    printf("%X\n", gid.raw[j]);
  }
}


int main(int argc, char **argv) {
  int ret;
  int num_devices;
  struct ibv_device **device_list;

  device_list = ibv_get_device_list(&num_devices);

  if(device_list == NULL){
    fprintf(stderr, "Failed to get device list. %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  print_device_information(device_list, num_devices);

  ibv_free_device_list(device_list);
  return 0;
}
