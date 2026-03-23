#pragma once
#include <vector>
#include <string>
#include <functional>
#include <borealis.hpp>

// Improved GridView for Moonlight PSVita/Windows
// Moonlight-Switch inspired, with focus navigation and limited columns

class GridView : public brls::Box {
public:
    using ItemSelectCallback = std::function<void(int)>;

    GridView();

    // Factory para XMLView
    static brls::View* create() { return new GridView(); }

    // Configure the elements to display (name, optional icon)
    void setItems(const std::vector<std::string>& names, const std::vector<std::string>& icons = {});

    // Callback when an item is selected
    void setOnItemSelect(ItemSelectCallback cb);

    // Set number of columns
    void setColumns(int cols) { this->columns = cols; }

    // Improved focus navigation
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    int columns;
    std::vector<std::string> itemNames;
    std::vector<std::string> itemIcons;
    ItemSelectCallback onItemSelect;
    std::vector<brls::View*> itemViews;

    void reload();
};
