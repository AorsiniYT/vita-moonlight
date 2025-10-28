#include <borealis.hpp>
#include "utils/dialog_utils.h"

using namespace brls;

Dialog* createLoadingDialog(const std::string& message) {
    Style style = Application::getStyle();
    brls::Box* holder = new brls::Box(brls::Axis::COLUMN);

    auto* label = new brls::Label();
    label->setText(message);
    label->setFontSize(style["brls/dialog/fontSize"]);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setMarginBottom(21);

    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->View::setSize(brls::Size(92, 92));

    holder->addView(label);
    holder->addView(spinner);
    holder->setAlignItems(brls::AlignItems::CENTER);
    holder->setJustifyContent(brls::JustifyContent::CENTER);
    holder->setPadding(style["brls/dialog/paddingTopBottom"],
                       style["brls/dialog/paddingLeftRight"], 28,
                       style["brls/dialog/paddingLeftRight"]);

    auto* dialog = new brls::Dialog(holder);
    dialog->setCancelable(false);
    dialog->setFocusable(true);
    dialog->setHideHighlight(true);
    dialog->open();
    return dialog;
}

bool updateLoadingDialogText(brls::Dialog* dialog, const std::string& message) {
    if (!dialog) return false;
    // Buscar recursivamente la primera Label dentro del diálogo
    std::function<brls::View*(brls::View*)> findLabel;
    findLabel = [&findLabel](brls::View* v) -> brls::View* {
        if (!v) return nullptr;
        brls::Label* label = dynamic_cast<brls::Label*>(v);
        if (label) return v;
        brls::Box* box = dynamic_cast<brls::Box*>(v);
        if (!box) return nullptr;
        auto ch = box->getChildren();
        for (auto* c : ch) {
            brls::View* found = findLabel(c);
            if (found) return found;
        }
        return nullptr;
    };

    auto children = dialog->getChildren();
    for (auto* c : children) {
        brls::View* found = findLabel(c);
        if (found) {
            brls::Label* lbl = dynamic_cast<brls::Label*>(found);
            if (lbl) {
                lbl->setText(message);
                return true;
            }
        }
    }
    return false;
}
