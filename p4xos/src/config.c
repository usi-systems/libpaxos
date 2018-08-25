/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_string_fns.h>
#include <rte_tcp.h>

#include "main.h"

struct app_params app;

static const char usage[] =
    "    load_balancer <EAL PARAMS> -- <APP PARAMS>                          \n"
    "                                                                        \n"
    "Application manadatory parameters:                                      \n"
    "    --rx \"(PORT, QUEUE, LCORE), ...\" : List of NIC RX ports and queues\n"
    "           handled by the I/O RX lcores                                 \n"
    "    --tx \"(PORT, LCORE), ...\" : List of NIC TX ports handled lcores   \n"
    "    --w \"LCORE, ...\" : List of the worker lcores                      \n"
    "    --lpm \"IP / PREFIX => PORT; ...\" : List of LPM rules used         \n"
    "          by the worker lcores for packet forwarding                  \n\n"
    "Application optional parameters:                                        \n"
    "    --rsz \"A, B, C, D\" : Ring sizes                                   \n"
    "           A = Size (in number of buffer descriptors) of each of the NIC\n"
    "               RX rings read by the I/O RX lcores (default value is %u) \n"
    "           B = Size (in number of elements) of each of the SW rings     \n"
    "               used by the I/O RX lcores to send packets to worker      \n"
    "               lcores (default value is %u)                             \n"
    "           C = Size (in number of elements) of each of the SW rings     \n"
    "               used by the worker lcores to send packets to I/O TX      \n"
    "               lcores (default value is\n %u)                           \n"
    "           D = Size (in number of buffer descriptors) of each           \n"
    "               rings written by I/O TX lcores (default value is %u)     \n"
    "    --bsz \"(A, B), (C, D), (E, F)\" :  Burst sizes                     \n"
    "           A = I/O RX lcore read burst size from NIC RX                 \n"
    "               (default value is %u)                                    \n"
    "           B = I/O RX lcore write burst size to output SW rings         \n"
    "              (default value is %u)                                     \n"
    "           C = Worker lcore read burst size from input SW rings         \n"
    "               (default value is %u)                                    \n"
    "           D = Worker lcore write burst size to output SW rings         \n"
    "               (default value \n is %u)                                 \n"
    "           E = I/O TX lcore read burst size from input SW rings         \n"
    "               (default value is %u)                                    \n"
    "           F = I/O TX lcore write burst size to NIC TX                  \n"
    "               (default value is %u)                                    \n"
    "    --pos-lb POS : Position of the 1-byte field within the input packet \n"
    "               used bythe I/O RX lcores to identify the worker lcore for\n"
    "               the current packet (default value is %u)                 \n"
    "    --msgtype MSGTYPE : Type of p4xos packets (default value is %u)     \n"
    "    --baseline : Enable baseline mode (Do nothing) (default value is %u)\n"
    "    --multi-dbs : Enable multiple instance of DBs (default value is %u) \n"
    "    --reset-inst: Reset leader instance (default value is %u)           \n"
    "    --inc-inst : Proposer increases instance after receiving a response \n"
    "				(default value is %u)                                    \n"
    "    --run_prepare : Run prepare phase in learner recovery               \n"
    "				(default value is %u)                                    \n"
    "    --drop : Artificial drop packets (default value is %u)              \n"
    "    --leader : set node as Paxos leader (default value is %u)           \n"
    "    --resp : Send response to client (default value is %u)              \n"
    "    --latency : Enable measure latency (default value is %u)            \n"
    "    --port [PORT]: TX port Proposers use initially (default value is %u)\n"
    "    --num-ac NUM: Number of acceptors (default value is %u)             \n"
    "    --node-id NUM: Set Node Identifier (default value is %u)            \n"
    "    --cp-interval NUM : The interval between checkpoints                \n"
    "               (default value is %u)                                    \n"
    "    --ts-interval NUM : The interval between get time                   \n"
    "               (default value is %u)                                    \n"
    "    --osd NUM : The number of packets will be sent at beginning         \n"
    "               (default value is %u)                                    \n"
    "    --max NUM : Stop learner after delivered this instance              \n"
    "               (default value is %u)                                    \n"
    "    --rate Mbps : Set client sending rate (default value is %u)         \n"
    "    --window NUM : Pre-execute Prepare instances (default value is %u)  \n"
    "    --cliaddr \"IP\" : Client IP address                                \n"
    "               (default value is %s)			                         \n"
    "    --src \"IP\" : source IP address proposers use to generate packets  \n"
    "               (default value is %s)			                         \n"
    "    --acc-addr \"IP\" : acceptor multicast IP address                   \n"
    "                (default value is %s)                                   \n"
    "    --learner-addr \"IP\" : learner multicast IP address                \n"
    "                (default value is %s)                                   \n"
    "    --pri \"IP\" : leader IP address                                    \n"
    "                (default value is %s)                                   \n"
    "    --dst \"IP\" : destination IP address proposers use to generate     \n"
    "                packets (default value is %s)                           \n";

