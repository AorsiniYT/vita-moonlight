
#ifndef GET_MAC_H
#define GET_MAC_H

#include <stddef.h>
#include "device.h"

// Obtiene la MAC del host Sunshine usando certificados y uniqueid (requiere pairing previo)
// info: puntero a device_info_t con nombre y dirección IP
// mac_out: buffer de salida para la MAC (mínimo 18 bytes)
// errbuf: buffer para mensaje de error (opcional)
// errlen: tamaño de errbuf
// curl_code: código de error (opcional)
int get_mac_from_device_vita_verbose(const device_info_t *info, char *mac_out, char *errbuf, size_t errlen, long *curl_code);

#endif // GET_MAC_H
