#include "view/grid_view.hpp"
#include <borealis/views/label.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/button.hpp>

GridView::GridView() {
    brls::Logger::info("[GridView] Constructor llamado");
    this->setAxis(brls::Axis::COLUMN);
    this->itemNames.clear();
    this->itemIcons.clear();
    this->onItemSelect = nullptr;
}

void GridView::setItems(const std::vector<std::string>& names, const std::vector<std::string>& icons) {
    brls::Logger::info("[GridView] setItems llamado, names.size()=%zu", names.size());
    this->itemNames = names;
    this->itemIcons = icons;
    reload();
}

void GridView::setOnItemSelect(ItemSelectCallback cb) {
    this->onItemSelect = cb;
}

void GridView::reload() {
    brls::Logger::info("[GridView] reload llamado, itemNames.size()=%zu", itemNames.size());
    // Elimina todas las vistas hijas de forma segura
    while (!this->getChildren().empty()) {
        auto* child = this->getChildren().front();
        if (child) {
            brls::Logger::info("[GridView] removeView ejecutado para hijo (ptr)=%s", typeid(*child).name());
            this->removeView(child);
        } else {
            brls::Logger::error("[GridView] ¡Intento de removeView con hijo nullptr!");
            break;
        }
    }
    for (size_t i = 0; i < itemNames.size(); ++i) {
        brls::Logger::info("[GridView] Creando botón para '%s'", itemNames[i].c_str());
        auto* btn = new brls::Button();
        btn->setText(itemNames[i]);
        btn->registerClickAction([this, i](brls::View*) {
            brls::Logger::info("[GridView] Botón pulsado idx=%zu", i);
            if (onItemSelect) onItemSelect(i);
            return true;
        });
        this->addView(btn);
        brls::Logger::info("[GridView] addView ejecutado para '%s'", itemNames[i].c_str());
    }
    if (itemNames.empty()) {
        brls::Logger::info("[GridView] reload: lista vacía, no se añaden botones");
    }
    brls::Logger::info("[GridView] reload finalizado");
}
