# Guía del Backend de Borealis

Este documento describe la estructura y componentes del backend de la aplicación Borealis, que maneja la lógica y la interacción con los archivos XML de la interfaz de usuario.

## Estructura del Proyecto

```
demo/
├── include/                 # Archivos de encabezado (.hpp)
│   ├── activity/           # Actividades (pantallas principales)
│   ├── tab/                # Pestañas personalizadas
│   └── view/               # Componentes de vista personalizados
├── src/                    # Código fuente (.cpp)
│   ├── activity/          
│   ├── tab/
│   └── view/
├── resource.manifest       # Lista de recursos
└── resource.rc.in         # Configuración de recursos (Windows)
```

## Componentes Principales

### 1. Actividades (Activities)

Las actividades representan pantallas completas en la aplicación. Heredan de `brls::Activity` y se asocian con un archivo XML.

**Ejemplo: MainActivity**

```cpp
// main_activity.hpp
class MainActivity : public brls::Activity {
  public:
    // Especifica el archivo XML asociado a esta actividad
    CONTENT_FROM_XML_RES("activity/main.xml");
};
```

**Características principales:**
- Cada actividad tiene un diseño asociado en XML
- Maneja el ciclo de vida de la pantalla
- Puede contener múltiples pestañas o vistas

### 2. Pestañas (Tabs)

Las pestañas son componentes que se muestran dentro de un `TabFrame`. Cada pestaña puede contener su propia lógica y vista.

#### 2.1 Estructura Básica

```cpp
// components_tab.hpp
class ComponentsTab : public brls::Box {
  public:
    ComponentsTab();
    static brls::View* create();
    
  private:
    // Vinculación de elementos de la interfaz
    BRLS_BIND(brls::Label, progress, "progress");
    BRLS_BIND(brls::Slider, slider, "slider");
    
    // Manejadores de eventos
    bool onPrimaryButtonClicked(brls::View* view);
};
```

#### 2.2 Tipos de Pestañas

1. **Pestaña de Componentes (ComponentsTab)**
   - Muestra componentes de interfaz de usuario básicos
   - Incluye botones, deslizadores, etc.
   - Ejemplo de registro de acciones:
   ```cpp
   // Registrar acción para un botón
   BRLS_REGISTER_CLICK_BY_ID("button_primary", this->onPrimaryButtonClicked);
   
   // Configurar acción personalizada
   button->registerAction(
       "Acción", brls::BUTTON_A, 
       [](brls::View* view) { return true; }, 
       false, false, brls::SOUND_CLICK);
   ```

2. **Pestaña de Configuración (SettingsTab)**
   - Controles para ajustes de la aplicación
   - Incluye interruptores, selectores y campos de entrada
   - Ejemplo de configuración de controles:
   ```cpp
   // Configurar interruptor
   fps->init("Mostrar FPS", brls::Application::getFPSStatus(), 
       [](bool value) {
           brls::Application::setFPSStatus(value);
       });
   
   // Configurar selector
   swapInterval->init("Intervalo de V-Sync", 
       {"0", "1", "2", "3", "4"}, 1,
       [](int selected) {},
       [](int selected) { 
           brls::Application::setSwapInterval(selected); 
       });
   ```

3. **Pestaña de Lista Reciclable (RecyclingListTab)**
   - Muestra listas largas de manera eficiente
   - Recicla vistas para mejorar el rendimiento
   - Ideal para listas con muchos elementos

### 3. Vistas Personalizadas

Las vistas personalizadas heredan de clases base de Borealis (como `brls::Box`) y pueden vincularse a elementos XML.

#### 3.1 Vista con Imagen y Título (CaptionedImage)

