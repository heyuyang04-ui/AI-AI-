/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "pfcctl_protocol.h"

#ifndef CONFIG_SYSTEM_PFCCTL_DEVICE
#  define CONFIG_SYSTEM_PFCCTL_DEVICE "/dev/ttyS0"
#endif

#ifndef CONFIG_SYSTEM_PFCCTL_TIMEOUT_MS
#  define CONFIG_SYSTEM_PFCCTL_TIMEOUT_MS 1200
#endif

enum pfcctl_exit_e
{
  PFCCTL_EXIT_OK = 0,
  PFCCTL_EXIT_USAGE = 2,
  PFCCTL_EXIT_IO = 3,
  PFCCTL_EXIT_TIMEOUT = 4,
  PFCCTL_EXIT_REJECTED = 5,
  PFCCTL_EXIT_UNSUPPORTED = 6
};

struct pfcctl_rx_s
{
  struct pfcctl_status_s status;
  uint8_t sequence;
  uint8_t request_command;
  uint8_t result;
  bool got_status;
  bool got_result;
  bool rejected;
};

static uint8_t g_sequence = 1u;

static uint64_t pfcctl_now_ms(void)
{
  struct timespec value;

  if (clock_gettime(CLOCK_MONOTONIC, &value) < 0)
    {
      return 0u;
    }

  return (uint64_t)value.tv_sec * 1000u +
         (uint64_t)value.tv_nsec / 1000000u;
}

static uint8_t pfcctl_next_sequence(void)
{
  uint8_t value = g_sequence++;

  if (g_sequence == 0u)
    {
      g_sequence = 1u;
    }

  return value;
}

static const char *pfcctl_reject_name(uint8_t reason)
{
  switch (reason)
    {
      case 1u:
        return "BAD_PAYLOAD";
      case 2u:
        return "OUT_OF_RANGE";
      case 3u:
        return "CAN_SEND_FAILED";
      default:
        return "UNKNOWN";
    }
}

static bool pfcctl_reason_valid(const char *reason)
{
  size_t index;
  size_t length;

  if (reason == NULL)
    {
      return false;
    }

  length = strlen(reason);
  if (length == 0u || length > 48u)
    {
      return false;
    }

  for (index = 0u; index < length; index++)
    {
      unsigned char value = (unsigned char)reason[index];
      if (!(isupper(value) || isdigit(value) || value == '_' || value == '-'))
        {
          return false;
        }
    }

  return true;
}

static int pfcctl_open_uart(const char *device)
{
  struct termios options;
  int fd;

  fd = open(device, O_RDWR | O_NOCTTY);
  if (fd < 0)
    {
      return -1;
    }

  if (tcgetattr(fd, &options) < 0)
    {
      close(fd);
      return -1;
    }

  options.c_iflag = 0;
  options.c_oflag = 0;
  options.c_lflag = 0;
  options.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
  options.c_cflag |= CS8 | CLOCAL | CREAD;
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0;
  (void)cfsetispeed(&options, B115200);
  (void)cfsetospeed(&options, B115200);

  if (tcsetattr(fd, TCSANOW, &options) < 0)
    {
      close(fd);
      return -1;
    }

  (void)tcflush(fd, TCIFLUSH);
  return fd;
}

static int pfcctl_write_all(int fd, const uint8_t *data, size_t length)
{
  size_t written = 0u;

  while (written < length)
    {
      ssize_t result = write(fd, &data[written], length - written);
      if (result < 0 && errno == EINTR)
        {
          continue;
        }

      if (result <= 0)
        {
          return -1;
        }

      written += (size_t)result;
    }

  return 0;
}

static int pfcctl_send(int fd, uint8_t command, uint8_t sequence,
                       const uint8_t *payload, uint16_t length)
{
  struct pfcctl_packet_s packet;
  uint8_t wire[PFCCTL_MAX_FRAME];
  size_t wire_length;

  if (length > PFCCTL_MAX_PAYLOAD || (length > 0u && payload == NULL))
    {
      return -1;
    }

  memset(&packet, 0, sizeof(packet));
  packet.command = command;
  packet.sequence = sequence;
  packet.length = length;
  if (length > 0u)
    {
      memcpy(packet.payload, payload, length);
    }

  wire_length = pfcctl_encode(&packet, wire, sizeof(wire));
  return wire_length == 0u ? -1 : pfcctl_write_all(fd, wire, wire_length);
}