void app_print_usage(void) {
  printf(
      usage, APP_DEFAULT_NIC_RX_RING_SIZE, APP_DEFAULT_RING_RX_SIZE,
      APP_DEFAULT_RING_TX_SIZE, APP_DEFAULT_NIC_TX_RING_SIZE,
      APP_DEFAULT_BURST_SIZE_IO_RX_READ, APP_DEFAULT_BURST_SIZE_IO_RX_WRITE,
      APP_DEFAULT_BURST_SIZE_WORKER_READ, APP_DEFAULT_BURST_SIZE_WORKER_WRITE,
      APP_DEFAULT_BURST_SIZE_IO_TX_READ, APP_DEFAULT_BURST_SIZE_IO_TX_WRITE,
      APP_DEFAULT_IO_RX_LB_POS, APP_DEFAULT_MESSAGE_TYPE, APP_DEFAULT_FALSE,
      APP_DEFAULT_FALSE, APP_DEFAULT_FALSE, APP_DEFAULT_FALSE,
      APP_DEFAULT_FALSE, APP_DEFAULT_FALSE, APP_DEFAULT_FALSE,
      APP_DEFAULT_FALSE, APP_DEFAULT_FALSE,
      APP_DEFAULT_TX_PORT, APP_DEFAULT_NUM_ACCEPTORS, APP_DEFAULT_NODE_ID,
      APP_DEFAULT_CHECKPOINT_INTERVAL, APP_DEFAULT_TS_INTERVAL,
      APP_DEFAULT_OUTSTANDING, APP_DEFAULT_MAX_INST, APP_DEFAULT_SENDING_RATE,
      APP_PREEXEC_WINDOW,
      APP_DEFAULT_IP_SRC_ADDR, APP_DEFAULT_IP_SRC_ADDR, APP_DEFAULT_IP_ACCEPTOR_ADDR,
      APP_DEFAULT_IP_LEARNER_ADDR, APP_DEFAULT_IP_DST_ADDR, APP_DEFAULT_IP_BACKUP_DST_ADDR
    );
}

#ifndef APP_ARG_RX_MAX_CHARS
#define APP_ARG_RX_MAX_CHARS 4096
#endif

#ifndef APP_ARG_RX_MAX_TUPLES
#define APP_ARG_RX_MAX_TUPLES 128
#endif

static int str_to_unsigned_array(const char *s, size_t sbuflen, char separator,
                                 unsigned num_vals, unsigned *vals) {
  char str[sbuflen + 1];
  char *splits[num_vals];
  char *endptr = NULL;
  int i, num_splits = 0;

  /* copy s so we don't modify original string */
  snprintf(str, sizeof(str), "%s", s);
  num_splits = rte_strsplit(str, sizeof(str), splits, num_vals, separator);

  errno = 0;
  for (i = 0; i < num_splits; i++) {
    vals[i] = strtoul(splits[i], &endptr, 0);
    if (errno != 0 || *endptr != '\0')
      return -1;
  }

  return num_splits;
}

static int str_to_unsigned_vals(const char *s, size_t sbuflen, char separator,
                                unsigned num_vals, ...) {
  unsigned i, vals[num_vals];
  va_list ap;

  num_vals = str_to_unsigned_array(s, sbuflen, separator, num_vals, vals);

  va_start(ap, num_vals);
  for (i = 0; i < num_vals; i++) {
    unsigned *u = va_arg(ap, unsigned *);
    *u = vals[i];
  }
  va_end(ap);
  return num_vals;
}

static int parse_arg_rx(const char *arg) {
  const char *p0 = arg, *p = arg;
  uint32_t n_tuples;

  if (strnlen(arg, APP_ARG_RX_MAX_CHARS + 1) == APP_ARG_RX_MAX_CHARS + 1) {
    return -1;
  }

  n_tuples = 0;
  while ((p = strchr(p0, '(')) != NULL) {
    struct app_lcore_params *lp;
    uint32_t port, queue, lcore, i;

    p0 = strchr(p++, ')');
    if ((p0 == NULL) ||
        (str_to_unsigned_vals(p, p0 - p, ',', 3, &port, &queue, &lcore) != 3)) {
      return -2;
    }

    /* Enable port and queue for later initialization */
    if ((port >= APP_MAX_NIC_PORTS) ||
        (queue >= APP_MAX_RX_QUEUES_PER_NIC_PORT)) {
      return -3;
    }
    if (app.nic_rx_queue_mask[port][queue] != 0) {
      return -4;
    }
    app.nic_rx_queue_mask[port][queue] = 1;

    /* Check and assign (port, queue) to I/O lcore */
    if (rte_lcore_is_enabled(lcore) == 0) {
      return -5;
    }

    if (lcore >= APP_MAX_LCORES) {
      return -6;
    }
    lp = &app.lcore_params[lcore];
    if (lp->type == e_APP_LCORE_WORKER) {
      return -7;
    }
    lp->type = e_APP_LCORE_IO;
    const size_t n_queues =
        RTE_MIN(lp->io.rx.n_nic_queues, RTE_DIM(lp->io.rx.nic_queues));
    for (i = 0; i < n_queues; i++) {
      if ((lp->io.rx.nic_queues[i].port == port) &&
          (lp->io.rx.nic_queues[i].queue == queue)) {
        return -8;
      }
    }
    if (lp->io.rx.n_nic_queues >= APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE) {
      return -9;
    }
    lp->io.rx.nic_queues[lp->io.rx.n_nic_queues].port = port;
    lp->io.rx.nic_queues[lp->io.rx.n_nic_queues].queue = (uint8_t)queue;
    lp->io.rx.n_nic_queues++;

    n_tuples++;
    if (n_tuples > APP_ARG_RX_MAX_TUPLES) {
      return -10;
    }
  }

  if (n_tuples == 0) {
    return -11;
  }

  return 0;
}

#ifndef APP_ARG_TX_MAX_CHARS
#define APP_ARG_TX_MAX_CHARS 4096
#endif

#ifndef APP_ARG_TX_MAX_TUPLES
#define APP_ARG_TX_MAX_TUPLES 128
#endif

