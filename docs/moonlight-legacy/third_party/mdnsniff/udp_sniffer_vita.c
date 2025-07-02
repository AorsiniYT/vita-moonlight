/*
 * Sniffer UDP mDNS para PSVita (VitaSDK)
 * Detección de servicios Moonlight/Sunshine (GameStream) en red local.
 *
 * Autor: AorsiniYT
 * Copyright 2025
 */

#ifdef __vita__
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "udp_sniffer_vita.h"
#define MDNS_LOG sceClibPrintf
#include "common/debugScreen.h"

#define BUF_SIZE 2048
#define MCAST_ADDR "224.0.0.251"
#define MCAST_PORT 5353
#define MAX_SUNSHINE 16
#define HOSTNAME_LEN 256
#define CACHE_TIMEOUT 10 // segundos

// Decodifica un nombre DNS comprimido
static int decode_dns_name(const unsigned char* msg, int msglen, int offset, char* out, int outlen) {
    int len = 0, jumped = 0, pos = offset, outpos = 0;
    while (pos < msglen) {
        unsigned char c = msg[pos];
        if (c == 0) { if (!jumped) len++; break; }
        if ((c & 0xC0) == 0xC0) {
            int ptr = ((c & 0x3F) << 8) | msg[pos+1];
            if (!jumped) len += 2;
            pos = ptr; jumped = 1; continue;
        }
        pos++;
        if (outpos && outpos < outlen-1) out[outpos++] = '.';
        for (int i = 0; i < c && pos < msglen && outpos < outlen-1; ++i) out[outpos++] = msg[pos++];
        if (!jumped) len += c + 1;
    }
    out[outpos] = '\0';
    return jumped ? len : (pos - offset + 1);
}

struct sunshine_entry {
    char host[HOSTNAME_LEN];
    int port;
    unsigned int last_seen;
    char ip[16];
    char target[HOSTNAME_LEN];
};

static struct sunshine_entry sunshine_table[MAX_SUNSHINE];
static int sock = -1;
static int initialized = 0;
static int moonlight_count = 0;

static void update_sunshine(const char* host, int port, const char* ip, const char* target) {
    unsigned int now = sceKernelGetProcessTimeLow() / 1000;
    int idx = -1;
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (strcmp(sunshine_table[i].host, host) == 0) { idx = i; break; }
    }
    if (idx == -1) {
        unsigned int oldest = now;
        for (int i = 0; i < MAX_SUNSHINE; ++i) {
            if (sunshine_table[i].host[0] == '\0') { idx = i; break; }
            if (sunshine_table[i].last_seen < oldest) { oldest = sunshine_table[i].last_seen; idx = i; }
        }
        if (idx >= 0) {
            strncpy(sunshine_table[idx].host, host, HOSTNAME_LEN);
            sunshine_table[idx].port = 0;
            sunshine_table[idx].ip[0] = '\0';
            sunshine_table[idx].target[0] = '\0';
        }
    }
    if (idx >= 0) {
        if (port > 0) sunshine_table[idx].port = port;
        if (ip) strncpy(sunshine_table[idx].ip, ip, sizeof(sunshine_table[idx].ip));
        if (target && target[0]) {
            strncpy(sunshine_table[idx].target, target, sizeof(sunshine_table[idx].target) - 1);
            sunshine_table[idx].target[sizeof(sunshine_table[idx].target) - 1] = '\0';
        }
        sunshine_table[idx].last_seen = now;
    }
}

static void update_sunshine_ip_by_target(const char* target, const char* ip) {
    unsigned int now = sceKernelGetProcessTimeLow() / 1000;
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (strcmp(sunshine_table[i].target, target) == 0 && sunshine_table[i].host[0]) {
            strncpy(sunshine_table[i].ip, ip, sizeof(sunshine_table[i].ip));
            sunshine_table[i].last_seen = now;
            break;
        }
    }
}

typedef void (*moonlight_found_cb)(int idx, const char* host, const char* pcname, const char* ip, int port);
static moonlight_found_cb g_found_cb = NULL;

void udp_sniffer_vita_set_callback(moonlight_found_cb cb) {
    g_found_cb = cb;
}

// Fusionar IPs huérfanas (opcional, igual que en win)
static void merge_ip_entries() {
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (sunshine_table[i].host[0] && sunshine_table[i].target[0]) {
            for (int j = 0; j < MAX_SUNSHINE; ++j) {
                if (i != j && !sunshine_table[j].host[0] && sunshine_table[j].target[0] && sunshine_table[j].ip[0] && strcmp(sunshine_table[i].target, sunshine_table[j].target) == 0) {
                    strncpy(sunshine_table[i].ip, sunshine_table[j].ip, sizeof(sunshine_table[i].ip));
                    sunshine_table[j].target[0] = '\0';
                    sunshine_table[j].ip[0] = '\0';
                    sunshine_table[j].last_seen = 0;
                    break;
                }
            }
        }
    }
}

static void check_and_print_ready_entries_vita() {
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (sunshine_table[i].host[0] && sunshine_table[i].target[0] && sunshine_table[i].ip[0] && sunshine_table[i].port > 0) {
            moonlight_count++;
            if (g_found_cb) {
                g_found_cb(moonlight_count, sunshine_table[i].host, sunshine_table[i].target, sunshine_table[i].ip, sunshine_table[i].port);
            }
            sunshine_table[i].host[0] = '\0';
            sunshine_table[i].ip[0] = '\0';
            sunshine_table[i].port = 0;
            sunshine_table[i].target[0] = '\0';
        }
    }
}

