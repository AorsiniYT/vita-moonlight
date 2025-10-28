#pragma once
#include <borealis.hpp>

brls::Dialog* createLoadingDialog(const std::string& message);
// Actualiza el texto del diálogo devuelto por createLoadingDialog. Devuelve true si se encontró
// y actualizó una etiqueta dentro del diálogo.
bool updateLoadingDialogText(brls::Dialog* dialog, const std::string& message);