static int parse_arg_tx(const char *arg) {
  const char *p0 = arg, *p = arg;
  uint32_t n_tuples;

  if (strnlen(arg, APP_ARG_TX_MAX_CHARS + 1) == APP_ARG_TX_MAX_CHARS + 1) {
    return -1;
  }

  n_tuples = 0;
  while ((p = strchr(p0, '(')) != NULL) {
    struct app_lcore_params *lp;
    uint32_t port, lcore, i;

    p0 = strchr(p++, ')');
    if ((p0 == NULL) ||
        (str_to_unsigned_vals(p, p0 - p, ',', 2, &port, &lcore) != 2)) {
      return -2;
    }

    /* Enable port and queue for later initialization */
    if (port >= APP_MAX_NIC_PORTS) {
      return -3;
    }
    if (app.nic_tx_port_mask[port] != 0) {
      return -4;
    }
    app.nic_tx_port_mask[port] = 1;

    /* Check and assign (port, queue) to I/O lcore */
    if (rte_lcore_is_enabled(lcore) == 0) {
      return -5;
    }

    if (lcore >= APP_MAX_LCORES) {
      return -6;
    }
    lp = &app.lcore_params[lcore];
    if (lp->type == e_APP_LCORE_WORKER) {
      return -7;
    }
    lp->type = e_APP_LCORE_IO;
    const size_t n_ports =
        RTE_MIN(lp->io.tx.n_nic_ports, RTE_DIM(lp->io.tx.nic_ports));
    for (i = 0; i < n_ports; i++) {
      if (lp->io.tx.nic_ports[i] == port) {
        return -8;
      }
    }
    if (lp->io.tx.n_nic_ports >= APP_MAX_NIC_TX_PORTS_PER_IO_LCORE) {
      return -9;
    }
    lp->io.tx.nic_ports[lp->io.tx.n_nic_ports] = port;
    lp->io.tx.n_nic_ports++;

    n_tuples++;
    if (n_tuples > APP_ARG_TX_MAX_TUPLES) {
      return -10;
    }
  }

  if (n_tuples == 0) {
    return -11;
  }

  return 0;
}

#ifndef APP_ARG_W_MAX_CHARS
#define APP_ARG_W_MAX_CHARS 4096
#endif

#ifndef APP_ARG_W_MAX_TUPLES
#define APP_ARG_W_MAX_TUPLES APP_MAX_WORKER_LCORES
#endif

static int parse_arg_w(const char *arg) {
  const char *p = arg;
  uint32_t n_tuples;

  if (strnlen(arg, APP_ARG_W_MAX_CHARS + 1) == APP_ARG_W_MAX_CHARS + 1) {
    return -1;
  }

  n_tuples = 0;
  while (*p != 0) {
    struct app_lcore_params *lp;
    uint32_t lcore;

    errno = 0;
    lcore = strtoul(p, NULL, 0);
    if (errno != 0) {
      return -2;
    }

    /* Check and enable worker lcore */
    if (rte_lcore_is_enabled(lcore) == 0) {
      return -3;
    }

    if (lcore >= APP_MAX_LCORES) {
      return -4;
    }
    lp = &app.lcore_params[lcore];
    if (lp->type == e_APP_LCORE_IO) {
      return -5;
    }
    lp->type = e_APP_LCORE_WORKER;

    n_tuples++;
    if (n_tuples > APP_ARG_W_MAX_TUPLES) {
      return -6;
    }

    p = strchr(p, ',');
    if (p == NULL) {
      break;
    }
    p++;
  }

  if (n_tuples == 0) {
    return -7;
  }

  // if ((n_tuples & (n_tuples - 1)) != 0) {
  //   return -8;
  // }

  return 0;
}

#ifndef APP_ARG_LPM_MAX_CHARS
#define APP_ARG_LPM_MAX_CHARS 4096
#endif

static int parse_arg_lpm(const char *arg) {
  const char *p = arg, *p0;

  if (strnlen(arg, APP_ARG_LPM_MAX_CHARS + 1) == APP_ARG_TX_MAX_CHARS + 1) {
    return -1;
  }

  while (*p != 0) {
    uint32_t ip_a, ip_b, ip_c, ip_d, ip, depth, if_out;
    char *endptr;

    p0 = strchr(p, '/');
    if ((p0 == NULL) || (str_to_unsigned_vals(p, p0 - p, '.', 4, &ip_a, &ip_b,
                                              &ip_c, &ip_d) != 4)) {
      return -2;
    }

    p = p0 + 1;
    errno = 0;
    depth = strtoul(p, &endptr, 0);
    if (errno != 0 || *endptr != '=') {
      return -3;
    }
    p = strchr(p, '>');
    if (p == NULL) {
      return -4;
    }
    if_out = strtoul(++p, &endptr, 0);
    if (errno != 0 || (*endptr != '\0' && *endptr != ';')) {
      return -5;
    }

    if ((ip_a >= 256) || (ip_b >= 256) || (ip_c >= 256) || (ip_d >= 256) ||
        (depth == 0) || (depth > 32) || (if_out >= APP_MAX_NIC_PORTS)) {
      return -6;
    }
    ip = (ip_a << 24) | (ip_b << 16) | (ip_c << 8) | ip_d;

    if (app.n_lpm_rules >= APP_MAX_LPM_RULES) {
      return -7;
    }
    app.lpm_rules[app.n_lpm_rules].ip = ip;
    app.lpm_rules[app.n_lpm_rules].depth = (uint8_t)depth;
    app.lpm_rules[app.n_lpm_rules].if_out = (uint8_t)if_out;
    app.n_lpm_rules++;

    p = strchr(p, ';');
    if (p == NULL) {
      return -8;
    }
    p++;
  }

  if (app.n_lpm_rules == 0) {
    return -9;
  }

  return 0;
}

static int __rte_unused app_check_lpm_table(void) {
  uint32_t rule;

  /* For each rule, check that the output I/F is enabled */
  for (rule = 0; rule < app.n_lpm_rules; rule++) {
    uint32_t port = app.lpm_rules[rule].if_out;

    if (app.nic_tx_port_mask[port] == 0) {
      return -1;
    }
  }

  return 0;
}

static int app_check_every_rx_port_is_tx_enabled(void) {
  uint16_t port;

  for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
    if ((app_get_nic_rx_queues_per_port(port) > 0) &&
        (app.nic_tx_port_mask[port] == 0)) {
      return -1;
    }
  }

  return 0;
}

