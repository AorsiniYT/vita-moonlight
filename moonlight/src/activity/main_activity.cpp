/*
    Copyright 2020-2021 natinusala

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

#include "activity/main_activity.hpp"

#include "controller/ControllerInput.hpp"

// Ensures virtual destructor is defined to avoid vtable error
MainActivity::MainActivity()
{
    // Initialize input manager if it does not exist
    if (!g_controllerInput)
    {
        g_controllerInput = new ControllerInputManager();
    }

    // Resetear focus al default
    brls::Application::giveFocus(this->getDefaultFocus());
}

MainActivity::~MainActivity() { }
