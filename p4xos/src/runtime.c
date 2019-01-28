/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

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
#include <arpa/inet.h>

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
#include <rte_udp.h>
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
#include <rte_ring.h>
#include <rte_tcp.h>
#include <rte_pause.h>
#include <rte_arp.h>
#include <rte_icmp.h>

#include "dpp_paxos.h"
#include "main.h"

#ifndef APP_LCORE_IO_FLUSH
#define APP_LCORE_IO_FLUSH 2000000
#endif

#ifndef APP_LCORE_WORKER_FLUSH
#define APP_LCORE_WORKER_FLUSH 2000000
#endif

#ifndef APP_STATS
#define APP_STATS 2000000
#endif


#define APP_IO_RX_DROP_ALL_PACKETS 0
#define APP_WORKER_DROP_ALL_PACKETS 0
#define APP_IO_TX_DROP_ALL_PACKETS 0

#ifndef APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH_ENABLE 1
#endif

#ifndef APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH_ENABLE 1
#endif

#ifndef APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH_ENABLE 1
#endif

#if APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH0(p) rte_prefetch0(p)
#define APP_IO_RX_PREFETCH1(p) rte_prefetch1(p)
#else
#define APP_IO_RX_PREFETCH0(p)
#define APP_IO_RX_PREFETCH1(p)
#endif

#if APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH0(p) rte_prefetch0(p)
#define APP_WORKER_PREFETCH1(p) rte_prefetch1(p)
#else
#define APP_WORKER_PREFETCH0(p)
#define APP_WORKER_PREFETCH1(p)
#endif

#if APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH0(p) rte_prefetch0(p)
#define APP_IO_TX_PREFETCH1(p) rte_prefetch1(p)
#else
#define APP_IO_TX_PREFETCH0(p)
#define APP_IO_TX_PREFETCH1(p)
#endif

static inline void
app_lcore_io_rx_buffer_to_send(struct app_lcore_params_io *lp, uint32_t worker,
                               struct rte_mbuf *mbuf, uint32_t bsz) {
    uint32_t pos;
    pos = lp->rx.mbuf_out[worker].n_mbufs;
    lp->rx.mbuf_out[worker].array[pos++] = mbuf;
    if (likely(pos < bsz)) {
        lp->rx.mbuf_out[worker].n_mbufs = pos;
        return;
    }

    int ret = rte_ring_sp_enqueue_bulk(lp->rx.rings[worker],
             (void **)lp->rx.mbuf_out[worker].array, bsz,
             NULL);
    if (unlikely(ret == 0))
    {
        uint32_t k;
        for (k = 0; k < bsz; k++)
            rte_pktmbuf_free(lp->rx.mbuf_out[worker].array[k]);
        lp->rx.rings_drop[worker] += bsz;
    };

    lp->rx.mbuf_out[worker].n_mbufs = 0;
    lp->rx.mbuf_out_flush[worker] = 0;

#if APP_STATS
    lp->rx.rings_iters[worker]++;
    if (likely(ret == 0)) {
        lp->rx.rings_count[worker] += bsz;
    }
    if (unlikely(lp->rx.rings_iters[worker] == APP_STATS)) {
        unsigned lcore = rte_lcore_id();
        printf("I/O RX %u out (worker %u): dropped %u, enq success rate = %.2f\n",
            lcore, (unsigned)worker, lp->rx.rings_drop[worker],
            ((double)lp->rx.rings_count[worker]) /
            ((double)lp->rx.rings_iters[worker]));
        lp->rx.rings_iters[worker] = 0;
        lp->rx.rings_count[worker] = 0;
        lp->rx.rings_drop[worker] = 0;
    }
#endif
}

