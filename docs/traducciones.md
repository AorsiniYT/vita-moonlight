# Guía de manejo de traducciones (i18n) en Moonlight-Vita

## Estructura general
- **Las traducciones de la interfaz de usuario se gestionan principalmente desde los archivos XML** usando claves i18n.
- **Los archivos C++ (.cpp)** solo manipulan las IDs de los elementos definidos en el XML y rara vez usan traducciones directas, salvo para textos dinámicos o notificaciones.

## ¿Dónde se definen los textos traducibles?
- En los archivos XML de la UI, los textos visibles usan claves i18n, por ejemplo:
  ```xml
  <brls:Label text="@i18n/moonlight/welcome" />
  <brls:Button text="@i18n/moonlight/components/button_primary" />
  ```
- Estas claves se resuelven automáticamente al idioma activo usando los archivos JSON de recursos, por ejemplo:
  - `resources/i18n/es/moonlight.json`
  - `resources/i18n/en-US/moonlight.json`

## ¿Cómo funciona la traducción?
- Cuando se infla un XML con `inflateFromXMLRes(...)`, Borealis busca las claves i18n y las reemplaza por el texto traducido según el idioma activo.
- Si la clave no existe en el archivo de idioma, se mostrará la clave literal.

## Traducciones en C++
- Solo se usan para textos dinámicos o notificaciones:
  ```cpp
  #include <borealis/core/i18n.hpp>
  using namespace brls::literals;
  brls::Application::notify("settings/add_host_added"_i18n);
  ```
- Asegúrate de inicializar el sistema de recursos y el idioma antes de usar `_i18n` en C++.

## Buenas prácticas
- **Define todos los textos estáticos en XML usando claves i18n.**
- **Solo usa traducciones en C++ para textos dinámicos.**
- **Mantén sincronizados los archivos JSON de todos los idiomas.**
- **Verifica que la ruta de recursos y el idioma activo sean correctos en tiempo de ejecución.**

## Estructura recomendada de claves i18n
- Todas las claves deben estar anidadas en objetos, nunca usar `/` en los nombres de clave.
- Ejemplo correcto:
  ```json
  {
    "settings": {
      "add_host": "Añadir PC",
      "add_host_desc": "Agrega un nuevo PC para hacer streaming"
      // ...
    },
    "hints": {
      "cancel": "Cancelar"
    }
  }
  ```
- Así, en XML se accede como `@i18n/moonlight/settings/add_host` o `@i18n/moonlight/hints/cancel`.
- Esto evita problemas de resolución y mantiene la estructura clara y escalable.

## Resolución de claves y convención de acceso

- Aunque en el archivo JSON las claves están anidadas (por ejemplo, `"settings": { "add_host": ... }`), **la clave de acceso desde XML o C++ debe incluir el prefijo del archivo** (por ejemplo, `moonlight/settings/add_host`).
- Esto se debe a que Borealis asume que el nombre del archivo JSON es el primer segmento de la clave.
- Ejemplo: para acceder a `"add_host_ip_placeholder"` dentro de `settings` en `moonlight.json`, la clave debe ser:
  - En XML: `@i18n/moonlight/settings/add_host_ip_placeholder`
  - En C++: `brls::getStr("moonlight/settings/add_host_ip_placeholder")` o `"moonlight/settings/add_host_ip_placeholder"_i18n`
- Si la clave no existe o está mal escrita, Borealis mostrará la clave literal.

### Ejemplo práctico
Archivo: `resources/i18n/es/moonlight.json`
```json
{
  "settings": {
    "add_host_ip_placeholder": "Ej: 192.168.1.100"
  }
}
```
Acceso correcto:
```xml
<brls:InputCell placeholder="@i18n/moonlight/settings/add_host_ip_placeholder" />
```
```cpp
std::string placeholder = brls::getStr("moonlight/settings/add_host_ip_placeholder");
```

## Límites de caracteres en campos de entrada

Para que el contador de caracteres y el límite funcionen correctamente en los campos de entrada (`InputCell`), debes inicializarlos usando el método `init` en C++ y especificar el parámetro `maxInputLength`.

**Ejemplo aplicado en la pantalla de Añadir PC:**

```cpp
if (this->ipField)
    this->ipField->init(brls::getStr("moonlight/settings/ip"), "", [](std::string){}, brls::getStr("moonlight/settings/add_host_ip_placeholder"), "", 15); // IP: 15 caracteres
if (this->nameField)
    this->nameField->init(brls::getStr("moonlight/settings/add_host_manual"), "", [](std::string){}, brls::getStr("moonlight/settings/add_host_name_placeholder"), "", 32); // Nombre: 32 caracteres
```

Esto asegura que el contador muestre correctamente el límite (por ejemplo, 0/15 para IP) y evita valores erróneos como 0/0 o 0/1767335271.

> **Nota:** Si solo usas `setPlaceholder` y no llamas a `init`, el límite de caracteres no se aplica correctamente.

---

Cualquier duda, consulta este archivo o revisa los XML y JSON de recursos.
