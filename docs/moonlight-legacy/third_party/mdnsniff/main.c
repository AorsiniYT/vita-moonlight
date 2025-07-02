/*
 * Ejemplo de integración del sniffer UDP mDNS para PSVita/Windows
 *
 * Autor: AorsiniYT
 * Copyright 2025
 */

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h> 
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/ctrl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "udp_sniffer_vita.h"
#define MDNS_LOG sceClibPrintf
#include "common/debugScreen.h"

#define NET_PARAM_MEM_SIZE (256 * 1024)

void moonlight_found_callback(int idx, const char* host, const char* pcname, const char* ip, int port) {
    psvDebugScreenPrintf("[Moonlight #%d] Host: %s | Nombre PC: %s | IP: %s | Puerto: %d\n",
        idx, host, pcname, ip, port);
    MDNS_LOG("[Moonlight #%d] Host: %s | Nombre PC: %s | IP: %s | Puerto: %d\n",
        idx, host, pcname, ip, port);
}

int main(int argc, char *argv[]) {
    psvDebugScreenInit();
    MDNS_LOG("[USB] Test de red y sniffer UDP mDNS iniciado.\n");
    psvDebugScreenPrintf("Iniciando test de red y sniffer UDP mDNS en Vita...\n");
    psvDebugScreenPrintf("\nIMPORTANTE: Para que aparezcan servidores, debe estar activo GeForce Experience o Sunshine en la red local.\n\n");
    MDNS_LOG("IMPORTANTE: Para que aparezcan servidores, debe estar activo GeForce Experience o Sunshine en la red local.\n");

    int ret;
    void *net_mem = NULL;

    psvDebugScreenPrintf("Cargando modulo de red (SCE_SYSMODULE_NET)...\n");
    ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if (ret < 0) {
        psvDebugScreenPrintf("ERROR: sceSysmoduleLoadModule fallo: 0x%08X\n", ret);
        MDNS_LOG("ERROR: sceSysmoduleLoadModule fallo: 0x%08X\n", ret);
        goto end;
    }

    psvDebugScreenPrintf("Inicializando red (sceNetInit)...\n");
    SceNetInitParam netInitParam;
    net_mem = malloc(NET_PARAM_MEM_SIZE);
    if (!net_mem) {
        psvDebugScreenPrintf("ERROR: malloc para net_mem fallo\n");
        MDNS_LOG("ERROR: malloc para net_mem fallo\n");
        goto end;
    }
    netInitParam.memory = net_mem;
    netInitParam.size = NET_PARAM_MEM_SIZE;
    netInitParam.flags = 0;
    ret = sceNetInit(&netInitParam);
    if (ret < 0) {
        psvDebugScreenPrintf("ERROR: sceNetInit fallo: 0x%08X\n", ret);
        MDNS_LOG("ERROR: sceNetInit fallo: 0x%08X\n", ret);
        goto end;
    }

    psvDebugScreenPrintf("Inicializando control de red (sceNetCtlInit)...\n");
    ret = sceNetCtlInit();
    if (ret < 0) {
        psvDebugScreenPrintf("ERROR: sceNetCtlInit fallo: 0x%08X\n", ret);
        MDNS_LOG("ERROR: sceNetCtlInit fallo: 0x%08X\n", ret);
        goto end;
    }

    // Obtener información de la red
    SceNetCtlInfo info;
    ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
    if (ret < 0) {
        psvDebugScreenPrintf("ERROR: sceNetCtlInetGetInfo fallo: 0x%08X\n", ret);
        MDNS_LOG("ERROR: sceNetCtlInetGetInfo fallo: 0x%08X\n", ret);
        goto end;
    }

    psvDebugScreenPrintf("IP: %s\n", info.ip_address);
    MDNS_LOG("IP: %s\n", info.ip_address);

    psvDebugScreenPrintf("\nSniffer UDP mDNS activo. Observa la consola para logs de servicios Moonlight/Sunshine.\n");
    MDNS_LOG("Sniffer UDP mDNS activo. Observa la consola para logs de servicios Moonlight/Sunshine.\n");
    psvDebugScreenPrintf("Test finalizado. Presiona el boton PS para salir.\n");
    MDNS_LOG("Test finalizado. El usuario debe cerrar la app con el boton PS.\n");

    // Bucle principal: solo sniffer UDP
    udp_sniffer_vita_set_callback(moonlight_found_callback);
    while (1) {
        udp_sniffer_vita_poll(); // Procesa un paquete mDNS si hay
        sceKernelDelayThread(1000 * 10); // 10ms para no saturar CPU
    }

end:
    if (net_mem) {
        free(net_mem);
    }
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    return 0;
}
#else // Windows/otros
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "mdns.h"
#define MDNS_LOG printf

