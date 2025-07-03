/*
    Copyright 2020-2021 natinusala
    Copyright 2019 p-sam

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

#if defined(ANDROID) || defined(IOS)
#include <SDL2/SDL_main.h>
#endif

#include <borealis.hpp>
#include <cstdlib>
#include <string>
#include <iostream>

#include "activity/main_activity.hpp"
#include "SettingsPersistence.hpp"
#include "tab/add_host_tab.hpp"
#include "tab/settings_tab.hpp"
#include "tab/hosts_tab.hpp"



#if defined(__PSV__)
#include <psp2/kernel/threadmgr.h>
#include <psp2/vshbridge.h>
#endif

#if defined(__PSV__) && defined(BOREALIS_USE_OPENGL)
// Needed for the OpenGL driver to work
extern "C" unsigned int sceLibcHeapSize = 2 * 1024 * 1024;
#endif

using namespace brls::literals; // for _i18n

int main(int argc, char* argv[])
{
    // Leer idioma desde config y forzar variable de entorno antes de inicializar la app
    std::string lang = moonlight::SettingsPersistence::getLanguageFromConfig();
    std::cout << "[DEBUG] Idioma forzado desde config: " << lang << std::endl;
    if (!lang.empty()) {
        moonlight::SettingsPersistence::applyLanguageEnv(lang);
        brls::Application::setLocale(lang); // <-- Forzar el locale solo si hay config
    }

    // We recommend to use INFO for real apps
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-d") == 0) { // Set log level
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        } else if (std::strcmp(argv[i], "-o") == 0) {
            const char* path = (i + 1 < argc) ? argv[++i] : "borealis.log";
            brls::Logger::setLogOutput(std::fopen(path, "w+"));
        } else if (std::strcmp(argv[i], "-v") == 0) {
            brls::Application::enableDebuggingView(true);
        }
    }

    // Init the app and i18n
    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }
    // La carga de traducciones es automática tras Application::init() en esta versión de Borealis.

    std::cout << "[DEBUG] Locale detectado por Borealis: " << brls::Application::getLocale() << std::endl;

    // Cargar settings visuales (selector) después de init
    moonlight::SettingsPersistence::loadSettingsFromConfig();

    brls::Application::createWindow("moonlight/title"_i18n);

    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // Have the application register an action on every activity that will quit when you press BUTTON_START
    brls::Application::setGlobalQuit(false);

    // Registrar solo las vistas reales necesarias
    brls::Application::registerXMLView("AddHostTab", AddHostTab::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);
    brls::Application::registerXMLView("HostsTab", HostsTab::create);

#if defined(__PSV__)
    int search_unk[2];
    if(_vshKernelSearchModuleByName("CapUnlocker", search_unk) >= 0) {
        brls::Logger::info("[CapUnlocker] ¡CapUnlocker detectado por _vshKernelSearchModuleByName!");
        brls::sync([]() {
            brls::Application::notify("moonlight/capunlocker_notify"_i18n);
        });
        int res1 = sceKernelChangeThreadPriority(SCE_KERNEL_THREAD_ID_SELF, 64);
        int res2 = sceKernelChangeThreadCpuAffinityMask(SCE_KERNEL_THREAD_ID_SELF, 0xF);
        brls::Logger::info("[CapUnlocker] Cambio de prioridad res={} | Cambio de afinidad res={}", res1, res2);
    } else {
        brls::Logger::info("[CapUnlocker] CapUnlocker NO detectado. Solo 1-2 núcleos y RAM limitada disponibles.");
    }
#endif

    // El estilo visual de los labels de "about" (fuente y color) se aplica en el controlador de la vista correspondiente
    // usando setFontSize y setTextColor tras obtener cada Label por su id.
    // Aquí solo se mantienen las métricas de padding/margen si el layout XML las utiliza.
    brls::getStyle().addMetric("about/padding_top_bottom", 50);
    brls::getStyle().addMetric("about/padding_sides", 75);
    brls::getStyle().addMetric("about/description_margin", 50);

    // Create and push the main activity to the stack
    brls::Application::pushActivity(new MainActivity());

    // Run the app
    while (brls::Application::mainLoop())
        ;

    // Exit
    return EXIT_SUCCESS;
}

#ifdef __WINRT__
#include <borealis/core/main.hpp>
#endif
