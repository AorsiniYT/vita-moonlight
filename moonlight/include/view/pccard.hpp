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
#pragma once
#include <borealis.hpp>

// Reusable widget to display a "PC card" as a large button with centered image and name.
class PCCard : public brls::Button {
public:
    PCCard(const std::string& name, const std::string& imagePath = "resources/img/moonlight/pc.png");

    void setPCName(const std::string& name);
    void setPCImage(const std::string& imagePath);
    void setClickAction(std::function<void()> cb);

private:
    brls::Image* image = nullptr;
    brls::Label* label = nullptr;
    brls::Box* box = nullptr; // Save the layout so you can update it if required
    std::function<void()> clickCb;
};
