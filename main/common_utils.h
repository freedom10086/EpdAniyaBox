//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_COMMON_UTILS_H
#define EPD_ANIYA_BOX_COMMON_UTILS_H

#include <string.h>
#include "host/ble_hs.h"

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);

void print_mbuf(const struct os_mbuf *om);

void print_mbuf_data(const struct os_mbuf *om);

char *addr_str(const void *addr);

void print_uuid(const ble_uuid_t *uuid);

void print_addr(const void *addr);

void print_conn_desc(const struct ble_gap_conn_desc *desc);

void print_adv_fields(const struct ble_hs_adv_fields *fields);

void ext_print_adv_report(const void *param);

#endif //EPD_ANIYA_BOX_COMMON_UTILS_H
