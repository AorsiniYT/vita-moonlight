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
#include "settings.hpp"
#include "tab/settings_tab.hpp"
#include "ConfigManager.hpp"
#include <borealis.hpp>
#include <string>

using namespace brls;

namespace moonlight {

void settings::loadSettingsFromConfig() {
    ConfigManager config;
    config.load();
    std::string lang = config.get("general", "language", "en-US");
    // Adjust visual selector (only after init)
    if (lang == "es" || lang == "es-ES") {
        if (SettingsTab::languageSelectorPtr)
            SettingsTab::languageSelectorPtr->setSelection(0);
    } else {
        if (SettingsTab::languageSelectorPtr)
            SettingsTab::languageSelectorPtr->setSelection(1);
    }
    // Here you can load more settings and apply them
}

void settings::saveSettingsToConfig() {
    ConfigManager config;
    // Example: save language
    int langIdx = 1;
    if (SettingsTab::languageSelectorPtr)
        langIdx = SettingsTab::languageSelectorPtr->getSelection();
    std::string lang = (langIdx == 0) ? "es" : "en-US";
    config.set("general", "language", lang);
    // Here you can save more settings
    config.save();
}

std::string settings::getLanguageFromConfig() {
    ConfigManager config;
    if (!config.load())
        return "en-US";  // Default value if unable to load
    return config.get("general", "language", "en-US");
}

void settings::applyLanguageEnv(const std::string& lang) {
#ifdef _WIN32
    _putenv_s("LANG", lang.c_str());
    _putenv_s("BOREALIS_LANG", lang.c_str());
#else
    setenv("LANG", lang.c_str(), 1);
    setenv("BOREALIS_LANG", lang.c_str(), 1);
#endif
}

} // namespace moonlight
