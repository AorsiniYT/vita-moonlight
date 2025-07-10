/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#include "connection/pairing.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <borealis.hpp>
#include <filesystem>
#include "ConfigManager.hpp"
#include <cstdlib>
#include <ctime>
#include "model/HostStorage.hpp"
#include <cstring>
#if defined(__PSV__)
#include <psp2/kernel/threadmgr.h>
#include <psp2/vshbridge.h>
#endif

#include "client.h"
#include "errors.h"


// Estructura para manejar el popup y el estado
struct PairingPopupContext {
    brls::Dialog* dialog;
    brls::Label* label;
    brls::ProgressSpinner* spinner; // NUEVO: puntero al spinner
    std::atomic<bool>* finished;
    std::atomic<bool>* success;
    std::string pin;
};


// Función para mostrar mensajes en el popup
static void showPopupMessage(PairingPopupContext* ctx, const std::string& msg, bool hideSpinner = false) {
    if (ctx && ctx->dialog && ctx->label) {
        brls::sync([ctx, msg, hideSpinner] {
            ctx->label->setText(msg);
            // Solo ocultar el spinner si hideSpinner es true, si no, mostrarlo
            if (ctx->spinner) {
                if (hideSpinner)
                    ctx->spinner->setVisibility(brls::Visibility::GONE);
                else
                    ctx->spinner->setVisibility(brls::Visibility::VISIBLE);
            }
        });
        // Ya no se cierra el diálogo aquí, solo se muestra el mensaje de éxito.
    }
}



// --- Control global de pairing activo para cancelación segura ---
static std::atomic<bool>* g_pairing_cancelled = nullptr;
static std::atomic<bool>* g_pairing_finished = nullptr;

void stopPairing() {
    printf("[PAIRING][STOP] stopPairing() llamado\n");
    if (g_pairing_cancelled) {
        printf("[PAIRING][STOP] Señalando cancelación global de pairing...\n");
        *g_pairing_cancelled = true;
    } else {
        printf("[PAIRING][STOP] No hay pairing_cancelled activo\n");
    }
    // Ya no esperamos a que termine el hilo de pairing para no bloquear la UI
    if (g_pairing_finished) {
        printf("[PAIRING][STOP] Señal de cancelación enviada, no se espera al hilo.\n");
    } else {
        printf("[PAIRING][STOP] No hay pairing_finished activo\n");
    }
    g_pairing_cancelled = nullptr;
    g_pairing_finished = nullptr;
}