```cpp
// captioned_image.hpp
class CaptionedImage : public brls::Box {
  public:
    CaptionedImage();
    static brls::View* create();
    
    // Manejo de eventos de foco
    void onChildFocusGained(brls::View* directChild, brls::View* focusedView) override;
    void onChildFocusLost(brls::View* directChild, brls::View* focusedView) override;

  private:
    // Vinculación con elementos XML
    BRLS_BIND(brls::Image, image, "image");
    BRLS_BIND(brls::Label, label, "label");
};
```

#### 3.2 Vista de Pokémon (PokemonView)

```cpp
// pokemon_view.hpp
class PokemonView : public brls::Box {
  public:
    PokemonView(Pokemon pokemon);
    PokemonView() : PokemonView(Pokemon("001", "Ejemplo")) {}
    static brls::View* create();

  private:
    Pokemon pokemon;
    BRLS_BIND(brls::Image, image, "image");
    BRLS_BIND(brls::Label, description, "description");
};
```

**Características de las Vistas Personalizadas:**
- Heredan de `brls::Box` u otros controles base
- Usan `BRLS_BIND` para vincular elementos XML
- Pueden manejar eventos de entrada y foco
- Deben implementar un método estático `create()`

## Vinculación con XML

### 1. Registro de Componentes

Los componentes personalizados deben registrarse en `main.cpp`:

```cpp
brls::Application::registerXMLView("CaptionedImage", CaptionedImage::create);
```

### 2. Uso en XML

```xml
<CaptionedImage
    image="@res/img/example.png"
    caption="Ejemplo"
    imageWidth="200"
    imageHeight="150"/>
```

## Flujo de la Aplicación

1. **Inicialización**
   - Se configura el logger y el tema
   - Se registran los componentes personalizados
   - Se crea la ventana principal

2. **Ciclo Principal**
   - Se procesan eventos de entrada
   - Se actualiza la lógica de la aplicación
   - Se renderiza la interfaz

## Manejo de Recursos

### 1. Archivo resource.manifest

Lista todos los recursos que deben empaquetarse con la aplicación:

```
# Archivos de interfaz
resources/xml/activity/main.xml
resources/xml/views/*.xml

# Imágenes
resources/img/*.png
```

### 2. Archivo resource.rc.in (Windows)

Configura los recursos de la aplicación en Windows, como el icono:

```rc
IDI_ICON1 ICON DISCARDABLE "${RESOURCE_DIR}/icon.ico"
```

## Buenas Prácticas

1. **Organización del Código**
   - Mantener la lógica separada de la interfaz
   - Usar nombres descriptivos para componentes y variables
   - Documentar las clases y métodos principales

2. **Manejo de Memoria**
   - Usar punteros inteligentes cuando sea necesario
   - Liberar recursos en los destructores
   - Evitar fugas de memoria en bucles

3. **Rendimiento**
   - Minimizar las operaciones costosas en el hilo principal
   - Usar caché para recursos pesados
   - Optimizar el renderizado de listas largas

## Depuración

### Niveles de Log

```cpp
brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
brls::Logger::debug("Mensaje de depuración");
brls::Logger::info("Mensaje informativo");
brls::Logger::warning("Advertencia");
brls::Logger::error("Error");
```

### Depuración Visual

Habilitar la capa de depuración:
```cpp
brls::Application::enableDebuggingView(true);
```

## Internacionalización

Los textos se cargan desde archivos JSON en `resources/i18n/`:

```cpp
// En el código
brls::Application::getI18nString("tabs/components");

// En XML
<Label text="@i18n/tabs/components"/>
```

## Plataformas Soportadas

- Nintendo Switch (libnx)
- PC (Windows, Linux, macOS)
- Android
- iOS
- PlayStation Vita
- PlayStation 4

## Compilación

El proyecto usa CMake como sistema de construcción. Ejemplo básico:

```bash
mkdir build && cd build
cmake ..
make
```

## Recursos Adicionales

- [Documentación de Borealis](https://github.com/natinusala/borealis)
- [Guía de estilo de C++](https://google.github.io/styleguide/cppguide.html)
- [Documentación de CMake](https://cmake.org/documentation/)