static inline void
app_lcore_io_rx(struct app_lcore_params_io *lp,
               uint32_t n_workers, uint32_t bsz_rd,
               uint32_t bsz_wr, uint8_t pos_lb)
{
    struct rte_mbuf *mbuf_1_0, *mbuf_1_1, *mbuf_2_0, *mbuf_2_1;
    uint8_t *data_1_0, *data_1_1 = NULL;
    uint32_t i;

    for (i = 0; i < lp->rx.n_nic_queues; i++) {
        uint16_t port = lp->rx.nic_queues[i].port;
        uint8_t queue = lp->rx.nic_queues[i].queue;
        uint32_t n_mbufs, j;

        n_mbufs = rte_eth_rx_burst(port, queue, lp->rx.mbuf_in.array, (uint16_t)bsz_rd);

        if (unlikely(n_mbufs == 0)) {
            continue;
        }

#if APP_STATS
        lp->rx.nic_queues_iters[i]++;
        lp->rx.nic_queues_count[i] += n_mbufs;
        if (unlikely(lp->rx.nic_queues_iters[i] == APP_STATS)) {
            struct rte_eth_stats stats;
            unsigned lcore = rte_lcore_id();

            rte_eth_stats_get(port, &stats);

            printf("I/O RX %u in (NIC port %u): NIC rx %" PRIu64 ", tx %" PRIu64
                ", missed %" PRIu64 ", rx err: %" PRIu64 ", tx err %" PRIu64
                ", mbuf err: %" PRIu64 ", avg burst size "
                "= %.2f\n",
                lcore, port, stats.ipackets, stats.opackets, stats.imissed,
                stats.ierrors, stats.oerrors, stats.rx_nombuf,
                ((double)lp->rx.nic_queues_count[i]) /
                ((double)lp->rx.nic_queues_iters[i]));
            lp->rx.nic_queues_iters[i] = 0;
            lp->rx.nic_queues_count[i] = 0;
        }
#endif

        mbuf_1_0 = lp->rx.mbuf_in.array[0];
        mbuf_1_1 = lp->rx.mbuf_in.array[1];
        data_1_0 = rte_pktmbuf_mtod(mbuf_1_0, uint8_t *);
        if (likely(n_mbufs > 1)) {
            data_1_1 = rte_pktmbuf_mtod(mbuf_1_1, uint8_t *);
        }

        mbuf_2_0 = lp->rx.mbuf_in.array[2];
        mbuf_2_1 = lp->rx.mbuf_in.array[3];
        APP_IO_RX_PREFETCH0(mbuf_2_0);
        APP_IO_RX_PREFETCH0(mbuf_2_1);

        for (j = 0; j + 3 < n_mbufs; j += 2) {
            struct rte_mbuf *mbuf_0_0, *mbuf_0_1;
            uint8_t *data_0_0, *data_0_1;
            uint32_t worker_0, worker_1;

            mbuf_0_0 = mbuf_1_0;
            mbuf_0_1 = mbuf_1_1;
            data_0_0 = data_1_0;
            data_0_1 = data_1_1;

            mbuf_1_0 = mbuf_2_0;
            mbuf_1_1 = mbuf_2_1;
            data_1_0 = rte_pktmbuf_mtod(mbuf_2_0, uint8_t *);
            data_1_1 = rte_pktmbuf_mtod(mbuf_2_1, uint8_t *);
            APP_IO_RX_PREFETCH0(data_1_0);
            APP_IO_RX_PREFETCH0(data_1_1);

            mbuf_2_0 = lp->rx.mbuf_in.array[j + 4];
            mbuf_2_1 = lp->rx.mbuf_in.array[j + 5];
            APP_IO_RX_PREFETCH0(mbuf_2_0);
            APP_IO_RX_PREFETCH0(mbuf_2_1);

            worker_0 = data_0_0[pos_lb] % (n_workers);
            worker_1 = data_0_1[pos_lb] % (n_workers);

            app_lcore_io_rx_buffer_to_send(lp, worker_0, mbuf_0_0, bsz_wr);
            app_lcore_io_rx_buffer_to_send(lp, worker_1, mbuf_0_1, bsz_wr);
        }

        /* Handle the last 1, 2 (when n_mbufs is even) or 3 (when n_mbufs is odd)
        * packets  */
        for (; j < n_mbufs; j += 1) {
            struct rte_mbuf *mbuf;
            uint8_t *data;
            uint32_t worker;

            mbuf = mbuf_1_0;
            mbuf_1_0 = mbuf_1_1;
            mbuf_1_1 = mbuf_2_0;
            mbuf_2_0 = mbuf_2_1;

            data = rte_pktmbuf_mtod(mbuf, uint8_t *);

            APP_IO_RX_PREFETCH0(mbuf_1_0);

            worker = data[pos_lb] % (n_workers);

            app_lcore_io_rx_buffer_to_send(lp, worker, mbuf, bsz_wr);
        }
    }
}

