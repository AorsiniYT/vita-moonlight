# Sniffer UDP mDNS Moonlight/Sunshine para PSVita y Windows

## Autor
AorsiniYT (2025)

---

## Descripción
Este proyecto implementa un sniffer UDP mDNS multiplataforma para detectar servicios Moonlight/Sunshine (GameStream) en la red local. Permite descubrir hosts compatibles con GeForce Experience o Sunshine, mostrando su host, nombre de PC, IP y puerto, tanto en PSVita (VitaSDK) como en Windows (MinGW).

- **PSVita:** El sniffer es modular, fácil de integrar como librería y notifica detecciones mediante callback. Los resultados se muestran en pantalla y consola.
- **Windows:** Implementación modular, lista para integrarse como librería en otros proyectos, con ejemplo de uso incluido.

Si necesita implementarlo en otra plataforma, me dice y lo hago

---

## Estructura del Proyecto

- `udp_sniffer_vita.c/h`  — Sniffer mDNS para PSVita, API modular y callback.
- `udp_sniffer_win.c/h`   — Sniffer mDNS para Windows (modular, con callback y ejemplo de uso).
- `main.c`                — Ejemplo de integración multiplataforma (Vita/Win).
- `maketest`              — Script de build y despliegue para Vita/Windows.
- `toolchain-mingw.cmake` — Toolchain para compilar en Windows con MinGW.
- `build_test/`           — Build para PSVita.
- `build_win/`            — Build para Windows.

---

## Uso del Script de Build

```sh
# Compilar y desplegar en PSVita (por defecto)
./maketest
# o explícito
./maketest vita

# Compilar para Windows (MinGW, ejecutable en build_win/)
./maketest win
```

- Para PSVita, el script compila, empaqueta el VPK y lo envía por FTP a la consola.
- Para Windows, genera el ejecutable en `build_win/` usando el toolchain MinGW.

---

## Requisitos previos

Para usar el script `maketest` necesitas tener instalado:

### 1. VitaSDK (para compilar en PSVita)

- Instala dependencias básicas:
  ```sh
  sudo apt update && sudo apt install git cmake build-essential python3
  ```
- Descarga e instala VitaSDK:
  ```sh
  git clone https://github.com/vitasdk/vdpm.git
  cd vdpm
  ./bootstrap-vitasdk.sh
  export VITASDK="$HOME/vitasdk"
  export PATH="$VITASDK/bin:$PATH"
  # Puedes añadir las dos líneas anteriores a tu ~/.bashrc para que se carguen siempre
  ```
- Instala los paquetes necesarios de VitaSDK:
  ```sh
  vdpm install zlib libpng freetype
  ```

### 2. MinGW y dependencias para Windows

- Instala MinGW y herramientas necesarias:
  ```sh
  sudo apt install mingw-w64 cmake make gcc-mingw-w64 g++-mingw-w64
  ```
- No se requieren librerías externas adicionales, el toolchain-mingw.cmake está listo para compilar el sniffer modular para Windows.

---

## Integración como Librería (PSVita)

1. Incluye `udp_sniffer_vita.h` en tu proyecto.
2. Registra tu callback con `udp_sniffer_vita_set_callback()`.
3. Llama periódicamente a `udp_sniffer_vita_poll()` en tu bucle principal.

```c
void moonlight_found_callback(int idx, const char* host, const char* pcname, const char* ip, int port) {
    // Tu código aquí
}

int main() {
    udp_sniffer_vita_set_callback(moonlight_found_callback);
    while (1) {
        udp_sniffer_vita_poll();
        // ...
    }
}
```

---

## Integración como Librería (Windows)

1. Incluye `udp_sniffer_win.h` en tu proyecto.
2. Registra tu callback con `udp_sniffer_win_set_callback()`.
3. Llama a `udp_sniffer_win_init()` una vez antes de usar.
4. Llama periódicamente a `udp_sniffer_win_poll()` en tu bucle principal.
5. Llama a `udp_sniffer_win_deinit()` al finalizar.

```c
#include "udp_sniffer_win.h"

void moonlight_found_callback_win(int idx, const char* host, const char* pcname, const char* ip, int port) {
    printf("[Moonlight #%d] Host: %s | Nombre PC: %s | IP: %s | Puerto: %d\n", idx, host, pcname, ip, port);
}

int main() {
    udp_sniffer_win_set_callback(moonlight_found_callback_win);
    if (udp_sniffer_win_init() < 0) {
        printf("Error inicializando el sniffer mDNS.\n");
        return 1;
    }
    while (1) {
        udp_sniffer_win_poll();
        Sleep(10); // No saturar CPU
    }
    udp_sniffer_win_deinit();
    return 0;
}
```

---

## ¿Qué detecta?
- Solo servicios mDNS Moonlight/Sunshine (`_nvstream._tcp.local`).
- Asocia correctamente registros PTR, SRV y A.
- Deduplica y solo notifica cuando todos los datos están presentes.
- El callback se invoca solo cuando hay host, nombre PC, IP y puerto.
- El resultado se enumera como `[Moonlight #N] ...` igual en Vita y Windows.

---

## Créditos
- Desarrollo y adaptación: **AorsiniYT**
- Basado en lógica de sniffer UDP mDNS para Windows y adaptado a PSVita (VitaSDK).

---

## Notas
- El código es profesional, modular y fácil de integrar en otros proyectos (ej: Moonlight Vita).
- El sniffer para Vita y Windows no depende de librerías mdns externas.
- El script `maketest` automatiza todo el flujo de build y despliegue.
- El ejecutable de Windows solo compila el sniffer modular, sin dependencias innecesarias.

---

## Licencia
MIT o similar. Puedes usar, modificar y compartir este código citando al autor.