// Nueva versión: usa un diálogo externo y permite cancelación
void startMoonlightPairingWithPopupCustomDialog(const std::string& hostIp, const std::string& hostName, brls::Dialog* dialog, std::atomic<bool>* cancelled, std::function<void(bool)> onFinished, brls::Label* label, brls::ProgressSpinner* spinner) {
    printf("[PAIRING][DEBUG] Flag cancelled (ptr=%p) valor inicial: %d\n", (void*)cancelled, (int)cancelled->load());
    auto finished = new std::atomic<bool>(false);
    auto success = new std::atomic<bool>(false);
    auto* popupCtx = new PairingPopupContext{dialog, label, spinner, finished, success, ""};

    // Registrar los flags globales para poder cancelar desde fuera
    g_pairing_cancelled = cancelled;
    g_pairing_finished = finished;

    // --- Multihilo nativo PSVita para gs_init ---
#if defined(__PSV__)
    // Detección real de CapUnlocker/CoreUnlocker
    int search_unk[2];
    bool capunlockerDetected = (_vshKernelSearchModuleByName("CapUnlocker", search_unk) >= 0);
    if (capunlockerDetected) {
        printf("[PAIRING][CAPUNLOCKER] CapUnlocker detectado por _vshKernelSearchModuleByName\n");
    } else {
        printf("[PAIRING][CAPUNLOCKER] CapUnlocker NO detectado. Solo 1-2 núcleos y RAM limitada disponibles.\n");
    }
    // Afinidad: 3 núcleos de usuario estándar + el cuarto núcleo (SYSTEM) si CapUnlocker está activo
    int mask = SCE_KERNEL_CPU_MASK_USER_ALL;
    if (capunlockerDetected) {
        mask |= SCE_KERNEL_CPU_MASK_SYSTEM; // Añade el cuarto núcleo si el plugin lo permite
    }
    int res = sceKernelChangeThreadCpuAffinityMask(0, mask);
    printf("[PAIRING][CPU][MAIN] Afinidad a núcleos (mask=0x%X), resultado: %d\n", mask, res);
    int affinity = sceKernelGetThreadCpuAffinityMask(0);
    printf("[PAIRING][CPU][MAIN] Affinity mask real tras cambio: 0x%X\n", affinity);
    // Diagnóstico: mostrar el valor de cada bit relevante (núcleos 16-19)
    for (int i = 16; i <= 19; ++i) {
        printf("[PAIRING][CPU][MAIN] Núcleo %d: %s\n", i - 16, (affinity & (1 << i)) ? "ACTIVO" : "NO");
    }
    fflush(stdout);

    struct GsInitArgs {
        PairingPopupContext* popupCtx;
        std::atomic<bool>* finished;
        std::atomic<bool>* success;
        std::atomic<bool>* cancelled;
        std::function<void(bool)>* onFinished;
        const std::string* hostIp;
        const std::string* hostName;
        const std::string* safeHostName;
        const std::string* hostDir;
        char* address;
        SERVER_DATA* server;
        int* initRes;
        int logLevel;
        bool unsupported;
        std::chrono::high_resolution_clock::time_point* t_gsinit_start;
        std::chrono::high_resolution_clock::time_point* t_gsinit_end;
    };

    auto gs_init_thread = [](SceSize args, void* argp) -> int {
        GsInitArgs* pargs = (GsInitArgs*)argp;
        FILE* f = fopen("ux0:data/moonlight/gsinit_thread.log", "a");
        if (f) { fprintf(f, "INICIO HILO NATIVO\n"); fclose(f); }
        printf("[PAIRING][THREAD][DEBUG] Entrando en hilo nativo gs_init_thread\n"); fflush(stdout);
        int mask = SCE_KERNEL_CPU_MASK_USER_ALL | SCE_KERNEL_CPU_MASK_SYSTEM;
        int res = sceKernelChangeThreadCpuAffinityMask(0, mask);
        printf("[PAIRING][CPU][THREAD] Afinidad a núcleos (mask=0x%X), resultado: %d\n", mask, res); fflush(stdout);
        int affinity = sceKernelGetThreadCpuAffinityMask(0);
        printf("[PAIRING][CPU][THREAD] Affinity mask real tras cambio: 0x%X\n", affinity); fflush(stdout);
        for (int i = 16; i <= 19; ++i) {
            printf("[PAIRING][CPU][THREAD] Núcleo %d: %s\n", i - 16, (affinity & (1 << i)) ? "ACTIVO" : "NO"); fflush(stdout);
        }
        *(pargs->t_gsinit_start) = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][THREAD][DEBUG] Antes de gs_init()\n"); fflush(stdout);
        *(pargs->initRes) = gs_init(pargs->server, std::string(pargs->address), *pargs->hostDir);
        *(pargs->t_gsinit_end) = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][THREAD][DEBUG] Después de gs_init(), resultado: %d\n", *(pargs->initRes)); fflush(stdout);
        if (f) { f = fopen("ux0:data/moonlight/gsinit_thread.log", "a"); if (f) { fprintf(f, "FIN HILO NATIVO\n"); fclose(f); } }
        printf("[PAIRING][CPU][THREAD] FIN HILO NATIVO\n"); fflush(stdout);
        return 0;
    };