static inline void
app_lcore_io_rx_flush(struct app_lcore_params_io *lp, uint32_t n_workers)
{
    uint32_t worker;

    for (worker = 0; worker < n_workers; worker++) {
        if (likely((lp->rx.mbuf_out_flush[worker] == 0) ||
                    (lp->rx.mbuf_out[worker].n_mbufs == 0))) {
            lp->rx.mbuf_out_flush[worker] = 1;
            continue;
        }

        int ret = rte_ring_sp_enqueue_bulk(lp->rx.rings[worker],
            (void **)lp->rx.mbuf_out[worker].array,
            lp->rx.mbuf_out[worker].n_mbufs, NULL);
        if (unlikely(ret == 0))
        {
            uint32_t k;
			for (k = 0; k < lp->rx.mbuf_out[worker].n_mbufs; k ++) {
				struct rte_mbuf *pkt_to_free = lp->rx.mbuf_out[worker].array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
            lp->rx.rings_drop[worker] += lp->rx.mbuf_out[worker].n_mbufs;
        }

        lp->rx.mbuf_out[worker].n_mbufs = 0;
        lp->rx.mbuf_out_flush[worker] = 1;
    }
}

inline int
app_send_burst(uint16_t port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
    uint32_t sent = 0;

    // sent = rte_eth_tx_burst(port, 0, pkts, n_pkts);
    //
    // if (unlikely(sent < n_pkts)) {
    //     RTE_LOG(WARNING, P4XOS, "%s: Request %u Sent %u. Drop %u packets at TX\n",
    //             __func__, n_pkts, sent, n_pkts - sent);
    //     uint32_t k = sent;
    //     for (; k < n_pkts; k++) {
    //         rte_pktmbuf_free(pkts[k]);
    //     }
    // }

    do {
        sent += rte_eth_tx_burst(port, 0, pkts, n_pkts);
        rte_pause();
    } while (sent < n_pkts);

    return sent;
}

inline void
flush_port(uint16_t port)
{
    uint32_t lcore;
    if (app_get_lcore_for_nic_tx(port, &lcore) < 0) {
        rte_panic("Error: get lcore tx\n");
        return;
    }
    struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;
    app_send_burst(port, lp_io->tx.mbuf_out[port].array, lp_io->tx.mbuf_out[port].n_mbufs);
}

static inline void
app_lcore_io_tx(
    struct app_lcore_params_io *lp,
    uint32_t n_workers, uint32_t bsz_rd,
    uint32_t bsz_wr)
{
    uint32_t worker;

    for (worker = 0; worker < n_workers; worker++) {
        uint32_t i;
        uint32_t n_pkts;

        for (i = 0; i < lp->tx.n_nic_ports; i++) {
            uint16_t port = lp->tx.nic_ports[i];
            struct rte_ring *ring = lp->tx.rings[port][worker];
            uint32_t n_mbufs;
            int ret;

            n_mbufs = lp->tx.mbuf_out[port].n_mbufs;
            ret = rte_ring_sc_dequeue_bulk(
                ring,
                (void **)&lp->tx.mbuf_out[port].array[n_mbufs],
                bsz_rd,
                NULL);

            if (unlikely(ret == 0))
                continue;

            n_mbufs += bsz_rd;

            if (unlikely(n_mbufs < bsz_wr)) {
                lp->tx.mbuf_out[port].n_mbufs = n_mbufs;
                continue;
            }

#ifdef RATE_LIMITER
            struct rte_mbuf *pkts_tx[n_mbufs];
            uint32_t nb_eq = rte_sched_port_enqueue(lp->tx.sched_port,
                                            lp->tx.mbuf_out[port].array,
                                            n_mbufs);

            uint32_t nb_deq = rte_sched_port_dequeue(lp->tx.sched_port, pkts_tx, n_mbufs);
            if (unlikely(nb_deq == 0)) {
                lp->tx.mbuf_out[port].n_mbufs = 0;
                lp->tx.mbuf_out_flush[port] = 0;
                continue;
            }
            n_pkts = app_send_burst(port, lp->tx.mbuf_out[port].array, nb_deq);
#else
            n_pkts = app_send_burst(port, lp->tx.mbuf_out[port].array, n_mbufs);
#endif

#if APP_STATS
			lp->tx.nic_ports_iters[port] ++;
			lp->tx.nic_ports_count[port] += n_pkts;
			if (unlikely(lp->tx.nic_ports_iters[port] == APP_STATS)) {
				unsigned lcore = rte_lcore_id();

				printf("\t\t\tI/O TX %u out (port %u): avg burst size = %.2f\n",
					lcore,
					port,
					((double) lp->tx.nic_ports_count[port]) / ((double) lp->tx.nic_ports_iters[port]));
				lp->tx.nic_ports_iters[port] = 0;
				lp->tx.nic_ports_count[port] = 0;
			}
#endif

            lp->tx.mbuf_out[port].n_mbufs = 0;
            lp->tx.mbuf_out_flush[port] = 0;
        }
    }
}

static inline void
app_lcore_io_tx_flush(struct app_lcore_params_io *lp)
{
    uint16_t port;
    uint32_t i;
    uint32_t n_pkts;

    for (i = 0; i < lp->tx.n_nic_ports; i++) {

        port = lp->tx.nic_ports[i];
        if (likely((lp->tx.mbuf_out_flush[port] == 0) ||
                    (lp->tx.mbuf_out[port].n_mbufs == 0))) {
            lp->tx.mbuf_out_flush[port] = 1;
            continue;
        }

#ifdef RATE_LIMITER
        uint32_t bsz_wr = app.burst_size_io_tx_write;
        struct rte_mbuf *pkts_tx[bsz_wr];
        uint32_t nb_eq = rte_sched_port_enqueue(lp->tx.sched_port,
                                        lp->tx.mbuf_out[port].array,
                                        lp->tx.mbuf_out[port].n_mbufs);

        uint32_t nb_deq = rte_sched_port_dequeue(lp->tx.sched_port, pkts_tx, bsz_wr);
        if (unlikely(nb_deq == 0)) {
            lp->tx.mbuf_out[port].n_mbufs = 0;
            lp->tx.mbuf_out_flush[port] = 1;
            continue;
        }
        n_pkts = app_send_burst(port, lp->tx.mbuf_out[port].array, nb_deq);
#else
        n_pkts = app_send_burst(port, lp->tx.mbuf_out[port].array, lp->tx.mbuf_out[port].n_mbufs);
#endif

#if APP_STATS
			lp->tx.nic_ports_iters[port] ++;
			lp->tx.nic_ports_count[port] += n_pkts;
			if (unlikely(lp->tx.nic_ports_iters[port] == APP_STATS)) {
				unsigned lcore = rte_lcore_id();

				printf("\t\t\tI/O TX %u out (port %u): avg burst size = %.2f\n",
					lcore,
					port,
					((double) lp->tx.nic_ports_count[port]) / ((double) lp->tx.nic_ports_iters[port]));
				lp->tx.nic_ports_iters[port] = 0;
				lp->tx.nic_ports_count[port] = 0;
			}
#endif

        lp->tx.mbuf_out[port].n_mbufs = 0;
        lp->tx.mbuf_out_flush[port] = 1;
    }
}

static void
app_lcore_main_loop_io(void)
{
    uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
    uint32_t lcore = rte_lcore_id();
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
    uint32_t n_workers = app_get_lcores_worker();
    uint64_t i = 0;

    uint32_t bsz_rx_rd = app.burst_size_io_rx_read;
    uint32_t bsz_rx_wr = app.burst_size_io_rx_write;
    uint32_t bsz_tx_rd = app.burst_size_io_tx_read;
    uint32_t bsz_tx_wr = app.burst_size_io_tx_write;

    uint8_t pos_lb = app.pos_lb;

    while (!app.force_quit) {

        cur_tsc = rte_get_timer_cycles();
        diff_tsc = cur_tsc - prev_tsc;
        if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
            rte_timer_manage();
            prev_tsc = cur_tsc;
        };

        if (APP_LCORE_IO_FLUSH && (unlikely(i == APP_LCORE_IO_FLUSH))) {
            if (likely(lp->rx.n_nic_queues > 0)) {
                app_lcore_io_rx_flush(lp, n_workers);
            }

            if (likely(lp->tx.n_nic_ports > 0)) {
                app_lcore_io_tx_flush(lp);
            }

            i = 0;
        }

        if (likely(lp->rx.n_nic_queues > 0)) {
            app_lcore_io_rx(lp, n_workers, bsz_rx_rd, bsz_rx_wr, pos_lb);
        }

        if (likely(lp->tx.n_nic_ports > 0)) {
            app_lcore_io_tx(lp, n_workers, bsz_tx_rd, bsz_tx_wr);
        }

        i++;
    }
}

