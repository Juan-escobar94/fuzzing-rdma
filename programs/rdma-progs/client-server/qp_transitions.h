#include <stdint.h>

#ifndef QP_TRANSITIONS
#define QP_TRANSITIONS
int qp_reset_to_init(struct ibv_qp *qp, const int port_num);
int qp_init_to_rtr(struct ibv_qp *qp, union ibv_gid gid, uint32_t dest_qp, uint16_t destination_local_id, uint8_t port_num);
int qp_rtr_to_rts(struct ibv_qp *qp);
#endif /* QP_TRANSITIONS */