#ifndef APP_ARG_RSZ_CHARS
#define APP_ARG_RSZ_CHARS 63
#endif

static int parse_arg_rsz(const char *arg) {
  if (strnlen(arg, APP_ARG_RSZ_CHARS + 1) == APP_ARG_RSZ_CHARS + 1) {
    return -1;
  }

  if (str_to_unsigned_vals(arg, APP_ARG_RSZ_CHARS, ',', 4,
                           &app.nic_rx_ring_size, &app.ring_rx_size,
                           &app.ring_tx_size, &app.nic_tx_ring_size) != 4)
    return -2;

  if ((app.nic_rx_ring_size == 0) || (app.nic_tx_ring_size == 0) ||
      (app.ring_rx_size == 0) || (app.ring_tx_size == 0)) {
    return -3;
  }

  return 0;
}

#ifndef APP_ARG_BSZ_CHARS
#define APP_ARG_BSZ_CHARS 63
#endif

static int parse_arg_bsz(const char *arg) {
  const char *p = arg, *p0;
  if (strnlen(arg, APP_ARG_BSZ_CHARS + 1) == APP_ARG_BSZ_CHARS + 1) {
    return -1;
  }

  p0 = strchr(p++, ')');
  if ((p0 == NULL) ||
      (str_to_unsigned_vals(p, p0 - p, ',', 2, &app.burst_size_io_rx_read,
                            &app.burst_size_io_rx_write) != 2)) {
    return -2;
  }

  p = strchr(p0, '(');
  if (p == NULL) {
    return -3;
  }

  p0 = strchr(p++, ')');
  if ((p0 == NULL) ||
      (str_to_unsigned_vals(p, p0 - p, ',', 2, &app.burst_size_worker_read,
                            &app.burst_size_worker_write) != 2)) {
    return -4;
  }

  p = strchr(p0, '(');
  if (p == NULL) {
    return -5;
  }

  p0 = strchr(p++, ')');
  if ((p0 == NULL) ||
      (str_to_unsigned_vals(p, p0 - p, ',', 2, &app.burst_size_io_tx_read,
                            &app.burst_size_io_tx_write) != 2)) {
    return -6;
  }

  if ((app.burst_size_io_rx_read == 0) || (app.burst_size_io_rx_write == 0) ||
      (app.burst_size_worker_read == 0) || (app.burst_size_worker_write == 0) ||
      (app.burst_size_io_tx_read == 0) || (app.burst_size_io_tx_write == 0)) {
    return -7;
  }

  if ((app.burst_size_io_rx_read > APP_MBUF_ARRAY_SIZE) ||
      (app.burst_size_io_rx_write > APP_MBUF_ARRAY_SIZE) ||
      (app.burst_size_worker_read > APP_MBUF_ARRAY_SIZE) ||
      (app.burst_size_worker_write > APP_MBUF_ARRAY_SIZE) ||
      ((2 * app.burst_size_io_tx_read) > APP_MBUF_ARRAY_SIZE) ||
      (app.burst_size_io_tx_write > APP_MBUF_ARRAY_SIZE)) {
    return -8;
  }

  return 0;
}

#ifndef APP_ARG_NUMERICAL_SIZE_CHARS
#define APP_ARG_NUMERICAL_SIZE_CHARS 15
#endif

static int parse_arg_uint8(const char *arg, uint8_t *out_number) {
  uint32_t x;
  char *endpt;

  if (strnlen(arg, APP_ARG_NUMERICAL_SIZE_CHARS + 1) ==
      APP_ARG_NUMERICAL_SIZE_CHARS + 1) {
    return -1;
  }

  errno = 0;
  x = strtoul(arg, &endpt, 10);
  if (errno != 0 || endpt == arg || *endpt != '\0') {
    return -2;
  }

  *out_number = (uint8_t)x;
  return 0;
}

static int parse_arg_uint16(const char *arg, uint16_t *out_number) {
  uint32_t x;
  char *endpt;

  if (strnlen(arg, APP_ARG_NUMERICAL_SIZE_CHARS + 1) ==
      APP_ARG_NUMERICAL_SIZE_CHARS + 1) {
    return -1;
  }

  errno = 0;
  x = strtoul(arg, &endpt, 10);
  if (errno != 0 || endpt == arg || *endpt != '\0') {
    return -2;
  }

  *out_number = (uint16_t)x;
  return 0;
}

static int parse_arg_uint32(const char *arg, uint32_t *out_number) {
  uint32_t x;
  char *endpt;

  if (strnlen(arg, APP_ARG_NUMERICAL_SIZE_CHARS + 1) ==
      APP_ARG_NUMERICAL_SIZE_CHARS + 1) {
    return -1;
  }

  errno = 0;
  x = strtoul(arg, &endpt, 10);
  if (errno != 0 || endpt == arg || *endpt != '\0') {
    return -2;
  }

  *out_number = (uint32_t)x;
  return 0;
}

static int
parse_arg_ip_address(const char *arg, struct sockaddr_in *addr)
{
    int ret;
    char* ip_and_port = strdup(arg);
    const char delim[2] = ":";
    char* token = strtok(ip_and_port, delim);
    addr->sin_family = AF_INET;
    if (token != NULL) {
        ret = inet_pton(AF_INET, token, &addr->sin_addr);
        if (ret == 0 || ret < 0) {
            return -1;
        }
    }
    token = strtok(NULL, delim);
    if (token != NULL) {
        uint32_t x;
        char* endpt;
        errno = 0;
        x = strtoul(token, &endpt, 10);
        if (errno != 0 || endpt == arg || *endpt != '\0') {
          return -2;
        }
        addr->sin_port = htons(x);
    }
    return 0;
}

