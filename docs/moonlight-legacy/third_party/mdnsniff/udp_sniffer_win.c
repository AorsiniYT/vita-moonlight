/*
 * Sniffer UDP mDNS modular para Windows
 * Detección de servicios Moonlight/Sunshine (GameStream) en red local.
 *
 * Autor: AorsiniYT
 * Copyright 2025
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "udp_sniffer_win.h"
#pragma comment(lib, "ws2_32.lib")

#define MAX_SUNSHINE 16
#define HOSTNAME_LEN 256
#define CACHE_TIMEOUT 10 // segundos

struct sunshine_entry {
    char host[HOSTNAME_LEN];
    int port;
    time_t last_seen;
    char ip[INET_ADDRSTRLEN];
    char target[HOSTNAME_LEN];
};

static struct sunshine_entry sunshine_table[MAX_SUNSHINE];
static SOCKET sock = INVALID_SOCKET;
static int initialized = 0;
static int moonlight_count = 0;
static moonlight_found_cb_win g_found_cb = NULL;

void udp_sniffer_win_set_callback(moonlight_found_cb_win cb) {
    g_found_cb = cb;
}

int udp_sniffer_win_init(void) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return -1;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return -2;
    // Hacer el socket no bloqueante
    u_long nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(5353);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock); sock = INVALID_SOCKET; return -3;
    }
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    // Enviar consulta mDNS para forzar respuesta de Sunshine
    unsigned char query[] = {
        0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
        9,'_','n','v','s','t','r','e','a','m',
        4,'_','t','c','p',
        5,'l','o','c','a','l',0x00,
        0x00,0x0C,0x00,0x01
    };
    struct sockaddr_in mcast_addr;
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_addr.s_addr = inet_addr("224.0.0.251");
    mcast_addr.sin_port = htons(5353);
    sendto(sock, (const char*)query, sizeof(query), 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));
    memset(sunshine_table, 0, sizeof(sunshine_table));
    initialized = 1;
    moonlight_count = 0;
    return 0;
}

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

static void update_sunshine(const char* host, int port, const char* ip, const char* target) {
    time_t now = time(NULL);
    int idx = -1;
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (strcmp(sunshine_table[i].host, host) == 0) { idx = i; break; }
    }
    if (idx == -1) {
        time_t oldest = now;
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
            memset(sunshine_table[idx].target, 0, sizeof(sunshine_table[idx].target));
            strncpy(sunshine_table[idx].target, target, sizeof(sunshine_table[idx].target) - 1);
            sunshine_table[idx].target[sizeof(sunshine_table[idx].target) - 1] = '\0';
            for (int j = 0; j < MAX_SUNSHINE; ++j) {
                if (j != idx && strcmp(sunshine_table[j].target, target) == 0 && sunshine_table[j].ip[0]) {
                    strncpy(sunshine_table[idx].ip, sunshine_table[j].ip, sizeof(sunshine_table[idx].ip));
                    break;
                }
            }
        }
        sunshine_table[idx].last_seen = now;
    }
}

static void update_sunshine_ip_by_target(const char* target, const char* ip) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SUNSHINE; ++i) {
        if (strcmp(sunshine_table[i].target, target) == 0 && sunshine_table[i].host[0]) {
            strncpy(sunshine_table[i].ip, ip, sizeof(sunshine_table[i].ip));
            sunshine_table[i].last_seen = now;
            break;
        }
    }
}

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

static void check_and_print_ready_entries_win() {
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

void udp_sniffer_win_poll(void) {
    if (!initialized || sock == INVALID_SOCKET) return;
    char buffer[2048];
    struct sockaddr_in from;
    int fromlen = sizeof(from);
    int n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromlen);
    if (n <= 0) return;
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
                char ip[INET_ADDRSTRLEN];
                snprintf(ip, sizeof(ip), "%u.%u.%u.%u", pkt[pos], pkt[pos+1], pkt[pos+2], pkt[pos+3]);
                update_sunshine_ip_by_target(rrname, ip);
            }
            pos = rr_start + l + 10 + rdlength;
        }
        merge_ip_entries();
        check_and_print_ready_entries_win();
    }
}

void udp_sniffer_win_deinit(void) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
    initialized = 0;
}

// Ejemplo de uso
#ifdef UDP_SNIFFER_WIN_MAIN
void moonlight_found_callback_win(int idx, const char* host, const char* pcname, const char* ip, int port) {
    printf("[Moonlight #%d] Host: %s | Nombre PC: %s | IP: %s | Puerto: %d\n", idx, host, pcname, ip, port);
}

int main() {
    printf("Consulta mDNS enviada para _nvstream._tcp.local.\n");
    printf("Escuchando solo servicios Moonlight/Sunshine (_nvstream._tcp.local)...\n");
    udp_sniffer_win_set_callback(moonlight_found_callback_win);
    if (udp_sniffer_win_init() < 0) {
        printf("Error inicializando el sniffer mDNS.\n");
        return 1;
    }
    while (1) {
        udp_sniffer_win_poll();
        Sleep(10);
    }
    udp_sniffer_win_deinit();
    return 0;
}
#endif

#endif // _WIN32
