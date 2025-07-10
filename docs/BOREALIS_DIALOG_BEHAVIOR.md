# Borealis: Comportamiento de Diálogos y Botones en PSVita

## Resumen
En Borealis, al usar `Dialog::addButton`, cada botón cierra automáticamente el diálogo al ser pulsado, llamando internamente a `dismiss()`/`close()` antes de ejecutar el callback del botón. No es necesario (ni recomendable) llamar manualmente a `dialog->close()` dentro del callback, ya que esto puede provocar comportamientos inesperados como la aparición del diálogo de "cerrar app" o glitches visuales.

## Flujo correcto para abrir un Dropdown tras cerrar un diálogo
- Simplemente pon la lógica de apertura del dropdown dentro del callback del botón, usando `brls::sync` si necesitas esperar al siguiente frame.
- Ejemplo:

```cpp
dialog->addButton("Settings", [this, host](/*dialog*/) {
    // El diálogo se cerrará automáticamente
    brls::sync([this, host]() {
        // Abrir dropdown aquí
    });
});
```

## Notas adicionales
- El botón de cancelar (B/círculo) puede ser deshabilitado con `setCancelable(false)`, pero esto solo afecta al botón de cancelar, no a los botones añadidos manualmente.
- Si necesitas un botón que NO cierre el diálogo, deberás implementar una solución personalizada (no soportado por defecto en Borealis).

## Problemas comunes
- Llamar manualmente a `dialog->close()` dentro del callback de un botón añadido con `addButton` puede causar que aparezca el menú de "cerrar app" o glitches de navegación.
- Siempre deja que Borealis gestione el cierre del diálogo para los botones estándar.

---

Este archivo documenta el comportamiento real observado y validado en el proyecto Moonlight PSVita.
