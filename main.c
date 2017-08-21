/* initial project*/
/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <unistd.h>

#include "rte_config.h"
#include "rte_malloc.h"
#include "rte_mempool.h"
#include "rte_ethdev.h"
#include "rte_ether.h"
#include "rte_mbuf.h"
#include "rte_arp.h"
#include "rte_ip.h"

#define PHY_HEADER_LEN 14
#define	IPV4_HEADER_LEN 20

const int MBUF_SIZE = 4096;
const int MBUF_CACHE_SIZE = 512;
const int NUM_MBUFS = 16 * 1024;
const int RX_RING_SIZE = 1024;
const int TX_RING_SIZE = 1024;

const char *mbuf_pool_name = "pktmbuf_pool";
struct rte_mempool *mbuf_pool;

int main (int argc, char **argv)
{
	int ret, nb_ports, pkt_count;
	int i, j;
	struct ether_addr *ea;
	struct rte_eth_conf port_conf;
	struct rte_eth_dev_info dev_info;
	const uint16_t rx_rings = 1, tx_rings = 2;
	struct rte_mbuf * rx_pkts[16];
	int target = 0, totall = 0;

	/* Initiate RTE EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");

	/* Get current DPDK device ports */
	nb_ports = rte_eth_dev_count();
	if (nb_ports <= 0)
		rte_exit(EXIT_FAILURE, "There is no available port now\n");

	/* Create memory pool */
	mbuf_pool = rte_mempool_create(mbuf_pool_name,
									NUM_MBUFS * nb_ports,
									MBUF_SIZE,
									MBUF_CACHE_SIZE,
									sizeof(struct rte_pktmbuf_pool_private),
									rte_pktmbuf_pool_init, NULL,
									rte_pktmbuf_init, NULL,
									rte_socket_id(), 0);
	if (!mbuf_pool)
		rte_exit(EXIT_FAILURE, "Failed to create mbuf pool\n");

	/* Configure ports and net device */
	for (i = 0; i < nb_ports; ++i)
	{
		memset(&port_conf, 0, sizeof(port_conf));
		port_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;

		memset(&dev_info, 0, sizeof(dev_info));
		rte_eth_dev_info_get(i, &dev_info);

		ret = rte_eth_dev_configure(i, rx_rings, tx_rings, &port_conf);
		if (ret != 0)
			rte_exit(EXIT_FAILURE, "Failed on rte_eth_dev_configure\n");

		for (j = 0; j < rx_rings; j++)
		{
			/* Configure RX queue */
			ret = rte_eth_rx_queue_setup(i, j, RX_RING_SIZE, rte_eth_dev_socket_id(i), NULL, mbuf_pool);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Failed on rte_eth_rx_queue_setup\n");
		}

		for (j = 0; j < tx_rings; j++)
		{
			/* Configure TX queue */
			ret = rte_eth_tx_queue_setup(i, j, TX_RING_SIZE, rte_eth_dev_socket_id(i), NULL);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Failed on rte_eth_tx_queue_setup\n");
		}

		/* Start device */
		ret = rte_eth_dev_start(i);

		/* Enable promiscuous mode */
		rte_eth_promiscuous_enable(i);
	}

	/* Main loop for receiving packet */
	while (1)
	{
		pkt_count = rte_eth_rx_burst(0, 0, rx_pkts, 16);
		if (pkt_count > 0)
		{
			total += pkt_count;
			for (i = 0; i < pkt_count; ++i)
			{
				struct rte_mbuf *mbuf = rx_pkts[i];
				char *key;
				ea = rte_pktmbuf_mtod(mbuf, struct ether_addr *);
				if (is_multicast_ether_addr(ea))
				{
					if (is_broadcast_ether_addr(ea))
					{
						/* Do nothing for broadcast packet */
					} else {
						//rte_pktmbuf_dump(stdout, mbuf, rte_pktmbuf_data_len(mbuf));
						key = rte_crtlmbuf_data(mbuf);
						if (key[PHY_HEADER_LEN + IPV4_HEADER_LEN] == 0x1B)
							target++;
					}
					rte_pktmbuf_free(mbuf);
					rx_pkts[i] = 0;
				}
			}
			printf("Total packets received: %d, Target packets received: %d\n", total, target);
		}
	}

	return 0;
}