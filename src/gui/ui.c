#include "ui.h"

#include "guilib.h"
#include "ime.h"

#include "ui_settings.h"
#include "ui_connect.h"
#include "ui_device.h"
#include "ui_check.h"

#include "../config.h"
#include "../device.h"
#include "../connection.h"
#include "../video/vita.h"
#include "../input/vita.h"
#include "../power/vita.h"
#include "../util.h"
#include "../check_host.h"
#include "../debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>

extern SceNetInitParam net_param;

enum {
  MAIN_MENU_CONNECTED = 100,
  MAIN_MENU_SEARCH,
  MAIN_MENU_CONNECT_RESUME,
  MAIN_MENU_SETTINGS,
  MAIN_MENU_CONNECT,
  MAIN_MENU_CONNECT_PAIRED,
  MAIN_MENU_QUIT = 999,
};

// Variables globales para actualización pendiente de IP
int pending_ip_update_idx = -1;
char pending_ip_update[64] = "";
int first_scan_pending = 0;

// Prototipos para el control del escaneo de hosts
#define start_host_scan() start_host_scan_thread()
#define stop_host_scan() stop_host_scan_thread()

int ui_main_menu_loop(int cursor, void *context, const input_data *input) {
  // menu_entry *menu = (menu_entry*)context; // Variable no usada
  extern volatile int g_host_status_changed;
  extern volatile int g_host_scan_thread_status;
  // Refresco manual con Triángulo
  if ((input->buttons & SCE_CTRL_TRIANGLE) != 0) {
    vita_debug_log("[UI] Refresco manual solicitado (Triángulo): reiniciando escaneo de hosts");
    stop_host_scan();
    start_host_scan();
    g_host_status_changed = 0; // Limpiar flag para evitar refresco doble
    first_scan_pending = 0;
    return 2; // Forzar refresco del menú
  }
  // Refresco automático solo una vez tras el primer escaneo
  if (first_scan_pending && g_host_status_changed && g_host_scan_thread_status != 1) {
    vita_debug_log("[UI] Refresco automático tras primer escaneo\n");
    g_host_status_changed = 0;
    first_scan_pending = 0;
    return 2;
  }
  // Solo refrescar por cambio de estado si el hilo está activo
  if (g_host_status_changed && g_host_scan_thread_status == 1) {
    vita_debug_log("[UI] Refrescando menú por cambio de estado de host\n");
    g_host_status_changed = 0;
    return 2; // Forzar refresco del menú
  }

  if ((input->buttons & config.btn_confirm) == 0 || (input->buttons & SCE_CTRL_HOLD) != 0) {
    return 0;
  }
  // Esperar a que el hilo de escaneo esté activo antes de permitir selección de hosts
  if (cursor >= MAIN_MENU_CONNECT_PAIRED && cursor < MAIN_MENU_QUIT) {
    int host_idx = cursor - MAIN_MENU_CONNECT_PAIRED;
    // Mostrar diálogo de IP pendiente ANTES de chequear el estado del hilo
    extern int pending_ip_update_idx;
    extern char pending_ip_update[64];
    if (pending_ip_update_idx == host_idx && pending_ip_update[0] != '\0') {
      device_info_t *info = &known_devices.devices[host_idx];
      vita_debug_log("[UI] Mostrando diálogo de actualización de IP pendiente para %s: %s", info->name, pending_ip_update);
      ui_check_ip_update(info, pending_ip_update); // Variable 'res' no usada
      // Tras recargar, buscar el puntero actualizado y loguear si no se encuentra
      device_info_t *updated = find_device(info->name);
      if (updated) {
        info = updated;
      } else {
        vita_debug_log("[UI] ERROR: Host %s no encontrado tras recargar dispositivos", info->name);
      }
      return 2; // Forzar refresco del menú tras el diálogo
    }
    struct host_status st = g_host_status[host_idx];
    vita_debug_log("[UI] Intentando seleccionar host_idx=%d, hilo_estado=%d, st.current_ip=%s", host_idx, g_host_scan_thread_status, st.current_ip);
    // Si el hilo no está activo o el estado aún no fue actualizado, ignorar input
    if (g_host_scan_thread_status != 1 || st.current_ip[0] == '\0') {
      vita_debug_log("[UI] Esperando escaneo: hilo_estado=%d, st.current_ip='%s'", g_host_scan_thread_status, st.current_ip);
      flash_message("Buscando estado del host...");
      return 0;
    }
  }

  // Al seleccionar cualquier opción que cambie de menú, detener el escaneo de hosts
  int exit_menu = 0;
  if (cursor >= MAIN_MENU_CONNECT_PAIRED && cursor < MAIN_MENU_QUIT) {
    device_info_t *info = &known_devices.devices[cursor - MAIN_MENU_CONNECT_PAIRED];
    int host_idx = cursor - MAIN_MENU_CONNECT_PAIRED;
    struct host_status st;
    st = g_host_status[host_idx];
    vita_debug_log("[UI] (LOOP) host_idx=%d, hilo_estado=%d, st.status=%d, st.current_ip=%s", host_idx, g_host_scan_thread_status, st.status, st.current_ip);

    if (st.status == HOST_IP_CHANGED && strcmp(info->internal, st.current_ip) != 0) {
      vita_debug_log("[UI] Host %s detected IP change: old=%s, new=%s. Deferring IP update dialog.", info->name, info->internal, st.current_ip);
      pending_ip_update_idx = host_idx;
      strncpy(pending_ip_update, st.current_ip, sizeof(pending_ip_update)-1);
      pending_ip_update[sizeof(pending_ip_update)-1] = '\0';
      exit_menu = 1;
    } else if (st.status == HOST_ONLINE) {
      vita_debug_log("[UI] Host %s is ONLINE. Connecting normally.", info->name);
      vita_debug_log("[UI] Deteniendo escaneo de hosts antes de conectar");
      stop_host_scan();
      ui_connect_paired_device(info);
      exit_menu = 2;
    } else {
      vita_debug_log("[UI] Host %s is OFFLINE or unreachable.", info->name);
      flash_message("Host is offline or unreachable.");
      return 0;
    }
  }
  switch (cursor) {
    case MAIN_MENU_CONNECT:
      vita_debug_log("[UI] Seleccionado: Add manually");
      ui_connect_manual();
      exit_menu = 2;
      break;
    case MAIN_MENU_SEARCH:
      vita_debug_log("[UI] Seleccionado: Search devices");
      ui_search_device();
      exit_menu = 2;
      break;
    case MAIN_MENU_CONNECT_RESUME:
      vita_debug_log("[UI] Seleccionado: Resume connection");
      vita_debug_log("[UI] Deteniendo escaneo de hosts antes de reanudar conexión");
      stop_host_scan();
      ui_connect_resume();
      exit_menu = 2;
      break;
    case MAIN_MENU_SETTINGS:
      vita_debug_log("[UI] Seleccionado: Settings");
      vita_debug_log("[UI] Deteniendo escaneo de hosts antes de entrar a Settings");
      stop_host_scan();
      ui_settings_menu();
      exit_menu = 0;
      start_host_scan();
      break;
    case MAIN_MENU_QUIT:
      vita_debug_log("[UI] Seleccionado: Quit");
      vita_debug_log("[UI] Deteniendo escaneo de hosts antes de salir");
      stop_host_scan();
      if (connection_get_status() != LI_DISCONNECTED) {
        connection_terminate();
      }
      exit(0);
      return 0;
  }
  // Si se va a salir del menú, detener el escaneo
  if (exit_menu) {
    vita_debug_log("[UI] Saliendo del menú principal, deteniendo escaneo de hosts");
    stop_host_scan();
    return exit_menu;
  }
  return 0;
}

