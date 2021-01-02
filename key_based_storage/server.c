/**
 *
 * Server version of key based storage rdma example application
 * */
#include <stdio.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

void run_server(struct ibv_device *device) {
    int ret;
    struct ibv_context *context;
    const int port_num = 1;
    const int gid_index = 0;
    const int num_cqe = 0x10;
    struct ibv_port_attr port_attr;
    union ibv_gid gid;
    struct ibv_pd *pd;
    struct ibv_cq *cq;

    context = ibv_open_device(device);

    if (context == NULL) {
      fprintf(stderr, "Failed to open device. %d:%s\n", errno, strerror(errno));
      exit(1);
    }

    ret = ibv_query_gid(context, port_num, gid_index, &gid);

    if (ret) {
        fprintf(stderr, "Failed to query gid. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    pd = ibv_alloc_pd(context);

    if (pd == NULL) {
        fprintf(stderr, "Failed to allocate protection domain. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    cq = ibv_create_cq(context, num_cqe, NULL, NULL, 0);

    if (cq == NULL) {
        fprintf(stderr, "Failed to create completion queue. %d:%s\n", errno, strerror(errno));
        exit(1);
    }


    /*
     * create a queue pair in state RESET
     */
    struct ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 1;
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.cap.max_send_wr = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;

    struct ibv_qp *qp;
    qp = ibv_create_qp(pd, &qp_init_attr);

    if (qp == NULL) {
        fprintf(stderr, "Failed to create queue pair. %d:%s\n", errno, strerror(errno));
        exit(1);
    }


    /*
     * change state from RESET to INIT
     */
    struct ibv_qp_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_state = IBV_QPS_INIT;
    init_attr.port_num = port_num;
    init_attr.pkey_index = 0;
    init_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    ret = ibv_modify_qp(qp, &init_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);

    if (ret) {
        fprintf(stderr, "Failed to reach INIT state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    /*
     * Change from state INIT to RTR
     *
     */

    struct ibv_qp_attr rtr_attr;
    memset(&rtr_attr, 0, sizeof(rtr_attr));

    rtr_attr.qp_state = IBV_QPS_RTR;
    rtr_attr.path_mtu = IBV_MTU_1024;
    rtr_attr.rq_psn = 0;
    rtr_attr.max_dest_rd_atomic = 1;
    rtr_attr.min_rnr_timer = 0x12;
    memset(&rtr_attr.ah_attr, 0, sizeof(rtr_attr.ah_attr));
    // rtr_attr.ah_attr.is_global = 0;
    rtr_attr.ah_attr.is_global = 1;
    rtr_attr.ah_attr.grh.dgid = gid;
    rtr_attr.ah_attr.sl = 0;
    rtr_attr.ah_attr.src_path_bits = 0;
    rtr_attr.ah_attr.port_num = port_num;

    uint32_t dest_qp;
    uint16_t destination_local_id;
    printf("our QPN: %d\n", qp->qp_num);

    struct ibv_port_attr port_attr_two;
    ret = ibv_query_port(context, port_num, &port_attr_two);

    if (ret) {
        fprintf(stderr, "Failed to query local id. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    printf("our local id: %d\n", port_attr_two.lid);
    printf("Enter a destination QP Number:\n");
    scanf("%" SCNd32, &dest_qp);
    printf("Enter a destination local id:\n");
    scanf("%" SCNd16, &destination_local_id);
    rtr_attr.dest_qp_num = dest_qp;
    rtr_attr.ah_attr.dlid = destination_local_id;

    ret = ibv_modify_qp(qp, &rtr_attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
    if (ret) {
        fprintf(stderr, "Failed to reach RTR state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }


    /* RTR -> RTS */

    int attr_mask = (IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_RETRY_CNT \
                     | IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT);


    struct ibv_qp_attr rts_attr;
    memset(&rts_attr, 0, sizeof(rts_attr));
    rts_attr.qp_state = IBV_QPS_RTS;
    rts_attr.sq_psn = 0;
    rts_attr.max_rd_atomic = 0;
    rts_attr.retry_cnt = 7;
    rts_attr.rnr_retry = 7;
    rts_attr.timeout = 18;

    ret = ibv_modify_qp(qp, &rts_attr, attr_mask);

    if (ret) {
        fprintf(stderr, "Failed to reach RTS state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    struct ibv_sge sge;
    struct ibv_recv_wr wr, *bad_wr;

    int mem_len = 0x100;
    char *mem = malloc(mem_len);

    struct ibv_mr *mr;

    mr = ibv_reg_mr(pd, mem, mem_len, IBV_ACCESS_LOCAL_WRITE);

    if (mr == NULL) {
        fprintf(stderr, "Failed to register memory region. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uint64_t) mem;
    sge.length =  mem_len;
    sge.lkey = mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    ret = ibv_post_recv(qp, &wr, &bad_wr);

    int poll_cq_ret = 0;
    if (ret) {
        fprintf(stderr, "Failed to post WRs to receive Queue. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    struct ibv_wc wc;
    memset(&wc, 0, sizeof(wc));

    do {

        poll_cq_ret = ibv_poll_cq(cq, num_cqe, &wc);
    } while(poll_cq_ret == 0);

    if (poll_cq_ret < 0) {
        fprintf(stderr, "Failed to poll cq. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d)  for wr_id %d\n", ibv_wc_status_str(wc.status), \
                                                                 wc.status, (int)wc.wr_id);
        exit(1);
    }

    printf("we received: %s\n", mem);
    ret = ibv_dereg_mr(mr);

    if (ret) {
        fprintf(stderr, "Failed to deregister mr. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    ibv_close_device(context);
}



int main(int argc, char **argv) {
    int ret;
    int num_devices;
    struct ibv_device *device;
    struct ibv_device **device_list;


    device_list = ibv_get_device_list(&num_devices);

    if(device_list == NULL) {
        fprintf(stderr, "Failed to get device list. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    device = device_list[0];

    run_server(device);

    ibv_free_device_list(device_list);

    return 0;
}