static bool pfcctl_receive_packet(void *context,
                                  const struct pfcctl_packet_s *packet)
{
  struct pfcctl_rx_s *rx = context;

  if (packet->command == PFCCTL_EVT_STATUS &&
      (packet->sequence == rx->sequence || packet->sequence == 0u) &&
      pfcctl_decode_status(packet, &rx->status))
    {
      rx->got_status = true;
      return true;
    }

  if ((packet->command == PFCCTL_EVT_ACK ||
       packet->command == PFCCTL_EVT_REJECT) &&
      packet->sequence == rx->sequence && packet->length == 2u)
    {
      rx->request_command = packet->payload[0];
      rx->result = packet->payload[1];
      rx->rejected = packet->command == PFCCTL_EVT_REJECT;
      rx->got_result = true;
      return true;
    }

  return false;
}

static int pfcctl_wait(int fd, struct pfcctl_rx_s *rx, bool want_status)
{
  struct pfcctl_parser_s parser;
  uint8_t input[32];
  uint64_t deadline = pfcctl_now_ms() + CONFIG_SYSTEM_PFCCTL_TIMEOUT_MS;

  pfcctl_parser_init(&parser);
  while (pfcctl_now_ms() < deadline)
    {
      struct pollfd descriptor;
      uint64_t now = pfcctl_now_ms();
      int remaining;
      int ready;
      ssize_t length;
      ssize_t index;

      if (now >= deadline)
        {
          break;
        }

      remaining = (int)(deadline - now);
      descriptor.fd = fd;
      descriptor.events = POLLIN;
      descriptor.revents = 0;
      ready = poll(&descriptor, 1, remaining);
      if (ready < 0 && errno == EINTR)
        {
          continue;
        }

      if (ready <= 0)
        {
          break;
        }

      length = read(fd, input, sizeof(input));
      if (length < 0 && errno == EINTR)
        {
          continue;
        }

      if (length <= 0)
        {
          continue;
        }

      for (index = 0; index < length; index++)
        {
          (void)pfcctl_parser_feed(&parser, input[index],
                                   pfcctl_receive_packet, rx);
          if ((want_status && rx->got_status) ||
              (!want_status && rx->got_result))
            {
              return 0;
            }
        }
    }

  return -1;
}

static int pfcctl_request_status(int fd, struct pfcctl_status_s *status)
{
  struct pfcctl_rx_s rx;

  memset(&rx, 0, sizeof(rx));
  rx.sequence = pfcctl_next_sequence();
  if (pfcctl_send(fd, PFCCTL_CMD_TELEMETRY_REQUEST, rx.sequence,
                  NULL, 0u) < 0 ||
      pfcctl_wait(fd, &rx, true) < 0)
    {
      return -1;
    }

  *status = rx.status;
  return 0;
}

static void pfcctl_print_status(const struct pfcctl_status_s *status,
                                const char *command_status,
                                const char *reject_reason,
                                const char *requested_reason)
{
  printf("{\"valid\":true,\"age_ms\":0,");
  printf("\"telemetry_complete\":false,");
  printf("\"pfc\":{\"state\":%u,\"fault\":%u,",
         status->pfc_state, status->pfc_fault);
  printf("\"ac_v\":null,\"bus_v\":null},");
  printf("\"llc\":{\"state\":%u,\"fault\":%u,",
         status->llc_state, status->llc_fault);
  printf("\"output_enabled\":null,\"output_v\":%.2f,",
         (double)status->voltage_cv / 100.0);
  printf("\"output_a\":%.2f,\"mos_temp_c\":null,",
         (double)status->current_ca / 100.0);
  printf("\"diode_temp_c\":null},");
  printf("\"gateway\":{\"command_status\":\"%s\",",
         command_status);
  printf("\"reject_reason\":\"%s\"},", reject_reason);
  if (requested_reason != NULL)
    {
      printf("\"requested_reason\":\"%s\",", requested_reason);
    }

  printf("\"limitations\":[\"UART_V1_NO_AC_OR_BUS_VOLTAGE\","
         "\"UART_V1_NO_TEMPERATURE\","
         "\"UART_V1_NO_OUTPUT_ENABLE_FEEDBACK\"]}\n");
}

static void pfcctl_print_invalid(const char *status, const char *reason)
{
  printf("{\"valid\":false,\"gateway\":{"
         "\"command_status\":\"%s\","
         "\"reject_reason\":\"%s\"}}\n", status, reason);
}

static int pfcctl_status_command(const char *device)
{
  struct pfcctl_status_s status;
  int fd = pfcctl_open_uart(device);

  if (fd < 0)
    {
      pfcctl_print_invalid("io_error", "UART_OPEN_FAILED");
      return PFCCTL_EXIT_IO;
    }

  if (pfcctl_request_status(fd, &status) < 0)
    {
      close(fd);
      pfcctl_print_invalid("timeout", "STM32_STATUS_TIMEOUT");
      return PFCCTL_EXIT_TIMEOUT;
    }

  close(fd);
  pfcctl_print_status(&status, "idle", "none", NULL);
  return PFCCTL_EXIT_OK;
}