int ui_main_menu_back(void *context) {
  return 1;
}
int ui_main_menu() {
  // Siempre reiniciar escaneo de hosts al entrar al menú principal
  vita_debug_log("[UI] Entrando al menú principal, iniciando escaneo de hosts");
  start_host_scan();
  first_scan_pending = 1;

  menu_entry menu[16];
  int idx = 0;

#define MENU_TITLE(NAME) \
  do { \
    menu[idx] = (menu_entry) { .name = "", .disabled = true, .color = 0xff00aa00 }; \
    strcpy(menu[idx].subname, (NAME)); \
    idx++; \
  } while (0)
#define MENU_ENTRY(ID, NAME, DISABLED) \
  do { \
    menu[idx] = (menu_entry) { .name = (NAME), .disabled = (DISABLED), .id = (ID) }; \
    idx++; \
  } while(0)
#define MENU_SEPARATOR(NAME) \
  do { \
    menu[idx] = (menu_entry) { .name = (NAME), .disabled = true, .separator = true }; \
    idx++; \
  } while(0)

  char program_info[256];
  snprintf(program_info, 256, "Moonlight v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  MENU_TITLE(program_info);

  //char name[256] = {0};
  char addr[256] = {0};
  char resume_msg[512] = {0};
  //if (ui_connect_connected()) {
  //  ui_connect_address(addr);
  //  sprintf(name, "Resume connection to %s", addr);
  //} else {
  //  sprintf(name, "Connect to %s", config.address ? config.address : "none");
  //}

  static char last_ip_logged[256] = "";
  if (ui_connect_connected()) {
    MENU_SEPARATOR("Current connection");
    ui_connect_address(addr);
    sprintf(resume_msg, "Resume connection to %s", addr);
    MENU_ENTRY(MAIN_MENU_CONNECT_RESUME, resume_msg, false);
  } else {
    MENU_SEPARATOR("Add new computer");
    MENU_ENTRY(MAIN_MENU_SEARCH, "Search devices ...", false);
    MENU_ENTRY(MAIN_MENU_CONNECT, "Add manually ...", false);

    if (known_devices.count) {
      MENU_SEPARATOR("Paired computers");
      // Usar resultados del hilo de escaneo
      for (int i = 0; i < known_devices.count; i++) {
        device_info_t *cur = &known_devices.devices[i];
        if (!cur->paired) {
          continue;
        }
        struct host_status st;
        st = g_host_status[i];

        // unsigned int color = 0; // No usado
        // const char* color_str = ""; // No usado
        // bool ip_changed = (st.current_ip[0] && strcmp(cur->internal, st.current_ip) != 0); // No usado
        // bool ip_pending = (pending_ip_update_idx == i && pending_ip_update[0] != '\0'); // No usado
        MENU_ENTRY(MAIN_MENU_CONNECT_PAIRED + i, cur->name, false);
        int real_idx = idx - 1;
        menu[real_idx].is_host_entry = true;
        // Si hay cambio de IP pendiente para este host, forzar amarillo y saltar el resto
        if (pending_ip_update_idx == i && pending_ip_update[0] != '\0') {
          strcpy(menu[real_idx].subname, "[IP changed]");
          menu[real_idx].color = 0xFF00FFFF;
          vita_debug_log("[UI] Host %s menu idx=%d, color=0x%08X (YELLOW, IP pendiente), archivo=ui.c", cur->name, real_idx, 0xFF00FFFF);
          continue;
        }
        // ...lógica normal de color...
        if (st.current_ip[0] && strcmp(cur->internal, st.current_ip) != 0) {
          if (strcmp(last_ip_logged, st.current_ip) != 0) {
            vita_debug_log("[UI] Host %s IP changed: old=%s, new=%s", cur->name, cur->internal, st.current_ip);
            strncpy(last_ip_logged, st.current_ip, sizeof(last_ip_logged)-1);
            last_ip_logged[sizeof(last_ip_logged)-1] = '\0';
          }
          strcpy(menu[real_idx].subname, "[IP changed]");
          menu[real_idx].color = 0xFF00FFFF;
          vita_debug_log("[UI] Host %s menu idx=%d, color=0x%08X (YELLOW), archivo=ui.c", cur->name, real_idx, 0xFF00FFFF);
        } else if (strcmp(menu[real_idx].subname, "[IP changed]") == 0) {
          menu[real_idx].color = 0xFF00FFFF;
          vita_debug_log("[UI] Host %s menu idx=%d, color=0x%08X (YELLOW), archivo=ui.c", cur->name, real_idx, 0xFF00FFFF);
        } else if (st.status == HOST_ONLINE) {
          menu[real_idx].color = 0xFF00FF00;
          vita_debug_log("[UI] Host %s menu idx=%d, color=0x%08X (GREEN), archivo=ui.c", cur->name, real_idx, 0xFF00FF00);
        } else {
          menu[real_idx].color = 0xFF0000FF;
          vita_debug_log("[UI] Host %s menu idx=%d, color=0x%08X (RED), archivo=ui.c", cur->name, real_idx, 0xFF0000FF);
        }
      }
    }
  }

  MENU_SEPARATOR("");
  MENU_ENTRY(MAIN_MENU_SETTINGS, "Settings", false);
  MENU_ENTRY(MAIN_MENU_QUIT, "Quit", false);

  // Solo iniciar escaneo si NO hay cambio de IP pendiente y no se ha hecho ya
  menu_geom geom = make_geom_centered(500, 200);
  return display_menu(menu, idx, &geom, &ui_main_menu_loop, &ui_main_menu_back, NULL, NULL);
}

int global_loop(int cursor, void *ctx, const input_data *input) {
  if (is_rectangle_touched(&input->touch, 0, 0, 150, 150)) {
    if (connection_get_status() == LI_MINIMIZED) {
      vitapower_config(config);
      vitainput_config(config);

      sceKernelDelayThread(500 * 1000);
      connection_resume();

      while (connection_is_connected()) {
        sceKernelDelayThread(500 * 1000);
      }
    }
  }
  return 0;
}

void gui_init() {
  guilib_init(&global_loop, NULL);
}

void gui_loop() {
  gui_init();

  while (ui_main_menu() == 2);

  vita2d_fini();
}
