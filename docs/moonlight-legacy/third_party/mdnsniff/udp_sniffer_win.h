/*
 * Sniffer UDP mDNS modular para Windows
 * Detección de servicios Moonlight/Sunshine (GameStream) en red local.
 *
 * Autor: AorsiniYT
 * Copyright 2025
 */

#ifndef UDP_SNIFFER_WIN_H
#define UDP_SNIFFER_WIN_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback para detección de host Moonlight/Sunshine.
 * @param idx   Número de detección (1, 2, ...)
 * @param host  Nombre de host mDNS (ej: "DESKTOP-XXXXXX._nvstream._tcp.local")
 * @param pcname Nombre de la máquina/PC (target SRV, ej: "DESKTOP-XXXXXX.local")
 * @param ip    Dirección IP detectada (ej: "192.168.1.100")
 * @param port  Puerto TCP del servicio GameStream
 */
typedef void (*moonlight_found_cb_win)(int idx, const char* host, const char* pcname, const char* ip, int port);

/**
 * Registra la función callback que será llamada al detectar un host Moonlight/Sunshine.
 */
void udp_sniffer_win_set_callback(moonlight_found_cb_win cb);

/**
 * Inicializa el sniffer mDNS (abre socket, envía consulta, etc).
 * Llamar una vez antes de usar.
 * Devuelve 0 si ok, <0 si error.
 */
int udp_sniffer_win_init(void);

/**
 * Procesa un paquete mDNS si hay disponible (no bloqueante).
 * Llamar periódicamente desde el bucle principal.
 */
void udp_sniffer_win_poll(void);

/**
 * Libera recursos del sniffer (cierra socket, etc).
 */
void udp_sniffer_win_deinit(void);

#ifdef __cplusplus
}
#endif
#endif // UDP_SNIFFER_WIN_H
