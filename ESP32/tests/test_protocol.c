/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../openvela_overlay/apps/system/pfcctl/pfcctl_protocol.h"

struct capture_s
{
  struct pfcctl_packet_s packet;
  unsigned int calls;
};

static bool capture_packet(void *context,
                           const struct pfcctl_packet_s *packet)
{
  struct capture_s *capture = context;

  capture->packet = *packet;
  capture->calls++;
  return true;
}

static void test_crc_reference(void)
{
  static const uint8_t data[] = "123456789";

  assert(pfcctl_crc16(data, 9u) == 0x4b37u);
}

static void test_round_trip_and_status(void)
{
  struct pfcctl_packet_s input;
  struct pfcctl_parser_s parser;
  struct pfcctl_status_s status;
  struct capture_s capture;
  uint8_t wire[PFCCTL_MAX_FRAME];
  size_t length;
  size_t index;

  memset(&input, 0, sizeof(input));
  memset(&capture, 0, sizeof(capture));
  input.command = PFCCTL_EVT_STATUS;
  input.sequence = 17u;
  input.length = 8u;
  input.payload[0] = 3u;
  input.payload[1] = 0u;
  input.payload[2] = 3u;
  input.payload[3] = 0x0cu;
  input.payload[4] = 0xc0u;
  input.payload[5] = 0x12u;
  input.payload[6] = 0xf4u;
  input.payload[7] = 0x01u;

  length = pfcctl_encode(&input, wire, sizeof(wire));
  assert(length == 17u);
  assert(wire[0] == 0xa5u && wire[1] == 0x5au);

  pfcctl_parser_init(&parser);
  for (index = 0u; index < length; index++)
    {
      (void)pfcctl_parser_feed(&parser, wire[index], capture_packet,
                               &capture);
    }

  assert(capture.calls == 1u);
  assert(capture.packet.sequence == 17u);
  assert(pfcctl_decode_status(&capture.packet, &status));
  assert(status.pfc_state == 3u);
  assert(status.llc_fault == 0x0cu);
  assert(status.voltage_cv == 4800u);
  assert(status.current_ca == 500u);
}

static void test_bad_crc_is_rejected(void)
{
  struct pfcctl_packet_s input;
  struct pfcctl_parser_s parser;
  struct capture_s capture;
  uint8_t wire[PFCCTL_MAX_FRAME];
  size_t length;
  size_t index;

  memset(&input, 0, sizeof(input));
  memset(&capture, 0, sizeof(capture));
  input.command = PFCCTL_CMD_TELEMETRY_REQUEST;
  input.sequence = 2u;
  length = pfcctl_encode(&input, wire, sizeof(wire));
  assert(length == 9u);
  wire[length - 1u] ^= 0x80u;

  pfcctl_parser_init(&parser);
  for (index = 0u; index < length; index++)
    {
      (void)pfcctl_parser_feed(&parser, wire[index], capture_packet,
                               &capture);
    }

  assert(capture.calls == 0u);
}

static void test_parser_resynchronizes(void)
{
  struct pfcctl_packet_s input;
  struct pfcctl_parser_s parser;
  struct capture_s capture;
  uint8_t wire[PFCCTL_MAX_FRAME];
  size_t length;
  size_t index;

  memset(&input, 0, sizeof(input));
  memset(&capture, 0, sizeof(capture));
  input.command = PFCCTL_CMD_HEARTBEAT;
  input.sequence = 9u;
  length = pfcctl_encode(&input, wire, sizeof(wire));

  pfcctl_parser_init(&parser);
  (void)pfcctl_parser_feed(&parser, 0x00u, capture_packet, &capture);
  (void)pfcctl_parser_feed(&parser, 0xa5u, capture_packet, &capture);
  (void)pfcctl_parser_feed(&parser, 0xa5u, capture_packet, &capture);
  for (index = 1u; index < length; index++)
    {
      (void)pfcctl_parser_feed(&parser, wire[index], capture_packet,
                               &capture);
    }

  assert(capture.calls == 1u);
  assert(capture.packet.command == PFCCTL_CMD_HEARTBEAT);
}

int main(void)
{
  test_crc_reference();
  test_round_trip_and_status();
  test_bad_crc_is_rejected();
  test_parser_resynchronizes();
  puts("pfcctl protocol tests: PASS");
  return 0;
}
