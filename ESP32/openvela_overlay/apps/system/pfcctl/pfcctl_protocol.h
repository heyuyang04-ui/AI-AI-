/* SPDX-License-Identifier: Apache-2.0 */

#ifndef PFCCTL_PROTOCOL_H
#define PFCCTL_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PFCCTL_VERSION 1u
#define PFCCTL_MAX_PAYLOAD 32u
#define PFCCTL_MAX_FRAME (7u + PFCCTL_MAX_PAYLOAD + 2u)

enum pfcctl_command_e
{
  PFCCTL_CMD_SET = 0x01u,
  PFCCTL_CMD_HEARTBEAT = 0x02u,
  PFCCTL_CMD_TELEMETRY_REQUEST = 0x03u,
  PFCCTL_EVT_ACK = 0x80u,
  PFCCTL_EVT_REJECT = 0x81u,
  PFCCTL_EVT_SET_REPLY = 0x82u,
  PFCCTL_EVT_STATUS = 0x10u
};

struct pfcctl_packet_s
{
  uint8_t command;
  uint8_t sequence;
  uint16_t length;
  uint8_t payload[PFCCTL_MAX_PAYLOAD];
};

struct pfcctl_parser_s
{
  uint8_t buffer[PFCCTL_MAX_FRAME];
  uint16_t used;
  uint16_t expected;
};

struct pfcctl_status_s
{
  uint8_t pfc_state;
  uint8_t pfc_fault;
  uint8_t llc_state;
  uint8_t llc_fault;
  uint16_t voltage_cv;
  uint16_t current_ca;
};

typedef bool (*pfcctl_packet_cb_t)(void *context,
                                   const struct pfcctl_packet_s *packet);

uint16_t pfcctl_crc16(const uint8_t *data, uint16_t length);
void pfcctl_parser_init(struct pfcctl_parser_s *parser);
bool pfcctl_parser_feed(struct pfcctl_parser_s *parser, uint8_t byte,
                        pfcctl_packet_cb_t callback, void *context);
size_t pfcctl_encode(const struct pfcctl_packet_s *packet, uint8_t *output,
                     size_t capacity);
bool pfcctl_decode_status(const struct pfcctl_packet_s *packet,
                          struct pfcctl_status_s *status);

#endif
