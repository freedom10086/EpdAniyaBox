#ifndef H_GATTS_SENS_
#define H_GATTS_SENS_

#include "nimble/ble.h"
#include "modlog/modlog.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_svr_init(void);
void gatt_svr_handle_subscribe(struct ble_gap_event *event, void *arg);
void gatt_svr_notify_tx_event(struct ble_gap_event *event, void *arg);
int gatt_svr_deinit(void);
#ifdef __cplusplus
}
#endif

#endif