static inline void
app_lcore_worker(
    struct app_lcore_params_worker *lp,
    uint32_t bsz_rd,
    uint32_t bsz_wr)
{
    uint32_t i;

    for (i = 0; i < lp->n_rings_in; i++) {
        struct rte_ring *ring_in = lp->rings_in[i];
        uint32_t j;
        int ret;
        ret = rte_ring_sc_dequeue_bulk(
            ring_in,
            (void **)lp->mbuf_in.array,
            bsz_rd,
            NULL);

        if (unlikely(ret == 0))
            continue;

        APP_WORKER_PREFETCH1(rte_pktmbuf_mtod(lp->mbuf_in.array[0], unsigned char *));
        APP_WORKER_PREFETCH0(lp->mbuf_in.array[1]);

        for (j = 0; j < bsz_rd; j++) {
            struct rte_mbuf *pkt;


            if (likely(j < bsz_rd - 1)) {
                APP_WORKER_PREFETCH1(
                    rte_pktmbuf_mtod(lp->mbuf_in.array[j + 1], unsigned char *));
            }
            if (likely(j < bsz_rd - 2)) {
                APP_WORKER_PREFETCH0(lp->mbuf_in.array[j + 2]);
            }

            pkt = lp->mbuf_in.array[j];

            struct arp_hdr *arp_hdr;
            struct ipv4_hdr *ipv4_hdr;
            struct udp_hdr *udp_hdr;
            struct icmp_hdr *icmp_hdr;
            uint32_t ip_dst, pos;
            uint32_t port;
            int ret;
            char src[INET_ADDRSTRLEN];
            char dst[INET_ADDRSTRLEN];

            struct ether_hdr *eth_hdr = rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, 0);
            size_t ip_offset = sizeof(struct ether_hdr);

            uint32_t bond_ip = app.p4xos_conf.mine.sin_addr.s_addr;
            uint16_t BOND_PORT = app.p4xos_conf.tx_port;
            struct ether_addr d_addr;
            switch (rte_be_to_cpu_16(eth_hdr->ether_type)) {
                case ETHER_TYPE_ARP:
                    arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, ip_offset);
                    inet_ntop(AF_INET, &(arp_hdr->arp_data.arp_sip), src, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &(arp_hdr->arp_data.arp_tip), dst, INET_ADDRSTRLEN);
                    RTE_LOG(DEBUG, P4XOS, "ARP: %s -> %s\n", src, dst);
                    if (arp_hdr->arp_data.arp_tip == bond_ip) {
                        if (arp_hdr->arp_op == rte_cpu_to_be_16(ARP_OP_REQUEST)) {
                            RTE_LOG(DEBUG, P4XOS, "ARP Request\n");
                            arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
                            /* Switch src and dst data and set bonding MAC */
                            ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
                            rte_eth_macaddr_get(BOND_PORT, &eth_hdr->s_addr);
                            ether_addr_copy(&arp_hdr->arp_data.arp_sha, &arp_hdr->arp_data.arp_tha);
                            arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
                            rte_eth_macaddr_get(BOND_PORT, &d_addr);
                            ether_addr_copy(&d_addr, &arp_hdr->arp_data.arp_sha);
                            arp_hdr->arp_data.arp_sip = bond_ip;
                            ip_dst = rte_be_to_cpu_32(bond_ip);
                            ret = 0;
                        }
                    }
                    break;
                case ETHER_TYPE_IPv4:
                    ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, ip_offset);
                    size_t l4_offset = ip_offset + sizeof(struct ipv4_hdr);
                    inet_ntop(AF_INET, &(ipv4_hdr->src_addr), src, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &(ipv4_hdr->dst_addr), dst, INET_ADDRSTRLEN);
                    ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

                    RTE_LOG(DEBUG, P4XOS, "IPv4: %s -> %s\n", src, dst);

                    switch (ipv4_hdr->next_proto_id) {
                        case IPPROTO_UDP:
                            udp_hdr = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, l4_offset);
                            ret = udp_hdr->dst_port;
                            break;
                        case IPPROTO_ICMP:
                            icmp_hdr = rte_pktmbuf_mtod_offset(pkt, struct icmp_hdr *, l4_offset);
                            RTE_LOG(DEBUG, P4XOS, "ICMP: %s -> %s: Type: %02x\n", src, dst, icmp_hdr->icmp_type);
                            if (icmp_hdr->icmp_type == IP_ICMP_ECHO_REQUEST) {
                                if (ipv4_hdr->dst_addr == bond_ip) {
                                    icmp_hdr->icmp_type = IP_ICMP_ECHO_REPLY;
                                    ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
                                    rte_eth_macaddr_get(BOND_PORT, &eth_hdr->s_addr);
                                    ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
                                    ipv4_hdr->src_addr = bond_ip;
                                    ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
                                    ret = 0;
                                }
                            }
                            break;
                        default:
                            ret = -1;
                            RTE_LOG(DEBUG, P4XOS, "IP Proto: %d\n", ipv4_hdr->next_proto_id);
                            break;
                    }
                    break;
                default:
                    RTE_LOG(DEBUG, P4XOS, "Ether Proto: 0x%04x\n", rte_be_to_cpu_16(eth_hdr->ether_type));
                    ret = -1;
                    break;
            }


            if (ret == app.p4xos_conf.paxos_leader.sin_port) {

                RTE_LOG(DEBUG, P4XOS, "Process packet. Code %d\n", ret);
                ret = lp->process_pkt(pkt, lp);
                if (ret < 0) {
                    RTE_LOG(DEBUG, P4XOS, "Process dropped packet. Code %d\n", ret);
                    rte_pktmbuf_free(pkt);
                    continue;
                }
            } else if (ret == lp->app_port) {
                RTE_LOG(DEBUG, P4XOS, "App processes packet. Code %d\n", ret);
                size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
                struct udp_hdr *udp =
                    rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, udp_offset);
                size_t data_offset = udp_offset + sizeof(struct udp_hdr);
                char* buffer = rte_pktmbuf_mtod_offset(pkt, char*, data_offset);
                size_t len = rte_be_to_cpu_16(udp->dgram_len) - sizeof(struct udp_hdr);
                struct sockaddr_in from;
                from.sin_family = AF_INET;
                from.sin_port = udp->src_port;
                from.sin_addr.s_addr = ipv4_hdr->src_addr;
                ret = lp->app_recvfrom(buffer, len, lp->worker_id, &from);
                if (ret < 0) {
                    RTE_LOG(DEBUG, P4XOS, "App dropped packet. Code %d\n", ret);
                    rte_pktmbuf_free(pkt);
                    continue;
                }
            } else  if (ret < 0) {
                RTE_LOG(DEBUG, P4XOS, "Runtime dropped packet. Code %d\n", ret);
                rte_pktmbuf_free(pkt);
                continue;
            }






            if (unlikely(rte_lpm_lookup(lp->lpm_table, ip_dst, &port) != 0)) {
                inet_ntop(AF_INET, &(ip_dst), dst, INET_ADDRSTRLEN);
                RTE_LOG(DEBUG, P4XOS, "Lookup dst %s dropped packet. Code\n", dst);
                rte_pktmbuf_free(pkt);
                continue;
            }

            uint32_t port_mask;
            if (IS_IPV4_MCAST(ip_dst)) {
                port_mask = port;
                /* Mark all packet's segments as referenced port_num times */
                // rte_pktmbuf_refcnt_update(pkt, (uint16_t)port_num);

                for (port = 0; port_mask > 0; port_mask >>= 1, port++) {
                    /* Prepare output packet and send it out. */
                    if ((port_mask & 1) != 0) {
                        RTE_LOG(DEBUG, P4XOS, "Multicast packet. Port %d\n", port);

                        pos = lp->mbuf_out[port].n_mbufs;
                        lp->mbuf_out[port].array[pos++] = pkt;
                        if (likely(pos < bsz_wr)) {
                            lp->mbuf_out[port].n_mbufs = pos;
                            continue;
                        }
                        ret = rte_ring_sp_enqueue_bulk(lp->rings_out[port],
                                        (void **)lp->mbuf_out[port].array,
                                        bsz_wr, NULL);

                        if (unlikely(ret == 0)) {
                            uint32_t k;
                            for (k = 0; k < bsz_wr; k ++) {
                                struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
                                rte_pktmbuf_free(pkt_to_free);
                            }
                            lp->rings_out_count_drop[port] += bsz_wr;
                        }

                        lp->mbuf_out[port].n_mbufs = 0;
                        lp->mbuf_out_flush[port] = 0;
                    }
                }
            } else {
                RTE_LOG(DEBUG, P4XOS, "Unicast packet. Port %d\n", port);
                pos = lp->mbuf_out[port].n_mbufs;
                lp->mbuf_out[port].array[pos++] = pkt;
                if (likely(pos < bsz_wr)) {
                    lp->mbuf_out[port].n_mbufs = pos;
                    continue;
                }

                int ret = rte_ring_sp_enqueue_bulk(lp->rings_out[port],
                                           (void **)lp->mbuf_out[port].array,
                                           bsz_wr, NULL);

                if (unlikely(ret == 0)) {
                    uint32_t k;
                    for (k = 0; k < bsz_wr; k ++) {
                        struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
                        rte_pktmbuf_free(pkt_to_free);
                    }
                    lp->rings_out_count_drop[port] += bsz_wr;
                }

#if APP_STATS
                lp->rings_out_iters[port] ++;
                if (ret > 0) {
                    lp->rings_out_count[port] += bsz_wr;
                }
                if (lp->rings_out_iters[port] == APP_STATS) {
                    printf("\t\tWorker %u out (NIC port %u): enq success rate = %.2f. Dropped %u\n",
                        (unsigned) lp->worker_id,
                        port,
                        ((double) lp->rings_out_count[port]) / ((double) lp->rings_out_iters[port]),
                        lp->rings_out_count_drop[port]);
                    lp->rings_out_iters[port] = 0;
                    lp->rings_out_count[port] = 0;
                    lp->rings_out_count_drop[port] = 0;
                }
#endif

                lp->mbuf_out[port].n_mbufs = 0;
                lp->mbuf_out_flush[port] = 0;
            }
        }
    }
}