static int pfcctl_stop_command(const char *device, const char *reason)
{
  struct pfcctl_status_s status;
  struct pfcctl_rx_s rx;
  uint8_t payload[5] = {0u, 0u, 0u, 0u, 0u};
  int fd = pfcctl_open_uart(device);
  int status_result;

  memset(&status, 0, sizeof(status));
  if (fd < 0)
    {
      pfcctl_print_invalid("io_error", "UART_OPEN_FAILED");
      return PFCCTL_EXIT_IO;
    }

  memset(&rx, 0, sizeof(rx));
  rx.sequence = pfcctl_next_sequence();
  if (pfcctl_send(fd, PFCCTL_CMD_SET, rx.sequence, payload,
                  sizeof(payload)) < 0 ||
      pfcctl_wait(fd, &rx, false) < 0)
    {
      close(fd);
      pfcctl_print_invalid("timeout", "STM32_STOP_ACK_TIMEOUT");
      return PFCCTL_EXIT_TIMEOUT;
    }

  if (rx.request_command != PFCCTL_CMD_SET)
    {
      close(fd);
      pfcctl_print_invalid("invalid_reply", "ACK_COMMAND_MISMATCH");
      return PFCCTL_EXIT_IO;
    }

  status_result = pfcctl_request_status(fd, &status);
  close(fd);
  if (rx.rejected || rx.result != 0u)
    {
      if (status_result == 0)
        {
          pfcctl_print_status(&status, "rejected",
                              pfcctl_reject_name(rx.result), reason);
        }
      else
        {
          pfcctl_print_invalid("rejected", pfcctl_reject_name(rx.result));
        }

      return PFCCTL_EXIT_REJECTED;
    }

  if (status_result < 0)
    {
      pfcctl_print_invalid("accepted_unverified", "STATUS_TIMEOUT_AFTER_ACK");
      return PFCCTL_EXIT_TIMEOUT;
    }

  pfcctl_print_status(&status, "accepted", "none", reason);
  return PFCCTL_EXIT_OK;
}

static int pfcctl_derate_command(const char *reason)
{
  (void)reason;
  pfcctl_print_invalid("rejected",
      "UART_V1_DERATE_UNSAFE_WITHOUT_REFERENCE_AND_HEARTBEAT_DAEMON");
  return PFCCTL_EXIT_UNSUPPORTED;
}

static void pfcctl_usage(const char *program)
{
  fprintf(stderr,
          "usage: %s [--device PATH] status --json\n"
          "       %s [--device PATH] safe-stop --reason REASON\n"
          "       %s [--device PATH] request-derate --percent 80 "
          "--reason REASON\n",
          program, program, program);
}

int main(int argc, char *argv[])
{
  const char *device = CONFIG_SYSTEM_PFCCTL_DEVICE;
  const char *command;
  const char *reason = NULL;
  int index = 1;
  int percent = -1;

  if (index + 1 < argc && strcmp(argv[index], "--device") == 0)
    {
      device = argv[index + 1];
      index += 2;
    }

  if (index >= argc)
    {
      pfcctl_usage(argv[0]);
      return PFCCTL_EXIT_USAGE;
    }

  command = argv[index++];
  while (index < argc)
    {
      if (index + 1 < argc && strcmp(argv[index], "--reason") == 0)
        {
          reason = argv[index + 1];
          index += 2;
        }
      else if (index + 1 < argc && strcmp(argv[index], "--percent") == 0)
        {
          percent = atoi(argv[index + 1]);
          index += 2;
        }
      else if (strcmp(argv[index], "--json") == 0)
        {
          index++;
        }
      else
        {
          pfcctl_usage(argv[0]);
          return PFCCTL_EXIT_USAGE;
        }
    }

  if (strcmp(command, "status") == 0 && reason == NULL && percent < 0)
    {
      return pfcctl_status_command(device);
    }

  if (strcmp(command, "safe-stop") == 0 &&
      pfcctl_reason_valid(reason) && percent < 0)
    {
      return pfcctl_stop_command(device, reason);
    }

  if (strcmp(command, "request-derate") == 0 &&
      pfcctl_reason_valid(reason) && percent == 80)
    {
      return pfcctl_derate_command(reason);
    }

  pfcctl_usage(argv[0]);
  return PFCCTL_EXIT_USAGE;
}
