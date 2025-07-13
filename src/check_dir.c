#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "config.h"
#include "check_dir.h"

// Intenta crear la carpeta en varias rutas y retorna la ruta exitosa en out_path
// Si hay errores, los registra en ux0:data/moonlight.log
// out_path y out_key_dir deben tener espacio suficiente (>=MOONLIGHT_PATH_MAX)
void check_and_create_moonlight_dir(char* out_path, char* out_key_dir) {
    const char* test_paths[] = {
        "ux0:data/moonlight",
        "ux0:moonlight",
        "uma0:data/moonlight"
    };
    int path_count = sizeof(test_paths)/sizeof(test_paths[0]);
    int success_idx = -1;
    int mkdir_result = 0;
    FILE* test_log = NULL;
    for (int i = 0; i < path_count; i++) {
        mkdir_result = sceIoMkdir(test_paths[i], 0777);
        if (mkdir_result >= 0 || mkdir_result == 0x80010011) { // 0x80010011 = ya existe
            success_idx = i;
            vita_debug_log("[Moonlight] Carpeta creada/existente: %s", test_paths[i]);
            break;
        } else {
            if (!test_log) test_log = fopen("ux0:data/moonlight.log", "w");
            if (test_log) fprintf(test_log, "[Moonlight] Error creando carpeta %s, código: 0x%08x\n", test_paths[i], (unsigned int)mkdir_result);
            vita_debug_log("[Moonlight] Error creando carpeta %s, código: 0x%08x", test_paths[i], (unsigned int)mkdir_result);
        }
    }
    if (success_idx >= 0) {
        if (test_log) fprintf(test_log, "[Moonlight] Carpeta creada/existente: %s\n", test_paths[success_idx]);
        snprintf(out_key_dir, MOONLIGHT_PATH_MAX, "%s/", test_paths[success_idx]);
        out_key_dir[MOONLIGHT_PATH_MAX-1] = '\0';
        snprintf(out_path, MOONLIGHT_PATH_MAX, "%s/moonlight.conf", test_paths[success_idx]);
        out_path[MOONLIGHT_PATH_MAX-1] = '\0';
    } else {
        snprintf(out_key_dir, MOONLIGHT_PATH_MAX, "ux0:data/");
        out_key_dir[MOONLIGHT_PATH_MAX-1] = '\0';
        snprintf(out_path, MOONLIGHT_PATH_MAX, "ux0:data/moonlight.conf");
        out_path[MOONLIGHT_PATH_MAX-1] = '\0';
        if (!test_log) test_log = fopen("ux0:data/moonlight.log", "w");
        if (test_log) fprintf(test_log, "[Moonlight] Ninguna carpeta pudo ser creada, usando ux0:data como fallback\n");
    }
    // Crear el archivo moonlight.conf si no existe
    FILE* conf_file = fopen(out_path, "a");
    if (conf_file) fclose(conf_file);
    if (test_log) fclose(test_log);

    // Guardar solo key_dir en moonlight.conf
    FILE* conf_save = fopen(out_path, "r+");
    if (conf_save) {
        // Buscar si ya existe la línea key_dir
        char line[512];
        int found = 0;
        while (fgets(line, sizeof(line), conf_save)) {
            if (strncmp(line, "key_dir = ", 10) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fseek(conf_save, 0, SEEK_END);
            fprintf(conf_save, "key_dir = %s\n", out_key_dir);
        }
        fclose(conf_save);
    }
}
