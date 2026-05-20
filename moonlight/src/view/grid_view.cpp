#include "view/grid_view.hpp"
#include <borealis/views/label.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/button.hpp>
#include "view/pccard.hpp"  // Add include for PCCard

GridView::GridView() {
    brls::Logger::info("[GridView] Constructor llamado");
    this->setAxis(brls::Axis::COLUMN);  // Switch to column for rows
    this->columns = 4;  // 4 columns by default for PS Vita
    this->itemNames.clear();
    this->itemIcons.clear();
    this->onItemSelect = nullptr;
}

void GridView::setItems(const std::vector<std::string>& names, const std::vector<std::string>& icons) {
    brls::Logger::info("[GridView] setItems llamado, names.size()={}", names.size());
    this->itemNames = names;
    this->itemIcons = icons;
    reload();
}

void GridView::setOnItemSelect(ItemSelectCallback cb) {
    this->onItemSelect = cb;
}

void GridView::setItemIcon(int index, const std::string& iconPath) {
    if (index >= 0 && index < (int)itemViews.size()) {
        PCCard* card = dynamic_cast<PCCard*>(itemViews[index]);
        if (card) {
            card->setPCImage(iconPath);
        }
    }
}

void GridView::reload() {
    brls::Logger::info("[GridView] reload llamado, itemNames.size()={}", itemNames.size());

    // Clean existing views
    this->clearViews();
    itemViews.clear();

    if (itemNames.empty()) {
        brls::Logger::info("[GridView] reload: lista vacía, no se añaden elementos");
        return;
    }

    // Create row containers based on number of columns
    size_t currentRow = 0;
    brls::Box* currentRowBox = nullptr;

    for (size_t i = 0; i < itemNames.size(); ++i) {
        // Create new row if necessary
        if (i % columns == 0) {
            currentRowBox = new brls::Box(brls::Axis::ROW);
            currentRowBox->setAlignItems(brls::AlignItems::STRETCH);
            this->addView(currentRowBox);
            currentRow++;
        }

        // Create PCCard for each item
        auto* card = new PCCard(itemNames[i], itemIcons.empty() ? "img/moonlight/pc.png" : itemIcons[i]);

        // Set click action
        card->setClickAction([this, i]() {
            brls::Logger::info("[GridView] Elemento seleccionado idx={}", i);
            if (onItemSelect) onItemSelect(i);
        });

        // Make focusable and configure navigation
        card->setFocusable(true);

        // Add margins for separation
        if (i % columns != 0) {
            card->setMarginLeft(12);  // Horizontal separation between elements
        }

        // Add to current row
        currentRowBox->addView(card);
        itemViews.push_back(card);

    brls::Logger::info("[GridView] Elemento añadido: '{}' en fila {}, columna {}",
              itemNames[i], currentRow, (i % columns) + 1);
    }

    brls::Logger::info("[GridView] reload finalizado, {} filas creadas", currentRow);
}

brls::View* GridView::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (!currentView || itemViews.empty())
        return nullptr;

    // Find the index of the current element
    auto it = std::find(itemViews.begin(), itemViews.end(), currentView);
    if (it == itemViews.end())
        return nullptr;

    size_t currentIndex = std::distance(itemViews.begin(), it);
    size_t totalItems = itemViews.size();
    int currentRow = currentIndex / columns;
    int currentCol = currentIndex % columns;
    int totalRows = (totalItems + columns - 1) / columns;

    brls::View* nextView = nullptr;

    switch (direction) {
        case brls::FocusDirection::RIGHT:
            if (currentCol < columns - 1 && currentIndex + 1 < totalItems) {
                nextView = itemViews[currentIndex + 1];
            }
            break;

        case brls::FocusDirection::LEFT:
            if (currentCol > 0) {
                nextView = itemViews[currentIndex - 1];
            }
            break;

        case brls::FocusDirection::DOWN:
            if (currentRow < totalRows - 1) {
                int nextRowIndex = (currentRow + 1) * columns + currentCol;
                if (nextRowIndex < (int)totalItems) {
                    nextView = itemViews[nextRowIndex];
                } else {
                    // If there is no element in this column, go to the last in the row
                    int lastInRow = std::min((int)totalItems - 1, (currentRow + 1) * columns + columns - 1);
                    nextView = itemViews[lastInRow];
                }
            }
            break;

        case brls::FocusDirection::UP:
            if (currentRow > 0) {
                int prevRowIndex = (currentRow - 1) * columns + currentCol;
                if (prevRowIndex < (int)totalItems) {
                    nextView = itemViews[prevRowIndex];
                }
            }
            break;

        default:
            break;
    }

    return nextView;
}