// Stubs para funciones Vita
#define psvDebugScreenInit() ((void)0)
#define psvDebugScreenPrintf printf
#define sceKernelDelayThread(ms) Sleep((ms)/1000)

// Estructura para almacenar la información del servidor encontrado
typedef struct {
    char instance[256];
    char host[256];
    uint16_t port;
    char ip_v4[16];
    int has_ptr;
    int has_srv;
    int has_a;
} ServerInfo;

ServerInfo servers[8];
static ServerInfo* find_or_create_server_by_instance(const char* instance) {
    for (int i = 0; i < 8; ++i) {
        if (servers[i].has_ptr && strcmp(servers[i].instance, instance) == 0) {
            return &servers[i];
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (!servers[i].has_ptr) {
            strncpy(servers[i].instance, instance, sizeof(servers[i].instance) - 1);
            servers[i].has_ptr = 1;
            return &servers[i];
        }
    }
    return NULL;
}
static ServerInfo* find_server_by_host(const char* host) {
    for (int i = 0; i < 8; ++i) {
        if (servers[i].has_srv && strcmp(servers[i].host, host) == 0) {
            return &servers[i];
        }
    }
    return NULL;
}
static void clear_servers() { memset(servers, 0, sizeof(servers)); }

// Declaraciones de funciones necesarias de mdns.c
int mdns_socket_open_ipv4(const struct sockaddr_in* saddr);
void mdns_socket_close(int sock);
int mdns_discovery_send(int sock);
size_t mdns_discovery_recv(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data);
mdns_string_t mdns_record_parse_ptr(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer, size_t capacity);
mdns_record_srv_t mdns_record_parse_srv(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer, size_t capacity);
struct sockaddr_in* mdns_record_parse_a(const void* buffer, size_t size, size_t offset, size_t length, struct sockaddr_in* addr);

// Callback para procesar TODAS las respuestas mDNS
int mdns_callback(int sock, const struct sockaddr* from, size_t addrlen,
                  mdns_entry_type_t entry, uint16_t query_id,
                  uint16_t rtype, uint16_t rclass, uint32_t ttl,
                  const void* data, size_t size,
                  size_t name_offset, size_t name_length,
                  size_t record_offset, size_t record_length,
                  void* user_data) {
    (void)sock; (void)addrlen; (void)query_id; (void)rclass; (void)ttl; (void)data; (void)size; (void)user_data;
    char from_addr_buffer[64] = {0};
    const struct sockaddr_in* sa = (const struct sockaddr_in*)from;
    inet_ntop(AF_INET, &sa->sin_addr, from_addr_buffer, sizeof(from_addr_buffer));
    mdns_string_t from_addr_str = { from_addr_buffer, strlen(from_addr_buffer) };
    const char* entry_type = (entry == MDNS_ENTRYTYPE_ANSWER) ? "ANSWER" : (entry == MDNS_ENTRYTYPE_AUTHORITY) ? "AUTHORITY" : "ADDITIONAL";
    printf("[mDNS] [%s] from %s | rtype: %u\n", entry_type, from_addr_str.str, rtype);
    // Imprimir nombre del registro
    printf("  Nombre: %.*s\n", (int)name_length, (const char*)MDNS_POINTER_OFFSET(data, name_offset));
    // Imprimir datos según tipo
    if (rtype == MDNS_RECORDTYPE_PTR) {
        char name_buffer[256];
        mdns_string_t name_str = mdns_record_parse_ptr(data, size, record_offset, record_length, name_buffer, sizeof(name_buffer));
        printf("    PTR: %.*s\n", (int)name_str.length, name_str.str);
    } else if (rtype == MDNS_RECORDTYPE_SRV) {
        char srv_name_buffer[256];
        mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length, srv_name_buffer, sizeof(srv_name_buffer));
        printf("    SRV: %.*s port %d\n", (int)name_length, (const char*)MDNS_POINTER_OFFSET(data, name_offset), srv.port);
    } else if (rtype == MDNS_RECORDTYPE_A) {
        struct sockaddr_in addr;
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);
        char a_addr_buffer[64];
        inet_ntop(AF_INET, &addr.sin_addr, a_addr_buffer, sizeof(a_addr_buffer));
        printf("    A: %s\n", a_addr_buffer);
    } else if (rtype == MDNS_RECORDTYPE_TXT) {
        const uint8_t* txt = (const uint8_t*)MDNS_POINTER_OFFSET(data, record_offset);
        size_t txt_len = record_length;
        printf("    TXT: ");
        for (size_t i = 0; i < txt_len; ++i) {
            char c = txt[i];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
    // Otros tipos también se imprimen
    return 0;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    printf("[WIN] Test de red y mDNS iniciado.\n");
    printf("Iniciando test de red y mDNS en Windows...\n");
    printf("IMPORTANTE: Para que aparezcan servidores, debe estar activo GeForce Experience o Sunshine en la red local.\n\n");

    // Mostrar IPs locales antes de iniciar mDNS
    printf("IPs locales detectadas en este equipo:\n");
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {0}, *res, *p;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
            for (p = res; p != NULL; p = p->ai_next) {
                struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr->sin_addr, ipstr, sizeof(ipstr));
                printf("  %s\n", ipstr);
            }
            freeaddrinfo(res);
        } else {
            printf("  (No se pudo obtener la IP local)\n");
        }
    } else {
        printf("  (No se pudo obtener el hostname local)\n");
    }
    printf("---------------------------------\n");

    clear_servers();
