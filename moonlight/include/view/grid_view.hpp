#pragma once
#include <vector>
#include <string>
#include <functional>
#include <borealis.hpp>

// GridView simple para Moonlight PSVita/Windows
// Inspirado en Moonlight-Switch, pero minimalista y portable

class GridView : public brls::Box {
public:
    using ItemSelectCallback = std::function<void(int)>;

    GridView();

    // Factory para XMLView
    static brls::View* create() { return new GridView(); }

    // Configura los elementos a mostrar (nombre, icono opcional)
    void setItems(const std::vector<std::string>& names, const std::vector<std::string>& icons = {});

    // Callback cuando se selecciona un elemento
    void setOnItemSelect(ItemSelectCallback cb);

private:
    std::vector<std::string> itemNames;
    std::vector<std::string> itemIcons;
    ItemSelectCallback onItemSelect;

    void reload();
};
