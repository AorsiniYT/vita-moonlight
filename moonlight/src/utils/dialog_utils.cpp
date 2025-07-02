#include <borealis.hpp>
#include "utils/dialog_utils.h"

using namespace brls;

Dialog* createLoadingDialog(const std::string& message) {
    auto* dialog = new Dialog(message);
    auto* spinner = new ProgressSpinner();
    dialog->setCancelable(false);
    dialog->addView(spinner);
    dialog->open();
    return dialog;
}
