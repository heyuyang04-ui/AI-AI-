/* SPDX-License-Identifier: Apache-2.0 */

#include "pfcctl_protocol.h"

#include <string.h>

static uint16_t pfcctl_read_le16(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void pfcctl_write_le16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
}

uint16_t pfcctl_crc16(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xffffu;
  uint16_t index;
  uint8_t bit;

  for (index = 0; index < length; index++)
    {
      crc ^= data[index];
      for (bit = 0; bit < 8u; bit++)
        {
          crc = (crc & 1u) != 0u
                    ? (uint16_t)((crc >> 1) ^ 0xa001u)
                    : (uint16_t)(crc >> 1);
        }
    }

  return crc;
}

void pfcctl_parser_init(struct pfcctl_parser_s *parser)
{
  parser->used = 0u;
  parser->expected = 0u;
}

static void pfcctl_parser_restart(struct pfcctl_parser_s *parser,
                                  uint8_t byte)
{
  parser->used = byte == 0xa5u ? 1u : 0u;
  if (parser->used != 0u)
    {
      parser->buffer[0] = byte;
    }

  parser->expected = 0u;
}

bool pfcctl_parser_feed(struct pfcctl_parser_s *parser, uint8_t byte,
                        pfcctl_packet_cb_t callback, void *context)
{
  struct pfcctl_packet_s packet;
  uint16_t payload_length;
  uint16_t received_crc;
  bool delivered = false;

  if (parser == NULL)
    {
      return false;
    }

  if (parser->used == 0u)
    {
      pfcctl_parser_restart(parser, byte);
      return false;
    }

  if (parser->used == 1u && byte != 0x5au)
    {
      pfcctl_parser_restart(parser, byte);
      return false;
    }

  if (parser->used >= sizeof(parser->buffer))
    {
      pfcctl_parser_restart(parser, byte);
      return false;
    }

  parser->buffer[parser->used++] = byte;
  if (parser->used == 7u)
    {
      payload_length = pfcctl_read_le16(&parser->buffer[5]);
      if (parser->buffer[2] != PFCCTL_VERSION ||
          payload_length > PFCCTL_MAX_PAYLOAD)
        {
          pfcctl_parser_restart(parser, byte);
          return false;
        }

      parser->expected = (uint16_t)(7u + payload_length + 2u);
    }

  if (parser->expected == 0u || parser->used != parser->expected)
    {
      return false;
    }

  received_crc = pfcctl_read_le16(
      &parser->buffer[parser->expected - 2u]);
  if (received_crc ==
      pfcctl_crc16(&parser->buffer[2],
                   (uint16_t)(parser->expected - 4u)))
    {
      packet.command = parser->buffer[3];
      packet.sequence = parser->buffer[4];
      packet.length = pfcctl_read_le16(&parser->buffer[5]);
      memcpy(packet.payload, &parser->buffer[7], packet.length);
      delivered = callback != NULL && callback(context, &packet);
    }

  pfcctl_parser_init(parser);
  return delivered;
}

size_t pfcctl_encode(const struct pfcctl_packet_s *packet, uint8_t *output,
                     size_t capacity)
{
  size_t total;
  uint16_t crc;

  if (packet == NULL || output == NULL ||
      packet->length > PFCCTL_MAX_PAYLOAD)
    {
      return 0u;
    }

  total = 7u + packet->length + 2u;
  if (capacity < total)
    {
      return 0u;
    }

  output[0] = 0xa5u;
  output[1] = 0x5au;
  output[2] = PFCCTL_VERSION;
  output[3] = packet->command;
  output[4] = packet->sequence;
  pfcctl_write_le16(&output[5], packet->length);
  memcpy(&output[7], packet->payload, packet->length);
  crc = pfcctl_crc16(&output[2], (uint16_t)(total - 4u));
  pfcctl_write_le16(&output[total - 2u], crc);
  return total;
}

bool pfcctl_decode_status(const struct pfcctl_packet_s *packet,
                          struct pfcctl_status_s *status)
{
  if (packet == NULL || status == NULL ||
      packet->command != PFCCTL_EVT_STATUS || packet->length != 8u)
    {
      return false;
    }

  status->pfc_state = packet->payload[0];
  status->pfc_fault = packet->payload[1];
  status->llc_state = packet->payload[2];
  status->llc_fault = packet->payload[3];
  status->voltage_cv = pfcctl_read_le16(&packet->payload[4]);
  status->current_ca = pfcctl_read_le16(&packet->payload[6]);
  return true;
}
