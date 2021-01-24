#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdint.h>

int qp_reset_to_init(struct ibv_qp *qp, const int port_num) {

    struct ibv_qp_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_state = IBV_QPS_INIT;
    init_attr.port_num = port_num;
    init_attr.pkey_index = 0;
    init_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE  | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    int attr_mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |IBV_QP_ACCESS_FLAGS;

    return ibv_modify_qp(qp, &init_attr, attr_mask);
}


int qp_init_to_rtr(struct ibv_qp *qp, union ibv_gid gid, uint32_t dest_qp, uint16_t destination_local_id, uint8_t port_num) {

    struct ibv_qp_attr rtr_attr;
    memset(&rtr_attr, 0, sizeof(rtr_attr));

    rtr_attr.qp_state = IBV_QPS_RTR;
    rtr_attr.path_mtu = IBV_MTU_4096;
    rtr_attr.rq_psn = 0;
    rtr_attr.max_dest_rd_atomic = 1;
    rtr_attr.min_rnr_timer = 0x12;
    rtr_attr.dest_qp_num = dest_qp;

    memset(&rtr_attr.ah_attr, 0, sizeof(rtr_attr.ah_attr));
    rtr_attr.ah_attr.is_global = 1;
    rtr_attr.ah_attr.grh.dgid = gid;
    rtr_attr.ah_attr.sl = 0;
    rtr_attr.ah_attr.src_path_bits = 0;
    rtr_attr.ah_attr.port_num = port_num;
    rtr_attr.ah_attr.dlid = destination_local_id;

    int attr_mask = (IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN \
        | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);

    return ibv_modify_qp(qp, &rtr_attr, attr_mask);
}


int qp_rtr_to_rts(struct ibv_qp *qp) {

    struct ibv_qp_attr rts_attr;
    memset(&rts_attr, 0, sizeof(rts_attr));
    rts_attr.qp_state = IBV_QPS_RTS;
    rts_attr.sq_psn = 0;
    rts_attr.max_rd_atomic = 0;
    rts_attr.retry_cnt = 7;
    rts_attr.rnr_retry = 7;
    rts_attr.timeout = 18;

    int attr_mask = (IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_RETRY_CNT \
                     | IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT);

    return ibv_modify_qp(qp, &rts_attr, attr_mask);
}
