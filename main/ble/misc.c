/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "esp_central.h"

/**
 * Utility function to log an array of bytes.
 */
void print_bytes(const uint8_t *bytes, int len) {
    int i;

    for (i = 0; i < len; i++) {
        printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

void print_mbuf(const struct os_mbuf *om) {
    int colon, i;

    colon = 0;
    while (om != NULL) {
        if (colon) {
            printf(":");
        } else {
            colon = 1;
        }
        for (i = 0; i < om->om_len; i++) {
            printf("%s0x%02x", i != 0 ? ":" : "", om->om_data[i]);
        }
        om = SLIST_NEXT(om, om_next);
    }
}

char * addr_str(const void *addr) {
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}

void print_uuid(const ble_uuid_t *uuid) {
    char buf[BLE_UUID_STR_LEN];

    printf("%s", ble_uuid_to_str(uuid, buf));
}

/**
 * Logs information about a connection to the console.
 */
void print_conn_desc(const struct ble_gap_conn_desc *desc) {
    printf("handle=%d our_ota_addr_type=%d our_ota_addr=%s ",
                desc->conn_handle, desc->our_ota_addr.type,
                addr_str(desc->our_ota_addr.val));
    printf("our_id_addr_type=%d our_id_addr=%s ",
                desc->our_id_addr.type, addr_str(desc->our_id_addr.val));
    printf("peer_ota_addr_type=%d peer_ota_addr=%s ",
                desc->peer_ota_addr.type, addr_str(desc->peer_ota_addr.val));
    printf("peer_id_addr_type=%d peer_id_addr=%s ",
                desc->peer_id_addr.type, addr_str(desc->peer_id_addr.val));
    printf("conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                       "encrypted=%d authenticated=%d bonded=%d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
    printf("\n");
}

#if CONFIG_EXAMPLE_EXTENDED_ADV
void
print_addr(const void *addr, const char *name)
{
    const uint8_t *u8p;
    u8p = addr;
    printf("%s = %02x:%02x:%02x:%02x:%02x:%02x",
                name, u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}
void
ext_print_adv_report(const void *param)
{
    const struct ble_gap_ext_disc_desc *disc = (struct ble_gap_ext_disc_desc *)param;
    printf("props=%d data_status=%d legacy_event_type=%d", disc->props, disc->data_status, disc->legacy_event_type);
    print_addr(disc->addr.val, "address");
    printf("rssi=%d tx_power=%d", disc->rssi, disc->tx_power);
    printf("sid=%d prim_phy=%d sec_phy=%d", disc->sid, disc->prim_phy, disc->sec_phy);
    printf("periodic_adv_itvl=%d length_data=%d", disc->periodic_adv_itvl, disc->length_data);
    print_addr(disc->direct_addr.val, "direct address");
}
#endif

void print_adv_fields(const struct ble_hs_adv_fields *fields) {
    char s[BLE_HS_ADV_MAX_SZ];
    const uint8_t *u8p;
    int i;

    if (fields->flags != 0) {
        printf("    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        printf("    uuids16(%scomplete)=",
                    fields->uuids16_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids16; i++) {
            print_uuid(&fields->uuids16[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->uuids32 != NULL) {
        printf("    uuids32(%scomplete)=",
                    fields->uuids32_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids32; i++) {
            print_uuid(&fields->uuids32[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->uuids128 != NULL) {
        printf("    uuids128(%scomplete)=",
                    fields->uuids128_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids128; i++) {
            print_uuid(&fields->uuids128[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->name != NULL) {
        assert(fields->name_len < sizeof s - 1);
        memcpy(s, fields->name, fields->name_len);
        s[fields->name_len] = '\0';
        printf("    name(%scomplete)=%s\n",
                    fields->name_is_complete ? "" : "in", s);
    }

    if (fields->tx_pwr_lvl_is_present) {
        printf("    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->slave_itvl_range != NULL) {
        printf("    slave_itvl_range=");
        print_bytes(fields->slave_itvl_range, BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        printf("\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        printf("    svc_data_uuid16=");
        print_bytes(fields->svc_data_uuid16, fields->svc_data_uuid16_len);
        printf("\n");
    }

    if (fields->public_tgt_addr != NULL) {
        printf("    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            printf("public_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        printf("\n");
    }

    if (fields->appearance_is_present) {
        printf("    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        printf("    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->svc_data_uuid32 != NULL) {
        printf("    svc_data_uuid32=");
        print_bytes(fields->svc_data_uuid32, fields->svc_data_uuid32_len);
        printf("\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        printf("    svc_data_uuid128=");
        print_bytes(fields->svc_data_uuid128, fields->svc_data_uuid128_len);
        printf("\n");
    }

    if (fields->uri != NULL) {
        printf("    uri=");
        print_bytes(fields->uri, fields->uri_len);
        printf("\n");
    }

    if (fields->mfg_data != NULL) {
        printf("    mfg_data=");
        print_bytes(fields->mfg_data, fields->mfg_data_len);
        printf("\n");
    }

    printf("\n");
}

void print_mbuf_data(const struct os_mbuf *om) {
    int i;

    printf("Data: ");
    while (om != NULL) {
        for (i = 0; i < om->om_len; i++) {
            printf(" %d", om->om_data[i]);
        }
        om = SLIST_NEXT(om, om_next);
    }
}