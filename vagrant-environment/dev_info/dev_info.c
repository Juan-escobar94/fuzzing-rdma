#include <infiniband/verbs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int run_server(struct ibv_context *ctx) {
  int ret;
  // for some reason. port number cannot be 0.
  const int port_num = 1;
  const int gid_index = 0;
  const int num_cqe = 16;
  struct ibv_port_attr port_attr;
  union ibv_gid gid;
  ret = ibv_query_port(ctx, port_num, &port_attr);

  if (ret != 0) {
      fprintf(stderr, "Failed to query port: %d: %s\n", errno, strerror(errno));
      exit(1);
  }

  ret = ibv_query_gid(ctx, port_num, gid_index, &gid);
  if (ret != 0) {
      fprintf(stderr, "Failed to query gid: %s\n", strerror(errno));
      exit(1);
  }
  int i = 0;
  for (; i < sizeof(gid.raw) - 1; i++) {
      printf("%X:", gid.raw[i]);
  }
  printf("%X", gid.raw[i]);
  printf("\n");

  struct ibv_pd *pd;

  pd = ibv_alloc_pd(ctx);

  if (pd == NULL) {
      fprintf(stderr, "Failed to allocate protection domain: %s\n", strerror(errno));
      exit(1);
  }
  struct ibv_qp *qp;
  struct ibv_qp_init_attr qp_init_attr;

  struct ibv_cq *cq;
  /*
  ** cq_context = NULL, normally a pointer that is set when a notification is received
  ** comp_channel = NULL, usually for blocking notifications. Receiving a msg over a socket requires a read syscall, you call read on your socket and return from read when the message is ready.
  ** with ibverbs the normal protocol is different: there is a read_like call (library call with no kernel involvement) it will check if the message error has arrived, and if msg has not arrived it returns
  ** immeadiatly, meaning there is no msg. There is also a posiblility to make a system call version, and for that there is this mechanism of completion channels.
  ** comp_vector = NULL, it is also only relevant for completion channel.
   * */
  cq = ibv_create_cq(ctx, num_cqe, NULL, NULL, 0);
  if (cq == NULL) {
      fprintf(stderr, "Failed to create CQ: %s\n", strerror(errno));
      exit(1);
  }

  qp_init_attr.qp_context = ctx;
  qp_init_attr.send_cq = cq;
  qp_init_attr.recv_cq = cq;
  qp_init_attr.srq = NULL;
  qp_init_attr.qp_type = IBV_QPT_RC;
  qp_init_attr.sq_sig_all = 1;
  qp_init_attr.cap.max_send_wr = 1;
  qp_init_attr.cap.max_recv_wr = 1;
  qp_init_attr.cap.max_send_sge = 1;
  qp_init_attr.cap.max_recv_sge = 1;
  qp_init_attr.cap.max_inline_data = 4;

  qp = ibv_create_qp(pd, &qp_init_attr);

  if (qp == NULL) {
      fprintf(stderr, "Failed to create QP: %s\n", strerror(errno));
      exit(1);
  }
  int attr_mask;
  struct ibv_qp_attr qp_attr;
  /* RESET -> INIT */
  qp_attr.qp_state = IBV_QPS_INIT;
  qp_attr.pkey_index = 0;
  qp_attr.port_num = port_num;
  qp_attr.qp_access_flags = 0;
  attr_mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

  ret = ibv_modify_qp(qp, &qp_attr, attr_mask);
  if (ret != 0) {
      fprintf(stderr, "Failed to react INIT state: %s\n", strerror(errno));
      exit(1);
  }

  printf("OUR QPN: %d\n", qp->qp_num);

  /* INIT -> Ready To Receive (RTR) */

  int dest_qpn;
  printf("Enter Destination QPN:\t");
  scanf("%d", &dest_qpn);


  // PSN = Packet Sequence Number is for sync purposes.
  attr_mask = (IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN
               | IBV_QP_RQ_PSN );

  qp_attr.qp_state = IBV_QPS_RTR;
  qp_attr.pkey_index = 0;
  qp_attr.path_mtu = IBV_MTU_256;
  qp_attr.dest_qp_num = dest_qpn;
  qp_attr.rq_psn =  0;
  // primary path address vector
  memset(&qp_attr, 0, sizeof(qp_attr.ah_attr));
  qp_attr.ah_attr.grh.dgid = gid;
  qp_attr.ah_attr.is_global = 1;
  qp_attr.ah_attr.port_num = port_num;

  ret = ibv_modify_qp(qp, &qp_attr, attr_mask);

  //FIXME: code is failing here, with errno 22 (invalid argument);
  if (ret != 0) {
      fprintf(stderr, "Failed to reach RTR state: %s\n", strerror(errno));
      exit(1);
  }
  /* RTR -> Ready To Send (RTS) */
  attr_mask = (IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_RETRY_CNT
               | IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT );

  qp_attr.qp_state = IBV_QPS_RTS;
  qp_attr.sq_psn = 0;
  qp_attr.max_rd_atomic = 0;
  qp_attr.retry_cnt = 7;
  qp_attr.rnr_retry = 7;
  qp_attr.timeout = 18;
  ret = ibv_modify_qp(qp, &qp_attr, attr_mask);
  if (ret != 0) {
      fprintf(stderr, "Failed to reach RTS state: %s\n", strerror(errno));
      exit(1);
  }

  // scatter / gather element attached to the request, when msg arrives it will store the entry
  // that we point to.
  struct ibv_sge sge;
  struct ibv_recv_wr wr, *bad_wr;

  int mem_len = 32;
  char *mem = malloc(mem_len);

  struct ibv_mr *mr;

  mr = ibv_reg_mr(pd, mem, mem_len, IBV_ACCESS_LOCAL_WRITE);
  if(mr == NULL) {
      fprintf(stderr, "failed to register memory region: %s\n", strerror(errno));
      exit(1);
  }
  sge.addr = (uint64_t) mem;
  sge.length = mem_len;
  sge.lkey = mr->lkey;

  wr.wr_id = 0;
  wr.next = NULL;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  ret = ibv_post_recv(qp, &wr, &bad_wr);
  if (ret != 0) {
      fprintf(stderr, "Failed to post WRs to recieve Queue %s\n", strerror(errno));
      exit(1);
  }


// do {
// } while (ret == 0);

  ret = ibv_dereg_mr(mr);
  if(ret != 0) {
      fprintf(stderr, "Failed to de-register memory region: %s\n", strerror(errno));
      exit(1);
  }

  return 0;
}

int main (int argc, char **argv) {
 int ret;
 int num_devices;
 struct ibv_context *context;
 struct ibv_device **device_list;
 device_list = ibv_get_device_list(&num_devices);

 if (device_list == NULL) {
     fprintf(stderr, "Failed to get device list: %s\n", strerror(errno));
     exit(1);
 }

 printf("Found %d devices:\n", num_devices);

 for (int i = 0; i < num_devices; i++) {
     const char *device_name;
     device_name = ibv_get_device_name(device_list[i]);
     printf("- %s\n", device_name);
 }
 struct ibv_device *device = device_list[0];
 context = ibv_open_device(device);

 if (context == NULL) {
     fprintf(stderr, "Failed to open device: %s\n", strerror(errno));
     exit(1);
 }

 ibv_free_device_list(device_list);

 run_server(context);

 ret = ibv_close_device(context);
 if(ret != 0) {
     fprintf(stderr, "Failed to close device: %s\n", strerror(errno));
     exit(1);
 }
 // 1:38:25
 //
 return 0;
}
