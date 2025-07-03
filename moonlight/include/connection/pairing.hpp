#pragma once
#include <string>
#include <atomic>
#include <functional>

// Permite detener cualquier pairing activo de forma global (como stopVitaDiscovery)
void stopPairing();

// Muestra el proceso de pairing con un popup interactivo
// El callback recibe true si el pairing fue exitoso, false si falló
void startMoonlightPairingWithPopup(const std::string& hostIp, const std::string& hostName, std::function<void(bool)> onFinished);

// Nueva versión: permite pasar un diálogo externo y un flag de cancelación
// El pairing se puede cancelar desde fuera y nunca crea su propio diálogo
namespace brls { class Dialog; }
namespace brls { class Label; }
namespace brls { class ProgressSpinner; }
void startMoonlightPairingWithPopupCustomDialog(const std::string& hostIp, const std::string& hostName, brls::Dialog* dialog, std::atomic<bool>* cancelled, std::function<void(bool)> onFinished, brls::Label* label, brls::ProgressSpinner* spinner);