#endif

    // --- Parseo de IP y puerto ---
    std::string ip = hostIp;
    int port = 47989;
    size_t colon = hostIp.find(":");
    if (colon != std::string::npos) {
        ip = hostIp.substr(0, colon);
        try {
            port = std::stoi(hostIp.substr(colon + 1));
        } catch (...) {
            port = 47989;
        }
    }

    printf("[PAIRING] Iniciando emparejamiento con host: %s, nombre: %s, puerto: %d\n", ip.c_str(), hostName.c_str(), port);
    auto t_start = std::chrono::high_resolution_clock::now();
    SERVER_DATA server = {};
    char address[256];
    strncpy(address, ip.c_str(), sizeof(address));
    address[sizeof(address)-1] = '\0';

    // --- LOG DE PERF: Antes de cargar configuración ---
    auto t_before_config = std::chrono::high_resolution_clock::now();
    printf("[PAIRING][PERF] tA Antes de cargar ConfigManager: %.3f ms\n", std::chrono::duration<double, std::milli>(t_before_config.time_since_epoch()).count());
    ConfigManager config;
    config.load();
    auto t_after_config = std::chrono::high_resolution_clock::now();
    printf("[PAIRING][PERF] tB Después de cargar ConfigManager: %.3f ms (delta: %.3f ms)\n", std::chrono::duration<double, std::milli>(t_after_config.time_since_epoch()).count(), std::chrono::duration<double, std::milli>(t_after_config-t_before_config).count());

    std::string keyDirStr = config.getKeysDir();
    printf("[PAIRING][DEBUG] keyDirStr(base)='%s'\n", keyDirStr.c_str());
    printf("[PAIRING][DEBUG] hostName='%s'\n", hostName.c_str());
    printf("[PAIRING][LOG] add host: ip='%s', name='%s'\n", hostIp.c_str(), hostName.c_str());
    std::string safeHostName = hostName;
    for (char& c : safeHostName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            printf("[PAIRING][LOG] Caracter no permitido en hostName: '%c', reemplazando por '_'\n", c);
            c = '_';
        }
    }
    printf("[PAIRING][DEBUG] safeHostName='%s'\n", safeHostName.c_str());
    std::string hostDir = keyDirStr + "/" + safeHostName;
    printf("[PAIRING][DEBUG] hostDir(final)='%s'\n", hostDir.c_str());
    const char* keyDir = hostDir.c_str();
    std::error_code ec;

    // --- LOG DE PERF: Antes de borrar carpeta de llaves ---
    auto t_before_remove = std::chrono::high_resolution_clock::now();
    printf("[PAIRING][PERF] tC Antes de remove_all: %.3f ms\n", std::chrono::duration<double, std::milli>(t_before_remove.time_since_epoch()).count());
    if (std::filesystem::exists(hostDir)) {
        printf("[PAIRING] Borrando carpeta de llaves existente para host: %s\n", hostDir.c_str());
        std::filesystem::remove_all(hostDir, ec);
        auto t_after_remove = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] tD Después de remove_all: %.3f ms (delta: %.3f ms)\n", std::chrono::duration<double, std::milli>(t_after_remove.time_since_epoch()).count(), std::chrono::duration<double, std::milli>(t_after_remove-t_before_remove).count());
        if (ec) {
            printf("[PAIRING] Error borrando carpeta vieja: %s\n", ec.message().c_str());
        } else {
            printf("[PAIRING][LOG] Carpeta vieja eliminada correctamente\n");
        }
    } else {
        auto t_after_remove = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] tD Después de remove_all (no existía): %.3f ms (delta: %.3f ms)\n", std::chrono::duration<double, std::milli>(t_after_remove.time_since_epoch()).count(), std::chrono::duration<double, std::milli>(t_after_remove-t_before_remove).count());
    }

    // --- LOG DE PERF: Antes de crear directorio de llaves ---
    auto t_before_mkdir = std::chrono::high_resolution_clock::now();
    printf("[PAIRING][PERF] tE Antes de create_directories: %.3f ms\n", std::chrono::duration<double, std::milli>(t_before_mkdir.time_since_epoch()).count());
    std::filesystem::create_directories(hostDir, ec);
    auto t_after_mkdir = std::chrono::high_resolution_clock::now();
    printf("[PAIRING][PERF] tF Después de create_directories: %.3f ms (delta: %.3f ms)\n", std::chrono::duration<double, std::milli>(t_after_mkdir.time_since_epoch()).count(), std::chrono::duration<double, std::milli>(t_after_mkdir-t_before_mkdir).count());
    if (ec) {
        printf("[PAIRING] Error creando el directorio de llaves: %s\n", ec.message().c_str());
    } else {
        printf("[PAIRING][LOG] Directorio de llaves creado: %s\n", hostDir.c_str());
    }

    int logLevel = 2;
    bool unsupported = false;

    // --- Popup de cancelación bloqueante ---
    auto cancelDialog = std::make_shared<brls::Dialog>("");

    // Lanzar el hilo principal de pairing
    std::thread([=]() mutable {
        auto t_before_gsinit = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] tG Antes de gs_init: %.3f ms\n", std::chrono::duration<double, std::milli>(t_before_gsinit.time_since_epoch()).count());
        printf("[PAIRING][PERF] Antes de gs_init\n");
        printf("[PAIRING] Llamando a gs_init con keyDir='%s'...\n", keyDir);
        printf("[PAIRING][DEBUG][REQUEST] gs_init params: address='%s', port=%d, keyDir='%s', logLevel=%d, unsupported=%d\n", address, port, keyDir, logLevel, unsupported);

        #if defined(__PSV__)
        // --- Lanzar hilo nativo PSVita para gs_init ---
        int initRes = -1;
        auto t_gsinit_start = std::chrono::high_resolution_clock::now();
        auto t_gsinit_end = t_gsinit_start;
        GsInitArgs args = {popupCtx, finished, success, cancelled, &onFinished, &hostIp, &hostName, &safeHostName, &hostDir, address, &server, &initRes, logLevel, unsupported, &t_gsinit_start, &t_gsinit_end};
        // Aumentar stack a 256 KB para evitar crash por stack pequeño
        SceUID thid = sceKernelCreateThread("gsInitThread", gs_init_thread, 0x10000100, 0x40000, 0, 0, NULL);
        if (thid >= 0) {
            printf("[PAIRING][DEBUG] Hilo nativo creado correctamente (thid=%d), lanzando gs_init...\n", thid); fflush(stdout);
            int startRes = sceKernelStartThread(thid, sizeof(GsInitArgs), &args);
            printf("[PAIRING][DEBUG] Resultado de sceKernelStartThread: %d\n", startRes); fflush(stdout);
            int waitRes = sceKernelWaitThreadEnd(thid, NULL, NULL);
            printf("[PAIRING][DEBUG] Resultado de sceKernelWaitThreadEnd: %d\n", waitRes); fflush(stdout);
        } else {
            printf("[PAIRING][ERROR] No se pudo crear hilo nativo para gs_init, usando fallback.\n"); fflush(stdout);
            initRes = gs_init(&server, std::string(address), hostDir);
            t_gsinit_end = std::chrono::high_resolution_clock::now();
        }
        #else
        auto t_gsinit_start = std::chrono::high_resolution_clock::now();
        int initRes = gs_init(&server, std::string(address), hostDir);
        auto t_gsinit_end = std::chrono::high_resolution_clock::now();
        #endif
        double ms_gsinit = std::chrono::duration<double, std::milli>(t_gsinit_end-t_gsinit_start).count();
        printf("[PAIRING][PERF] Después de gs_init\n"); fflush(stdout);
        printf("[PAIRING][DEBUG][RESPONSE] gs_init result: %d (duración: %.2f ms)\n", initRes, ms_gsinit); fflush(stdout);
        printf("[PAIRING][PERF] Tiempo total desde t_start hasta fin de gs_init: %.2f ms\n", std::chrono::duration<double, std::milli>(t_gsinit_end-t_start).count()); fflush(stdout);
        printf("[PAIRING][DEBUG] Valor de *cancelled tras gs_init: %d\n", (int)cancelled->load());
        // --- NUEVO: Si se canceló durante gs_init, salir seguro aquí ---
        if (*cancelled) {
            printf("[PAIRING][CANCEL] Cancelación detectada tras gs_init. Saliendo seguro antes de continuar.\n"); fflush(stdout);
            *success = false;
            *finished = true;
            if (onFinished) onFinished(false);
            return;
        }

        if (initRes != GS_OK) {
            printf("[PAIRING][ERROR] Error en gs_init. No se pudo inicializar la conexión.\n");
            std::string gs_err = gs_error();
            if (!gs_err.empty()) printf("[PAIRING][DEBUG][RESPONSE] gs_init error: %s\n", gs_err.c_str());
            if (!*cancelled) showPopupMessage(popupCtx, brls::getStr("host_dialog/pairing_error_init"));
            *success = false;
            *finished = true;
            if (onFinished) onFinished(false);
            return;
        }

        // Generar PIN aleatorio
        auto t_before_pin = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] Tiempo desde fin de gs_init hasta mostrar PIN: %.2f ms\n",
            std::chrono::duration<double, std::milli>(t_before_pin - t_gsinit_end).count());
        printf("[PAIRING][PERF] Tiempo total desde t_start hasta mostrar PIN: %.2f ms\n",
            std::chrono::duration<double, std::milli>(t_before_pin - t_start).count());
        char pin[5];
        srand((unsigned int)time(nullptr));
        snprintf(pin, sizeof(pin), "%d%d%d%d",
            rand() % 10, rand() % 10, rand() % 10, rand() % 10);
        popupCtx->pin = pin;
        printf("[PAIRING] PIN generado: %s\n", pin);
        {
            std::string msg = brls::getStr("host_dialog/pairing_enter_pin");
            size_t pos = msg.find("$(pin)");
            if (pos != std::string::npos)
                msg.replace(pos, 6, pin);
            if (!*cancelled) showPopupMessage(popupCtx, msg, true); // true = ocultar spinner
        }

        // Llamar a gs_pair (esto realiza el pairing real)
        printf("[PAIRING][DEBUG] Valor de *cancelled antes de gs_pair: %d\n", (int)cancelled->load());
        printf("[PAIRING][PERF] Antes de gs_pair\n");
        printf("[PAIRING] Llamando a gs_pair...\n");
        printf("[PAIRING][DEBUG][REQUEST] gs_pair params: address='%s', pin='%s', port=%d\n", address, pin, port);
        auto t_gspair_start = std::chrono::high_resolution_clock::now();
        int pairRes = -1;
        if (!*cancelled)
            pairRes = gs_pair(&server, pin);
        auto t_gspair_end = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] Después de gs_pair\n");
        printf("[PAIRING][DEBUG][RESPONSE] gs_pair result: %d (duración: %.2f ms)\n", pairRes, std::chrono::duration<double, std::milli>(t_gspair_end-t_gspair_start).count());
        std::string gs_err = gs_error();
        if (!gs_err.empty()) printf("[PAIRING][DEBUG][RESPONSE] gs_pair error: %s\n", gs_err.c_str());
        printf("[PAIRING][DEBUG] Archivos en '%s' tras gs_pair:\n", hostDir.c_str());
        for (const auto& entry : std::filesystem::directory_iterator(hostDir)) {
            printf("[PAIRING][DEBUG]   %s\n", entry.path().string().c_str());
        }
        if (*cancelled) {
            printf("[PAIRING][CANCEL] Pairing cancelado por el usuario antes de finalizar.\n");
            *success = false;
            *finished = true;
            if (onFinished) onFinished(false);
            return;
        }
        if (pairRes == GS_OK) {
            printf("[PAIRING] Emparejamiento exitoso.\n");
            if (!*cancelled) showPopupMessage(popupCtx, brls::getStr("host_dialog/pairing_success"));
            HostStorage::savePairedHost(safeHostName, address, server.httpPort, server.paired);
            if (HostStorage::writeDeviceIni(hostDir, safeHostName, address, server.httpPort, server.paired)) {
                printf("[PAIRING] device.ini generado en: %s\n", (hostDir + "/device.ini").c_str());
            } else {
                printf("[PAIRING][ERROR] No se pudo crear device.ini en: %s\n", (hostDir + "/device.ini").c_str());
            }
            *success = true;
            // Esperar 1 segundo para que el usuario vea el mensaje de éxito antes de cerrar el popup
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (onFinished) onFinished(true);
        } else {
            std::string err = brls::getStr("host_dialog/pairing_error");
            std::string gs_err = gs_error();
            if (!gs_err.empty()) {
                std::string errDetail = err;
                size_t pos = errDetail.find("$(error)");
                if (pos != std::string::npos)
                    errDetail.replace(pos, 8, gs_err);
                err = errDetail;
            }
            printf("[PAIRING][ERROR] %s\n", err.c_str());
            printf("[PAIRING][LOG] Intentando cleanup/unpair tras fallo de pairing...\n");
            int unpairRes = gs_unpair(&server);
            printf("[PAIRING][LOG] Resultado de gs_unpair: %d\n", unpairRes);
            if (!*cancelled) showPopupMessage(popupCtx, err);
            *success = false;
            if (onFinished) onFinished(false);
        }
        *finished = true;
        auto t_end = std::chrono::high_resolution_clock::now();
        printf("[PAIRING][PERF] Emparejamiento total: %.2f ms\n", std::chrono::duration<double, std::milli>(t_end-t_start).count());
    }).detach();

    // Cerrar el popup cuando termine (dentro de la función, después del detach principal)
    std::thread([dialog, finished]() {
        while (!finished->load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        // Siempre desbloquear inputs y cerrar el diálogo si sigue visible
        brls::sync([dialog]() {
            dialog->dismiss();
            brls::Application::unblockInputs();
        });
        delete finished;
    }).detach();
}