#ifdef _WIN32
    // --- Apertura de socket mDNS en Windows con configuración multicast correcta ---
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("ERROR: No se pudo crear el socket mDNS\n");
        WSACleanup();
        return 1;
    }
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        printf("[WARN] No se pudo activar SO_REUSEADDR\n");
    }
    struct sockaddr_in addr_bind;
    memset(&addr_bind, 0, sizeof(addr_bind));
    addr_bind.sin_family = AF_INET;
    addr_bind.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_bind.sin_port = htons(5353);
    if (bind(sock, (struct sockaddr*)&addr_bind, sizeof(addr_bind)) < 0) {
        printf("ERROR: No se pudo hacer bind a INADDR_ANY:5353\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        printf("[WARN] No se pudo unir al grupo multicast mDNS (224.0.0.251)\n");
    } else {
        printf("[OK] Unido al grupo multicast mDNS (224.0.0.251)\n");
    }
#else
    int sock = mdns_socket_open_ipv4(NULL);
    if (sock < 0) {
        printf("ERROR: No se pudo abrir el socket mDNS: %d\n", sock);
        WSACleanup();
        return 1;
    }
#endif

    printf("Enviando consulta mDNS para: _services._dns-sd._udp.local.\n");
    // mdns_discovery_send_service(sock, "_nvstream._tcp.local.");
    printf("Esperando respuestas mDNS (10 segundos)...\n");
    char recv_buffer[2048];
    for (int i = 0; i < 200; ++i) {
        int processed;
        do {
            processed = mdns_discovery_recv(sock, recv_buffer, sizeof(recv_buffer), mdns_callback, NULL);
        } while (processed > 0);
        Sleep(50);
    }
    printf("\n--- Descubrimiento Finalizado ---\n");
    int found = 0;
    for (int i = 0; i < 8; ++i) {
        if (servers[i].has_ptr && servers[i].has_srv && servers[i].has_a) {
            printf("Servidor Encontrado:\n");
            printf("  Instancia: %s\n", servers[i].instance);
            printf("  Host:      %s\n", servers[i].host);
            printf("  IP:        %s\n", servers[i].ip_v4);
            printf("  Port:      %d\n", servers[i].port);
            found = 1;
        }
    }
    if (!found) {
        printf("No se encontraron servidores GameStream.\n");
    }
    printf("---------------------------------\n\n");
    found = 0;
    for (int i = 0; i < 8; ++i) {
        if (servers[i].has_a) {
            printf("Host con IP detectado:\n");
            if (servers[i].instance[0])
                printf("  Instancia: %s\n", servers[i].instance);
            if (servers[i].host[0])
                printf("  Host:      %s\n", servers[i].host);
            printf("  IP:        %s\n", servers[i].ip_v4);
            if (servers[i].port)
                printf("  Port:      %d\n", servers[i].port);
            found = 1;
        }
    }
    if (!found) {
        printf("No se encontraron hosts con IP por mDNS.\n");
    }
    printf("---------------------------------\n\n");
    mdns_socket_close(sock);
    printf("Socket mDNS cerrado.\n");
    printf("Test finalizado.\n");
    WSACleanup();
#ifdef _WIN32
    system("pause");
#endif
    return 0;
}
#endif