static inline void
app_lcore_worker_flush(struct app_lcore_params_worker *lp)
{
    uint32_t port;

    for (port = 0; port < APP_MAX_NIC_PORTS; port++) {

        if (unlikely(lp->rings_out[port] == NULL)) {
            continue;
        }

        if (likely((lp->mbuf_out_flush[port] == 0) ||
                (lp->mbuf_out[port].n_mbufs == 0))) {
            lp->mbuf_out_flush[port] = 1;
            continue;
        }
        int ret = rte_ring_sp_enqueue_bulk(lp->rings_out[port],
                                   (void **)lp->mbuf_out[port].array,
                                   lp->mbuf_out[port].n_mbufs, NULL);

        if (unlikely(ret == 0)) {
            uint32_t k;
			for (k = 0; k < lp->mbuf_out[port].n_mbufs; k ++) {
				struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
				rte_pktmbuf_free(pkt_to_free);
            }
            lp->rings_out_count_drop[port] += lp->mbuf_out[port].n_mbufs;
        }

        lp->mbuf_out[port].n_mbufs = 0;
        lp->mbuf_out_flush[port] = 1;
    }
}

static void app_lcore_main_loop_worker(void) {
    uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
    uint32_t lcore = rte_lcore_id();
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;
    uint64_t i = 0;

    uint32_t bsz_rd = app.burst_size_worker_read;
    uint32_t bsz_wr = app.burst_size_worker_write;

    while (!app.force_quit) {
        cur_tsc = rte_get_timer_cycles();
        diff_tsc = cur_tsc - prev_tsc;
        if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
            rte_timer_manage();
            prev_tsc = cur_tsc;
        }

        if (APP_LCORE_WORKER_FLUSH && (unlikely(i == APP_LCORE_WORKER_FLUSH))) {
            app_lcore_worker_flush(lp);
            i = 0;
        }

        app_lcore_worker(lp, bsz_rd, bsz_wr);

        i++;
    }
}

int app_lcore_main_loop(__attribute__((unused)) void *arg) {
    struct app_lcore_params *lp;
    unsigned lcore;

    lcore = rte_lcore_id();
    lp = &app.lcore_params[lcore];

    if (lp->type == e_APP_LCORE_IO) {
        app_lcore_main_loop_io();
    }

    if (lp->type == e_APP_LCORE_WORKER) {
        app_lcore_main_loop_worker();
    }

    return 0;
}