void startMoonlightPairingWithPopup(const std::string& hostIp, const std::string& hostName, std::function<void(bool)> onFinished) {
    // Crear un diálogo sin botones, solo con texto y spinner
    auto* holder = new brls::Box(brls::Axis::COLUMN);
    auto* label = new brls::Label();
    label->setText(brls::getStr("host_dialog/connecting"));
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setMarginBottom(21);
    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->View::setSize(brls::Size(92, 92));
    holder->addView(label);
    holder->addView(spinner);
    holder->setAlignItems(brls::AlignItems::CENTER);
    holder->setJustifyContent(brls::JustifyContent::CENTER);
    holder->setPadding(28, 28, 28, 28);
    auto* connectingDialog = new brls::Dialog(holder);
    connectingDialog->setCancelable(false);
    connectingDialog->setFocusable(true);
    connectingDialog->setHideHighlight(true);
    connectingDialog->open();
    // Bloquear inputs globales para evitar movimiento de fondo
    brls::Application::blockInputs();
    // Flag de cancelación local
    static std::atomic<bool> global_cancelled(false);
    global_cancelled = false; // Siempre reiniciar antes de cada pairing
    printf("[PAIRING][DEBUG] Reiniciando flag global_cancelled a %d antes de pairing.\n", (int)global_cancelled.load());
    startMoonlightPairingWithPopupCustomDialog(hostIp, hostName, connectingDialog, &global_cancelled, [onFinished](bool pairingSuccess) {
        brls::Application::unblockInputs();
        if (onFinished) onFinished(pairingSuccess);
    }, label, spinner);
}