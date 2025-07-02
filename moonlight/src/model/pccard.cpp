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


#include "model/pccard.hpp"

// Card visual como botón grande reutilizable
PCCard::PCCard(const std::string& name, const std::string& imagePath) : brls::Button() {
    this->setText("");
    this->setWidth(180);
    this->setHeight(140);
    this->setPadding(0);
    this->setCornerRadius(16);
    this->setBackgroundColor(nvgRGB(35, 39, 46));
    this->setBorderColor(nvgRGB(100, 100, 120));
    this->setBorderThickness(2);

    // Layout vertical: imagen y label

    box = new brls::Box(brls::Axis::COLUMN);
    box->setWidth(180);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setFocusable(false);

    image = new brls::Image();
    std::string fixedPath = (imagePath == "resources/img/moonlight/pc.png") ? "img/moonlight/pc.png" : imagePath;
    image->setImageFromRes(fixedPath);
    image->setWidth(48);
    image->setHeight(48);
    image->setMarginTop(20);
    image->setMarginBottom(18); // Separación extra entre imagen y texto
    box->addView(image);

    label = new brls::Label();
    label->setText(name);
    label->setFontSize(16);
    label->setMargins(0, 0, 0, 0); // El margen superior ya lo da la imagen
    label->setWidth(160); // Un poco menos que la card para padding
    label->setTextColor(nvgRGB(255,255,255));
    label->setSingleLine(true); // Necesario para animación
    label->setAutoAnimate(false); // Desactivamos auto-animación por foco
    // Cálculo manual del ancho del texto para decidir animación y alineación usando la nueva API pública
    float textWidth = brls::Label::measureTextWidth(label->getFont(), label->getFontSize(), name);
    if (textWidth > 160) {
        label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        label->setAnimated(true); // Animar si se trunca
    } else {
        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label->setAnimated(false);
    }
    box->addView(label);

    this->setCustomContent(box);
    this->label = label;

    this->registerClickAction([this](brls::View*) {
        if (clickCb) clickCb();
        return true;
    });
}



void PCCard::setPCName(const std::string& name) {
    if (label) {
        label->setText(name);
        // Recalcular animación y alineación si cambia el nombre usando la nueva API pública
        float textWidth = brls::Label::measureTextWidth(label->getFont(), label->getFontSize(), name);
        if (textWidth > 160) {
            label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            label->setAnimated(true);
        } else {
            label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            label->setAnimated(false);
        }
    }
}


void PCCard::setPCImage(const std::string& imagePath) {
    if (image) {
        std::string fixedPath = (imagePath == "resources/img/moonlight/pc.png") ? "img/moonlight/pc.png" : imagePath;
        image->setImageFromRes(fixedPath);
    }
}


void PCCard::setClickAction(std::function<void()> cb) {
    clickCb = cb;
}