/* Parse the argument given in the command line of the application */
int app_parse_args(int argc, char **argv) {
  int opt, ret;
  char **argvopt;
  int option_index;
  char *prgname = argv[0];
  static struct option lgopts[] = {
      {"rx", 1, 0, 0},          {"tx", 1, 0, 0},
      {"w", 1, 0, 0},           {"lpm", 1, 0, 0},
      {"rsz", 1, 0, 0},         {"bsz", 1, 0, 0},
      {"pos-lb", 1, 0, 0},      {"num-ac", 1, 0, 0},
      {"msgtype", 1, 0, 0},     {"node-id", 1, 0, 0},
      {"port", 1, 0, 0},        {"baseline", 0, 0, 0},
      {"multi-dbs", 0, 0, 0},   {"reset-inst", 0, 0, 0},
      {"inc-inst", 0, 0, 0},    {"run-prepare", 0, 0, 0},
      {"drop", 0, 0, 0},        {"osd", 1, 0, 0},
      {"resp", 0, 0, 0},        {"latency", 0, 0, 0},
      {"leader", 0, 0, 0},      {"window", 1, 0, 0},
      {"max", 1, 0, 0},         {"rate", 1, 0, 0},
      {"src", 1, 0, 0},         {"dst", 1, 0, 0},
      {"cliaddr", 1, 0, 0},     {"pri", 1, 0, 0},
      {"acc-addr", 1, 0, 0},    {"learner-addr", 1, 0, 0},
      {"cp-interval", 1, 0, 0}, {"ts-interval", 1, 0, 0},
      {NULL, 0, 0, 0}};
  uint32_t arg_w = 0;
  uint32_t arg_rx = 0;
  uint32_t arg_tx = 0;
  uint32_t arg_lpm = 0;
  uint32_t arg_rsz = 0;
  uint32_t arg_bsz = 0;
  uint32_t arg_pos_lb = 0;
  uint32_t arg_num_ac = 0;
  uint32_t src_addr = 0;
  uint32_t dst_addr = 0;
  uint32_t cli_addr = 0;
  uint32_t pri_addr = 0;
  uint32_t acc_addr = 0;
  uint32_t learner_addr = 0;
  uint32_t arg_max_inst = 0;
  uint32_t arg_rate = 0;
  uint32_t arg_window = 0;
  uint16_t tx_port = 0;
  uint16_t osd = 0;
  uint16_t node_id = 0;
  uint8_t msgtype = 0;
  uint8_t arg_baseline = 0;
  uint8_t arg_multi_dbs = 0;
  uint8_t arg_reset_inst = 0;
  uint8_t arg_inc_inst = 0;
  uint8_t arg_run_prepare = 0;
  uint8_t arg_drop = 0;
  uint8_t arg_resp = 0;
  uint8_t arg_latency = 0;
  uint8_t arg_leader = 0;
  uint8_t arg_checkpoint_interval = 0;
  uint8_t arg_ts_interval = 0;
  argvopt = argv;

  while ((opt = getopt_long(argc, argvopt, "", lgopts, &option_index)) != EOF) {

    switch (opt) {
    /* long options */
    case 0:
      if (!strcmp(lgopts[option_index].name, "rx")) {
        arg_rx = 1;
        ret = parse_arg_rx(optarg);
        if (ret) {
          printf("Incorrect value for --rx argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "tx")) {
        arg_tx = 1;
        ret = parse_arg_tx(optarg);
        if (ret) {
          printf("Incorrect value for --tx argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "w")) {
        arg_w = 1;
        ret = parse_arg_w(optarg);
        if (ret) {
          printf("Incorrect value for --w argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "lpm")) {
        arg_lpm = 1;
        ret = parse_arg_lpm(optarg);
        if (ret) {
          printf("Incorrect value for --lpm argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "rsz")) {
        arg_rsz = 1;
        ret = parse_arg_rsz(optarg);
        if (ret) {
          printf("Incorrect value for --rsz argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "bsz")) {
        arg_bsz = 1;
        ret = parse_arg_bsz(optarg);
        if (ret) {
          printf("Incorrect value for --bsz argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "pos-lb")) {
        arg_pos_lb = 1;
        ret = parse_arg_uint8(optarg, &app.pos_lb);
        if (ret) {
          printf("Incorrect value for --pos-lb argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "num-ac")) {
        arg_num_ac = 1;
        ret = parse_arg_uint8(optarg, &(app.p4xos_conf.num_acceptors));
        if (ret) {
          printf("Incorrect value for --num-ac argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "msgtype")) {
        msgtype = 1;
        ret = parse_arg_uint8(optarg, &(app.p4xos_conf.msgtype));
        if (ret) {
          printf("Incorrect value for --msgtype argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "node-id")) {
        node_id = 1;
        ret = parse_arg_uint16(optarg, &(app.p4xos_conf.node_id));
        if (ret) {
          printf("Incorrect value for --node-id argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "port")) {
        tx_port = 1;
        ret = parse_arg_uint16(optarg, &(app.p4xos_conf.tx_port));
        if (ret) {
          printf("Incorrect value for --port argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "baseline")) {
        arg_baseline = 1;
        app.p4xos_conf.baseline = 1;
      }
      if (!strcmp(lgopts[option_index].name, "multi-dbs")) {
        arg_multi_dbs = 1;
        app.p4xos_conf.multi_dbs = 1;
      }
      if (!strcmp(lgopts[option_index].name, "reset-inst")) {
        arg_reset_inst = 1;
        app.p4xos_conf.reset_inst = 1;
      }
      if (!strcmp(lgopts[option_index].name, "inc-inst")) {
        arg_inc_inst = 1;
        app.p4xos_conf.inc_inst = 1;
      }
      if (!strcmp(lgopts[option_index].name, "run-prepare")) {
        arg_run_prepare = 1;
        app.p4xos_conf.run_prepare = 1;
      }
      if (!strcmp(lgopts[option_index].name, "drop")) {
        arg_drop = 1;
        app.p4xos_conf.drop = 1;
      }
      if (!strcmp(lgopts[option_index].name, "resp")) {
        arg_resp = 1;
        app.p4xos_conf.respond_to_client = 1;
      }
      if (!strcmp(lgopts[option_index].name, "latency")) {
        arg_latency = 1;
        app.p4xos_conf.measure_latency = 1;
      }
      if (!strcmp(lgopts[option_index].name, "leader")) {
        arg_leader = 1;
        app.p4xos_conf.leader = 1;
      }
      if (!strcmp(lgopts[option_index].name, "osd")) {
        osd = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.osd));
        if (ret) {
          printf("Incorrect value for --osd argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "window")) {
        arg_window = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.preexec_window));
        if (ret) {
          printf("Incorrect value for --window argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "max")) {
        arg_max_inst = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.max_inst));
        if (ret) {
          printf("Incorrect value for --max argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "rate")) {
        arg_rate = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.rate));
        if (ret) {
          printf("Incorrect value for --rate argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "cp-interval")) {
        arg_checkpoint_interval = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.checkpoint_interval));
        if (ret) {
          printf("Incorrect value for --cp-interval argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "ts-interval")) {
        arg_ts_interval = 1;
        ret = parse_arg_uint32(optarg, &(app.p4xos_conf.ts_interval));
        if (ret) {
          printf("Incorrect value for --ts-interval argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "src")) {
        src_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.mine));
        if (ret) {
          printf("Incorrect value for --src argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "dst")) {
        dst_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.paxos_leader));
        if (ret) {
          printf("Incorrect value for --dst argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "cliaddr")) {
        cli_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.client));
        if (ret) {
          printf("Incorrect value for --cliaddr argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "pri")) {
        pri_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.primary_replica));
        if (ret) {
          printf("Incorrect value for --pri argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "acc-addr")) {
        acc_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.acceptor_addr));
        if (ret) {
          printf("Incorrect value for --acc-addr argument (%d)\n", ret);
          return -1;
        }
      }
      if (!strcmp(lgopts[option_index].name, "learner-addr")) {
        learner_addr = 1;
        ret = parse_arg_ip_address(optarg, &(app.p4xos_conf.learner_addr));
        if (ret) {
          printf("Incorrect value for --learner-addr argument (%d)\n", ret);
          return -1;
        }
      }
      break;

    default:
      return -1;
    }
  }

  /* Check that all mandatory arguments are provided */
  if ((arg_rx == 0) || (arg_tx == 0) || (arg_w == 0) || (arg_lpm == 0)) {
    printf("Not all mandatory arguments are present\n");
    return -1;
  }

  /* Assign default values for the optional arguments not provided */
  if (arg_rsz == 0) {
    app.nic_rx_ring_size = APP_DEFAULT_NIC_RX_RING_SIZE;
    app.nic_tx_ring_size = APP_DEFAULT_NIC_TX_RING_SIZE;
    app.ring_rx_size = APP_DEFAULT_RING_RX_SIZE;
    app.ring_tx_size = APP_DEFAULT_RING_TX_SIZE;
  }

  if (arg_bsz == 0) {
    app.burst_size_io_rx_read = APP_DEFAULT_BURST_SIZE_IO_RX_READ;
    app.burst_size_io_rx_write = APP_DEFAULT_BURST_SIZE_IO_RX_WRITE;
    app.burst_size_io_tx_read = APP_DEFAULT_BURST_SIZE_IO_TX_READ;
    app.burst_size_io_tx_write = APP_DEFAULT_BURST_SIZE_IO_TX_WRITE;
    app.burst_size_worker_read = APP_DEFAULT_BURST_SIZE_WORKER_READ;
    app.burst_size_worker_write = APP_DEFAULT_BURST_SIZE_WORKER_WRITE;
  }

  if (arg_pos_lb == 0) {
    app.pos_lb = APP_DEFAULT_IO_RX_LB_POS;
  }

  if (arg_num_ac == 0) {
    app.p4xos_conf.num_acceptors = APP_DEFAULT_NUM_ACCEPTORS;
  }

  if (cli_addr == 0) {
    parse_arg_ip_address(APP_DEFAULT_IP_SRC_ADDR, &(app.p4xos_conf.client));
  }

  if (src_addr == 0) {
    parse_arg_ip_address(APP_DEFAULT_IP_SRC_ADDR, &(app.p4xos_conf.mine));
  }

  if (dst_addr == 0) {
    parse_arg_ip_address(APP_DEFAULT_IP_DST_ADDR, &(app.p4xos_conf.paxos_leader));
  }

  if (pri_addr == 0) {
      parse_arg_ip_address(APP_DEFAULT_IP_BACKUP_DST_ADDR, &(app.p4xos_conf.primary_replica));
  }

  if (acc_addr == 0) {
      parse_arg_ip_address(APP_DEFAULT_IP_ACCEPTOR_ADDR, &(app.p4xos_conf.acceptor_addr));
  }

  if (learner_addr == 0) {
      parse_arg_ip_address(APP_DEFAULT_IP_LEARNER_ADDR, &(app.p4xos_conf.learner_addr));
  }

  if (msgtype == 0) {
    app.p4xos_conf.msgtype = APP_DEFAULT_MESSAGE_TYPE;
  }

  if (osd == 0) {
    app.p4xos_conf.osd = APP_DEFAULT_OUTSTANDING;
  }

  if (node_id == 0) {
    app.p4xos_conf.node_id = APP_DEFAULT_NODE_ID;
  }

  if (tx_port == 0) {
    app.p4xos_conf.tx_port = APP_DEFAULT_TX_PORT;
  }

  if (arg_baseline == 0) {
    app.p4xos_conf.baseline = APP_DEFAULT_FALSE;
  }

  if (arg_multi_dbs == 0) {
    app.p4xos_conf.multi_dbs = APP_DEFAULT_FALSE;
  }

  if (arg_reset_inst == 0) {
    app.p4xos_conf.reset_inst = APP_DEFAULT_FALSE;
  }

  if (arg_inc_inst == 0) {
    app.p4xos_conf.inc_inst = APP_DEFAULT_FALSE;
  }

  if (arg_checkpoint_interval == 0) {
    app.p4xos_conf.checkpoint_interval = APP_DEFAULT_CHECKPOINT_INTERVAL;
  }

  if (arg_ts_interval == 0) {
    app.p4xos_conf.ts_interval = APP_DEFAULT_TS_INTERVAL;
  }

  if (arg_drop == 0) {
    app.p4xos_conf.drop = APP_DEFAULT_FALSE;
  }

  if (arg_resp == 0) {
    app.p4xos_conf.respond_to_client = APP_DEFAULT_FALSE;
  }

  if (arg_latency == 0) {
    app.p4xos_conf.measure_latency = APP_DEFAULT_FALSE;
  }

  if (arg_run_prepare == 0) {
    app.p4xos_conf.run_prepare = APP_DEFAULT_FALSE;
  }

  if (arg_leader == 0) {
    app.p4xos_conf.leader = APP_DEFAULT_FALSE;
  }

  if (arg_max_inst == 0) {
    app.p4xos_conf.max_inst = APP_DEFAULT_MAX_INST;
  }

  if (arg_rate == 0) {
    app.p4xos_conf.rate = APP_DEFAULT_SENDING_RATE;
  }

  if (arg_window == 0) {
      app.p4xos_conf.preexec_window = APP_PREEXEC_WINDOW;
  }

  /* Check cross-consistency of arguments */
  // if ((ret = app_check_lpm_table()) < 0) {
  // 	printf("At least one LPM rule is inconsistent (%d)\n", ret);
  // 	return -1;
  // }
  if (app_check_every_rx_port_is_tx_enabled() < 0) {
    printf("On LPM lookup miss, packet is sent back on the input port.\n");
    printf("At least one RX port is not enabled for TX.\n");
    return -2;
  }

  if (optind >= 0)
    argv[optind - 1] = prgname;

  ret = optind - 1;
  optind = 1; /* reset getopt lib */
  return ret;
}

int app_get_nic_rx_queues_per_port(uint16_t port) {
  uint32_t i, count;

  if (port >= APP_MAX_NIC_PORTS) {
    return -1;
  }

  count = 0;
  for (i = 0; i < APP_MAX_RX_QUEUES_PER_NIC_PORT; i++) {
    if (app.nic_rx_queue_mask[port][i] == 1) {
      count++;
    }
  }

  return count;
}

int app_get_lcore_for_nic_rx(uint16_t port, uint8_t queue,
                             uint32_t *lcore_out) {
  uint32_t lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
    uint32_t i;

    if (app.lcore_params[lcore].type != e_APP_LCORE_IO) {
      continue;
    }

    const size_t n_queues =
        RTE_MIN(lp->rx.n_nic_queues, RTE_DIM(lp->rx.nic_queues));
    for (i = 0; i < n_queues; i++) {
      if ((lp->rx.nic_queues[i].port == port) &&
          (lp->rx.nic_queues[i].queue == queue)) {
        *lcore_out = lcore;
        return 0;
      }
    }
  }

  return -1;
}

int app_get_lcore_for_nic_tx(uint16_t port, uint32_t *lcore_out) {
  uint32_t lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
    uint32_t i;

    if (app.lcore_params[lcore].type != e_APP_LCORE_IO) {
      continue;
    }

    const size_t n_ports =
        RTE_MIN(lp->tx.n_nic_ports, RTE_DIM(lp->tx.nic_ports));
    for (i = 0; i < n_ports; i++) {
      if (lp->tx.nic_ports[i] == port) {
        *lcore_out = lcore;
        return 0;
      }
    }
  }

  return -1;
}

int app_is_socket_used(uint32_t socket) {
  uint32_t lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    if (app.lcore_params[lcore].type == e_APP_LCORE_DISABLED) {
      continue;
    }

    if (socket == rte_lcore_to_socket_id(lcore)) {
      return 1;
    }
  }

  return 0;
}

uint32_t app_get_lcores_io_rx(void) {
  uint32_t lcore, count;

  count = 0;
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;

    if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
        (lp_io->rx.n_nic_queues == 0)) {
      continue;
    }

    count++;
  }

  return count;
}

uint32_t app_get_lcores_worker(void) {
  uint32_t lcore, count;

  count = 0;
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }

    count++;
  }

  if (count > APP_MAX_WORKER_LCORES) {
    rte_panic("Algorithmic error (too many worker lcores)\n");
    return 0;
  }

  return count;
}

struct app_lcore_params_worker *app_get_worker(uint32_t worker_id) {
  uint32_t lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;
    if (lp->worker_id == worker_id)
      return lp;
  }

  return NULL;
}

int app_get_lcore_worker(uint32_t worker_id) {
  int lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;
    if (lp->worker_id == worker_id)
      return lcore;
  }

  return -1;
}

void app_print_params(void) {
  unsigned port, queue, lcore, rule, i, j;

  /* Print NIC RX configuration */
  printf("NIC RX ports: ");
  for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
    uint32_t n_rx_queues = app_get_nic_rx_queues_per_port(port);

    if (n_rx_queues == 0) {
      continue;
    }

    printf("%u (", port);
    for (queue = 0; queue < APP_MAX_RX_QUEUES_PER_NIC_PORT; queue++) {
      if (app.nic_rx_queue_mask[port][queue] == 1) {
        printf("%u ", queue);
      }
    }
    printf(")  ");
  }
  printf(";\n");

  /* Print I/O lcore RX params */
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;

    if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
        (lp->rx.n_nic_queues == 0)) {
      continue;
    }

    printf("I/O lcore %u (socket %u): ", lcore, rte_lcore_to_socket_id(lcore));

    printf("RX ports  ");
    for (i = 0; i < lp->rx.n_nic_queues; i++) {
      printf("(%u, %u)  ", (unsigned)lp->rx.nic_queues[i].port,
             (unsigned)lp->rx.nic_queues[i].queue);
    }
    printf("; ");

    printf("Output rings  ");
    for (i = 0; i < lp->rx.n_rings; i++) {
      printf("%p  ", lp->rx.rings[i]);
    }
    printf(";\n");
  }

  /* Print worker lcore RX params */
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }

    printf("Worker lcore %u (socket %u) ID %u: ", lcore,
           rte_lcore_to_socket_id(lcore), (unsigned)lp->worker_id);

    printf("Input rings  ");
    for (i = 0; i < lp->n_rings_in; i++) {
      printf("%p  ", lp->rings_in[i]);
    }

    printf(";\n");
  }

  printf("\n");

  /* Print NIC TX configuration */
  printf("NIC TX ports:  ");
  for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
    if (app.nic_tx_port_mask[port] == 1) {
      printf("%u  ", port);
    }
  }
  printf(";\n");

  /* Print I/O TX lcore params */
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
    uint32_t n_workers = app_get_lcores_worker();

    if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
        (lp->tx.n_nic_ports == 0)) {
      continue;
    }

    printf("I/O lcore %u (socket %u): ", lcore, rte_lcore_to_socket_id(lcore));

    printf("Input rings per TX port  ");
    for (i = 0; i < lp->tx.n_nic_ports; i++) {
      port = lp->tx.nic_ports[i];

      printf("%u (", port);
      for (j = 0; j < n_workers; j++) {
        printf("%p  ", lp->tx.rings[port][j]);
      }
      printf(")  ");
    }

    printf(";\n");
  }

  /* Print worker lcore TX params */
  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }

    printf("Worker lcore %u (socket %u) ID %u: \n", lcore,
           rte_lcore_to_socket_id(lcore), (unsigned)lp->worker_id);

    printf("Output rings per TX port  ");
    for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
      if (lp->rings_out[port] != NULL) {
        printf("%u (%p)  ", port, lp->rings_out[port]);
      }
    }

    printf(";\n");
  }

  /* Print LPM rules */
  printf("LPM rules: \n");
  for (rule = 0; rule < app.n_lpm_rules; rule++) {
    uint32_t ip = app.lpm_rules[rule].ip;
    uint8_t depth = app.lpm_rules[rule].depth;
    uint8_t if_out = app.lpm_rules[rule].if_out;

    printf("\t%u: %u.%u.%u.%u/%u => %u;\n", rule,
           (unsigned)(ip & 0xFF000000) >> 24, (unsigned)(ip & 0x00FF0000) >> 16,
           (unsigned)(ip & 0x0000FF00) >> 8, (unsigned)ip & 0x000000FF,
           (unsigned)depth, (unsigned)if_out);
  }

  /* Rings */
  printf("Ring sizes: NIC RX = %u; Worker in = %u; Worker out = %u; NIC TX = "
         "%u;\n",
         (unsigned)app.nic_rx_ring_size, (unsigned)app.ring_rx_size,
         (unsigned)app.ring_tx_size, (unsigned)app.nic_tx_ring_size);

  /* Bursts */
  printf("Burst sizes: I/O RX (rd = %u, wr = %u); Worker (rd = %u, wr = %u); "
         "I/O TX (rd = %u, wr = %u)\n",
         (unsigned)app.burst_size_io_rx_read,
         (unsigned)app.burst_size_io_rx_write,
         (unsigned)app.burst_size_worker_read,
         (unsigned)app.burst_size_worker_write,
         (unsigned)app.burst_size_io_tx_read,
         (unsigned)app.burst_size_io_tx_write);

  /* LB-POS */
  printf("Load balacing position: %u\n", app.pos_lb);

  /* Paxos */
  printf("Number of acceptors: %u\n"
         "Message type: %u\n"
         "Acceptor ID: %u\n"
         "Multiple DBs: %u\n"
         "Is leader: %u\n"
         "Max Instance: %u\n"
         "Proposer TX port: %u\n"
         "Respond to Client: %u\n"
         "Outstanding packets: %u\n",
         app.p4xos_conf.num_acceptors, app.p4xos_conf.msgtype,
         app.p4xos_conf.node_id, app.p4xos_conf.multi_dbs,
         app.p4xos_conf.leader,
         app.p4xos_conf.max_inst, app.p4xos_conf.tx_port,
         app.p4xos_conf.respond_to_client, app.p4xos_conf.osd);
  char str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(app.p4xos_conf.paxos_leader.sin_addr), str, INET_ADDRSTRLEN);
  printf("Leader address: %s:%d\n", str, ntohs(app.p4xos_conf.paxos_leader.sin_port));
  inet_ntop(AF_INET, &(app.p4xos_conf.acceptor_addr.sin_addr), str, INET_ADDRSTRLEN);
  printf("Acceptor address: %s:%d\n", str, ntohs(app.p4xos_conf.acceptor_addr.sin_port));
  inet_ntop(AF_INET, &(app.p4xos_conf.learner_addr.sin_addr), str, INET_ADDRSTRLEN);
  printf("Learner address: %s:%d\n", str, ntohs(app.p4xos_conf.learner_addr.sin_port));
  inet_ntop(AF_INET, &(app.p4xos_conf.mine.sin_addr), str, INET_ADDRSTRLEN);
  printf("Source address: %s:%d\n", str, ntohs(app.p4xos_conf.mine.sin_port));
  inet_ntop(AF_INET, &(app.p4xos_conf.client.sin_addr), str, INET_ADDRSTRLEN);
  printf("Client address: %s:%d\n", str, ntohs(app.p4xos_conf.client.sin_port));
}
