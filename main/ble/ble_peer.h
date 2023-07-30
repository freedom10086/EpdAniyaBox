#ifndef H_ESP_CENTRAL_
#define H_ESP_CENTRAL_

#include "modlog/modlog.h"
#include <string.h>
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Peer. */
struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;

    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;

    struct peer_chr_list chrs;
};
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer;

typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;

    uint16_t conn_handle;

    /** List of discovered GATT services. */
    struct peer_svc_list svcs;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;

    /** Callback that gets executed when service discovery completes. */
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};

int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb, void *disc_cb_arg);

const struct peer_dsc *peer_dsc_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid);

const struct peer_chr *peer_chr_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid);

struct peer_svc * peer_svc_find_range(struct peer *peer, uint16_t attr_handle);

const struct peer_svc *peer_svc_find_uuid(const struct peer *peer, const ble_uuid_t *uuid);

int peer_delete(uint16_t conn_handle);

int peer_add(uint16_t conn_handle);

int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);

struct peer *peer_find(uint16_t conn_handle);


#ifdef __cplusplus
}
#endif

#endif