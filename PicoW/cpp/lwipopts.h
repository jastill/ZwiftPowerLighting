#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Common settings used in most of the pico_w examples
// (NO_SYS=1 for bare metal)
#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#if !NO_SYS
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_MBOX_SIZE 16
#define DEFAULT_THREAD_STACKSIZE 4096
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_THREAD_PRIO 1
#define LWIP_DEFAULT_SENDER_NETIF 1
#else
#define LWIP_TIMEVAL_PRIVATE 0 // This is optional but often needed
#endif

// Memory adjustments
#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE 4000
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 24

// Protocol Support
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 1
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

#ifndef NDEBUG
#define LWIP_DEBUG 1
#define LWIP_STATS 1
#define LWIP_STATS_DISPLAY 1
#endif

// Platform specific locking
#define SYS_LIGHTWEIGHT_PROT 1
#define LWIP_ALTCP 0
#define LWIP_ALTCP_TLS 0

#endif
