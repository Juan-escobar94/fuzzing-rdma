/**
 *
 * Client version of key based storage rdma example appplication
 * */
#include <stdio.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include "qp_transitions.h"

void run_client(struct ibv_device *device) {
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
    }    pd = ibv_alloc_pd(context);

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
    ret = qp_reset_to_init(qp, port_num);
    if (ret) {
        fprintf(stderr, "Failed to reach INIT state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    /*
     * Change from state INIT to RTR
     */
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

    ret = qp_init_to_rtr(qp, gid, dest_qp, destination_local_id, port_num);
    if (ret) {
        fprintf(stderr, "Failed to reach RTR state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    /* RTR -> RTS */
    ret = qp_rtr_to_rts(qp);
    if (ret) {
        fprintf(stderr, "Failed to reach RTS state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }


    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr;

    memset(&wr, 0, sizeof(wr));

    int mem_len = 0x100;
    char *mem = malloc(mem_len); //TODO dealloc mem

    // strcpy(mem, "hello");

    //printf("message to send: ");
    // FIXME: dont use scanf
    // scanf("%s\n", mem);
    //fgets(mem, mem_len, stdin);
    //mem = "hello";

    struct ibv_mr *mr;

    mr = ibv_reg_mr(pd, mem, mem_len, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE \
                                      | IBV_ACCESS_REMOTE_READ); //TODO: need to deregister mr

    sge.addr = (uintptr_t) mem;
    sge.length = mem_len;
    sge.lkey = mr->lkey;

    //printf("we made it so far1\n");
    if(!mr) {
        fprintf(stderr, "Failed to register mr. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    //printf("we made it so far2\n");

    char input_buf[0x100];
    while(1) {

        memset(input_buf, 0, sizeof(input_buf));

        memset(&wr, 0, sizeof(wr));
        printf("input message: ");
        scanf("%50s", input_buf);

        strcpy(mem, input_buf);

        wr.wr_id = 0;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_SEND;
        wr.send_flags = IBV_SEND_SIGNALED;

        //printf("we made it so far3\n");
        ret = ibv_post_send(qp, &wr, &bad_wr);

        if (ret) {
            fprintf(stderr, "ibv_post_send() failed. %d:%s\n", errno, strerror(errno));
            exit(1);
        }

    }

    printf("we made it so far\n");

    //free(mem);
    //mem = NULL;
    ibv_close_device(context);
}



int main(int argc, char **argv) {
    int ret;
    int num_devices;
    struct ibv_device *device = NULL;
    struct ibv_device **device_list;


    size_t dev_name_size = 50;
    char *dev_name = malloc(dev_name_size);

    /**
     *
     * Open a device specified by a command line arg.
     * Or print usage when -h or incorrect number of args.
     *
     * */
    char *program_name = argv[0];

    if (argc < 2) {
       printf("usage: %s %s\n", program_name, "devicename");
       exit(1);
    }

    strcpy(dev_name, argv[1]);
    dev_name = realloc(dev_name, strlen(dev_name) + 1);

    device_list = ibv_get_device_list(&num_devices);

    if(device_list == NULL) {
        fprintf(stderr, "Failed to get device list. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    for (int i = 0; i < num_devices; i++) {
        puts(device_list[i]->name);
        if(strcmp(dev_name, device_list[i]->name) == 0){
            printf("found device: %s\n", dev_name);
            device = device_list[i];
        }
    }

    if(!device) {
        fprintf(stderr, "no device found with name %s\n", dev_name);
        exit(1);
    }

    run_client(device);

    ibv_free_device_list(device_list);

    return 0;
}
