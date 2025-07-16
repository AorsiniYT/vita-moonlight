
#include <curl/curl.h>
#include "device.h"
#include "config.h"
#include "../libgamestream/xml.h"
#include "../libgamestream/client.h"

static int extract_mac_from_json(const char *json, char *mac_out) {
    const char *mac_key = "\"root.mac\"\s*:\s*\"";
    const char *p = strstr(json, "\"root.mac\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\"' || *p == ':') p++;
    if (*p == '\"') p++;
    int n = 0;
    while (*p && *p != '\"' && n < 17) {
        mac_out[n++] = *p++;
    }
    mac_out[n] = '\0';
    return (n == 17);
}
int get_mac_from_device_vita_verbose(const device_info_t *info, char *mac_out, char *errbuf, size_t errlen, long *curl_code) {
    SERVER_DATA server;
    memset(&server, 0, sizeof(server));
    char host_key_dir[512];
    snprintf(host_key_dir, sizeof(host_key_dir), "%s/%s", config.key_dir, info->name);
    int ret = gs_init(&server, (char*)info->internal, 47989, host_key_dir, 0, false);
    if (ret == 0 && server.serverInfo.mac[0]) {
        strncpy(mac_out, server.serverInfo.mac, 17);
        mac_out[17] = '\0';
        if (curl_code) *curl_code = 200;
        return 1;
    } else {
        if (errbuf && errlen > 0) {
            if (ret != 0) snprintf(errbuf, errlen, "gs_init error %d", ret);
            else snprintf(errbuf, errlen, "No se encontró MAC en serverinfo");
        }
        if (curl_code) *curl_code = -1;
        return 0;
    }
}
