#pragma once
#include <borealis.hpp>
#include <string>
#include <vector>

// Options to customize dialogs
struct DialogOptions
{
    float contentPadding                = 0.0f; // Content padding (0 = no padding)
    float contentWidth                  = 0.0f; // Content width (0 = auto)
    brls::AlignItems alignItems         = brls::AlignItems::AUTO;
    brls::JustifyContent justifyContent = brls::JustifyContent::FLEX_START;
    bool cancelable                     = true; // Does pressing B close the dialog?
    bool focusable                      = false; // Dialog itself not focusable, focus goes to buttons
    bool hideHighlight                  = false;
};

// Create a custom dialog with Box content
brls::Dialog* createCustomDialog(brls::Box* content, const DialogOptions& options);

// Create a custom label
brls::Label* createLabel(const std::string& text, float fontSize = 20.0f,
    brls::HorizontalAlign align = brls::HorizontalAlign::LEFT,
    float marginBottom          = 0.0f);

// Create an Information Box with rows (key: value)
brls::Box* createInfoBox(const std::vector<std::pair<std::string, std::string>>& rows,
    float fontSize = 20.0f, float rowSpacing = 10.0f);

// Original features
brls::Dialog* createLoadingDialog(const std::string& message);
// Updates the dialog text returned by createLoadingDialog. Returns true if found
// and updated a label inside the dialog.
bool updateLoadingDialogText(brls::Dialog* dialog, const std::string& message);
