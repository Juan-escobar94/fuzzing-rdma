/**
 *
 * Server version of key based storage rdma example application
 * */
#include <stdio.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h> // getopt()
#include "qp_transitions.h"
#define DEBUG

void run_server(struct ibv_device *device, const char* db_file) {
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
    ret = qp_reset_to_init(qp, port_num);
    if (ret) {
        fprintf(stderr, "Failed to reach INIT state. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    /*
     * Change from state INIT to RTR
     *
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

    //memset(&wr, 0, sizeof(wr));
    //wr.wr_id = 0;
    //wr.next = NULL;
    //wr.sg_list = &sge;
    //wr.num_sge = 1;

    //ret = ibv_post_recv(qp, &wr, &bad_wr);

    //int poll_cq_ret = 0;
    //if (ret) {
    //    fprintf(stderr, "Failed to post WRs to receive Queue. %d:%s\n", errno, strerror(errno));
    //    exit(1);
    //}

    //struct ibv_wc wc;
    //memset(&wc, 0, sizeof(wc));

    /**
     *
     * Open database file passed in as a param.
     *
     * */
    FILE *db;
    db = fopen(db_file, "a+");
    if(!db) {
        perror("fopen(db_file)");
    }

    char buffer[mem_len];
    memset(buffer, 0, sizeof(buffer));
    /**
     * Server listening loop
     * */
    while (1) {
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

        strcpy(buffer, mem);
        printf("we received: %s\n", mem);
        memset(mem, 0, mem_len);
        memset(buffer, 0, sizeof(buffer));
        poll_cq_ret = 0;
    }
    ret = ibv_dereg_mr(mr);

    free(mem);

    if (ret) {
        fprintf(stderr, "Failed to deregister mr. %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    ibv_close_device(context);
}


void print_usage(const char* program_name);

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
       print_usage(program_name);
       printf("usage: %s %s\n", program_name, "devicename");
       exit(1);
    }

    #ifdef DEBUG
    puts("read command line args");
    #endif

    char ch;
    char *db_file = malloc(30);
    int is_specified_db = 0;

    #ifdef DEBUG
    puts("allocates space for db_file");
    #endif

    strcpy(dev_name, argv[1]);
    dev_name = realloc(dev_name, strlen(dev_name) + 1);


    while ((ch = getopt(argc, argv, "hd:")) != EOF) {
      switch(ch) {
        case 'd':
          strcpy(db_file, optarg);
          is_specified_db = 1;
          break;
        case 'h':
          //todo: refactor into print usage function
          print_usage(program_name);
          break;
        default:
          fprintf(stderr, "Unknown option: '%s'\n", optarg);
          exit(1);
       }
    }


    #ifdef DEBUG
    puts("read command line args");
    #endif

    if (!is_specified_db) strcpy(db_file, "db.txt");
    db_file = realloc(db_file, strlen(db_file) +1);

    #ifdef DEBUG
    puts("set db file");
    #endif
    printf("using %s as a db file\n", db_file);


    device_list = ibv_get_device_list(&num_devices);
    #ifdef DEBUG
    puts("queried device list");
    #endif

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

    #ifdef DEBUG
    puts("device was found, running server");
    #endif
    run_server(device, db_file);

    #ifdef DEBUG
    puts("server run exited");
    #endif

    free(dev_name);
    dev_name = NULL;
    free(db_file);
    db_file = NULL;
    ibv_free_device_list(device_list);

    return 0;
}

void print_usage(const char* program_name) {
    printf("usage: %s %s [options]\n", program_name, "devicename");
    puts("[options]");
    puts("\t-d db_filename: specify db file to read and write to.");
    puts("\t-h: print this message.");
    exit(1);
}