void udp_sniffer_vita_poll(void) {
    if (!initialized) {
        // Inicialización perezosa
        sock = sceNetSocket("mdns", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
        if (sock < 0) { MDNS_LOG("[VITA] sceNetSocket() failed\n"); return; }
        SceNetSockaddrIn addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = SCE_NET_AF_INET;
        addr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
        addr.sin_port = sceNetHtons(MCAST_PORT);
        if (sceNetBind(sock, (SceNetSockaddr*)&addr, sizeof(addr)) < 0) {
            MDNS_LOG("[VITA] sceNetBind() failed\n");
            sceNetSocketClose(sock); sock = -1; return;
        }
        struct SceNetIpMreq mreq;
        // Reemplazo de sceNetInetAddr por sceNetInetPton
        sceNetInetPton(SCE_NET_AF_INET, MCAST_ADDR, &mreq.imr_multiaddr.s_addr);
        mreq.imr_interface.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
        if (sceNetSetsockopt(sock, SCE_NET_IPPROTO_IP, SCE_NET_IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            MDNS_LOG("[VITA] sceNetSetsockopt(IP_ADD_MEMBERSHIP) failed\n");
            sceNetSocketClose(sock); sock = -1; return;
        }
        // Enviar consulta mDNS activa
        unsigned char query[] = {
            0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
            9,'_','n','v','s','t','r','e','a','m',
            4,'_','t','c','p',
            5,'l','o','c','a','l',0x00,
            0x00,0x0C,0x00,0x01
        };
        SceNetSockaddrIn mcast_addr;
        memset(&mcast_addr, 0, sizeof(mcast_addr));
        mcast_addr.sin_family = SCE_NET_AF_INET;
        sceNetInetPton(SCE_NET_AF_INET, MCAST_ADDR, &mcast_addr.sin_addr.s_addr);
        mcast_addr.sin_port = sceNetHtons(MCAST_PORT);
        sceNetSendto(sock, query, sizeof(query), 0, (SceNetSockaddr*)&mcast_addr, sizeof(mcast_addr));
        memset(sunshine_table, 0, sizeof(sunshine_table));
        initialized = 1;
        MDNS_LOG("[VITA] Sniffer UDP mDNS inicializado.\n");
        psvDebugScreenPrintf("[VITA] Sniffer UDP mDNS inicializado.\n");
    }
    if (sock < 0) return;
    char buffer[BUF_SIZE];
    SceNetSockaddrIn from;
    int fromlen = sizeof(from);
    int n = sceNetRecvfrom(sock, buffer, sizeof(buffer), SCE_NET_MSG_DONTWAIT, (SceNetSockaddr*)&from, &fromlen);
    if (n <= 0) return; // No hay datos
    const unsigned char* pkt = (const unsigned char*)buffer;
    int msglen = n;
    int qdcount = 0, ancount = 0;
    if (msglen > 12) {
        qdcount = (pkt[4]<<8) | pkt[5];
        ancount = (pkt[6]<<8) | pkt[7];
        int nscount = (pkt[8]<<8) | pkt[9];
        int arcount = (pkt[10]<<8) | pkt[11];
        int pos = 12;
        for (int q = 0; q < qdcount && pos < msglen; ++q) {
            char qname[256];
            int l = decode_dns_name(pkt, msglen, pos, qname, sizeof(qname));
            pos += l + 4;
        }
        int total_rr = ancount + nscount + arcount;
        for (int i = 0; i < total_rr && pos < msglen; ++i) {
            int rr_start = pos;
            char rrname[256];
            int l = decode_dns_name(pkt, msglen, pos, rrname, sizeof(rrname));
            pos += l;
            if (pos+10 > msglen) break;
            int type = (pkt[pos]<<8) | pkt[pos+1];
            int rdlength = (pkt[pos+8]<<8) | pkt[pos+9];
            pos += 10;
            if (type == 12) { // PTR
                char ptrname[256];
                decode_dns_name(pkt, msglen, pos, ptrname, sizeof(ptrname));
                if (strstr(ptrname, "_nvstream._tcp.local") != NULL) {
                    update_sunshine(ptrname, 0, NULL, NULL);
                }
            }
            if (type == 33 && rdlength >= 6) { // SRV
                int srv_port = (pkt[pos+4]<<8) | pkt[pos+5];
                char target[256] = {0};
                decode_dns_name(pkt, msglen, pos+6, target, sizeof(target));
                size_t len = strlen(rrname);
                const char* suffix = "._nvstream._tcp.local";
                size_t suffixlen = strlen(suffix);
                if (len > suffixlen && strcmp(rrname + len - suffixlen, suffix) == 0) {
                    if (target[0]) {
                        update_sunshine(rrname, srv_port, NULL, target);
                    }
                }
            }
            if (type == 1 && rdlength == 4) { // A
                char ip[16];
                snprintf(ip, sizeof(ip), "%u.%u.%u.%u", pkt[pos], pkt[pos+1], pkt[pos+2], pkt[pos+3]);
                update_sunshine_ip_by_target(rrname, ip);
            }
            pos = rr_start + l + 10 + rdlength;
        }
        merge_ip_entries();
        check_and_print_ready_entries_vita();
    }
}

void udp_sniffer_vita_deinit(void) {
    if (sock >= 0) {
        sceNetSocketClose(sock);
        sock = -1;
    }
    initialized = 0;
    moonlight_count = 0;
    memset(sunshine_table, 0, sizeof(sunshine_table));
}

void udp_sniffer_vita_init(void) {
    // No hace nada, inicialización perezosa en poll
    initialized = 0;
    moonlight_count = 0;
    memset(sunshine_table, 0, sizeof(sunshine_table));
}
#else
void udp_sniffer_vita_poll(void) {}
#endif
