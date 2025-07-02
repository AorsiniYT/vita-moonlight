/**
 * @file udp_sniffer_vita.h
 * @brief Sniffer UDP mDNS para PSVita (VitaSDK) - API de integración
 *
 * Este archivo define la API para integrar el sniffer mDNS de Moonlight/Sunshine en aplicaciones PSVita.
 * El sniffer detecta servicios GameStream (Moonlight/Sunshine) en la red local y notifica detecciones mediante callback.
 *
 * Uso típico:
 *   1. Llama a udp_sniffer_vita_set_callback() con tu función para recibir detecciones.
 *   2. Llama periódicamente a udp_sniffer_vita_poll() en tu bucle principal.
 *
 * La función de callback se invoca solo cuando se detecta un host Moonlight/Sunshine completo (host, nombre PC, IP y puerto).
 *
 * @author AorsiniYT
 * @copyright 2025
 */

#ifndef UDP_SNIFFER_VITA_H
#define UDP_SNIFFER_VITA_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tipo de callback para detección de host Moonlight/Sunshine.
 * @param idx   Número de detección (1, 2, ...)
 * @param host  Nombre de host mDNS (ej: "DESKTOP-XXXXXX._nvstream._tcp.local")
 * @param pcname Nombre de la máquina/PC (target SRV, ej: "DESKTOP-XXXXXX.local")
 * @param ip    Dirección IP detectada (ej: "192.168.1.100")
 * @param port  Puerto TCP del servicio GameStream
 */
typedef void (*moonlight_found_cb)(int idx, const char* host, const char* pcname, const char* ip, int port);

/**
 * Registra la función callback que será llamada al detectar un host Moonlight/Sunshine.
 * Solo se invoca cuando todos los datos (host, pcname, ip, puerto) están presentes.
 * @param cb Función de callback definida por el usuario.
 */
void udp_sniffer_vita_set_callback(moonlight_found_cb cb);

/**
 * Procesa un paquete mDNS si hay disponible (no bloqueante).
 * Llamar periódicamente desde el bucle principal.
 */
void udp_sniffer_vita_poll(void);

/**
 * Cierra el sniffer UDP y libera recursos. Debe llamarse antes de reiniciar una búsqueda.
 */
void udp_sniffer_vita_deinit(void);

/**
 * Reinicia el estado interno del sniffer (opcional, para simetría con deinit).
 */
void udp_sniffer_vita_init(void);

#ifdef __cplusplus
}
#endif

#endif // UDP_SNIFFER_VITA_H
