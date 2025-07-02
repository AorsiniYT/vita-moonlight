# Guía de Borealis: Biblioteca de Interfaz de Usuario

Borealis es una biblioteca de interfaz de usuario moderna y flexible diseñada para aplicaciones en consolas como Nintendo Switch, PS4, PSVita y más. Esta guía te mostrará cómo utilizar sus características principales.

## Tabla de Contenidos
1. [Estructura del Proyecto](#estructura-del-proyecto)
2. [Conceptos Básicos](#conceptos-básicos)
3. [Componentes de la Interfaz](#componentes-de-la-interfaz)
4. [Manejo de Eventos](#manejo-de-eventos)
5. [Animaciones](#animaciones)
6. [Temas y Estilos](#temas-y-estilos)
7. [Internacionalización](#internacionalización)
8. [Ejemplos Prácticos](#ejemplos-prácticos)
9. [Plataformas Soportadas](#plataformas-soportadas)

## Estructura del Proyecto

La estructura típica de un proyecto con Borealis es:

```
mi_proyecto/
├── demo/                 # Código de demostración
│   ├── include/         # Encabezados
│   └── src/             # Código fuente
├── library/             # Biblioteca Borealis
└── resources/           # Recursos (imágenes, fuentes, etc.)
```

## Conceptos Básicos

### Inicialización

Para comenzar a usar Borealis, necesitas inicializar la aplicación:

```cpp
#include <borealis.hpp>

int main()
{
    // Inicializar la aplicación
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);
    
    if (!brls::Application::init()) {
        brls::Logger::error("No se pudo inicializar Borealis");
        return EXIT_FAILURE;
    }
    
    // Crear y mostrar la ventana principal
    brls::Window* window = new brls::Window("Mi Aplicación");
    window->setContentView(new brls::Label("¡Hola, Mundo!"));
    
    // Ejecutar el bucle principal
    while (brls::Application::mainLoop());
    
    return EXIT_SUCCESS;
}
```

## Componentes de la Interfaz

### Etiquetas (Labels)

```cpp
auto* label = new brls::Label(
    brls::LabelStyle::REGULAR,
    "¡Hola, Mundo!",
    true
);
```

### Botones

```cpp
auto* button = new brls::Button("Presionar");
button->setParent(view);
button->setSize(200, 60);
button->setPosition(100, 100);
button->setAction([](View* view) {
    brls::Application::notify("¡Botón presionado!");
    return true;
});
```

### Listas

```cpp
auto* list = new brls::List();

// Añadir elementos a la lista
list->addView(new brls::Header("Sección 1"));
list->addView(new brls::Button("Elemento 1"));
list->addView(new brls::Button("Elemento 2"));

// Añadir lista a la vista principal
window->setContentView(list);
```

## Manejo de Eventos

### Gestos Táctiles

```cpp
view->addGestureRecognizer(new brls::TapGestureRecognizer(
    [](brls::TapGestureStatus status, brls::Sound* soundToPlay) {
        if (status.state() == brls::GestureState::END) {
            // Código al tocar la pantalla
        }
        return true;
    }));
```

### Eventos de Teclado/Gamepad

```cpp
view->registerAction("Aceptar", brls::Key::A, [](View* view) {
    brls::Application::notify("Botón A presionado");
    return true;
});
```

## Animaciones

### Animaciones Básicas

```cpp
brls::Animatable positionX = 0.0f;
brls::Animatable opacity = 0.0f;

// Animar posición X
auto animX = new brls::Animation(
    brls::EasingFunction::quadraticOut,
    [&positionX](float value) {
        positionX = value;
        view->setTranslationX(positionX);
    }, 0.0f, 100.0f, 500  // De 0 a 100 en 500ms
);

// Animar opacidad
auto animOpacity = new brls::Animation(
    brls::EasingFunction::quadraticOut,
    [&opacity](float value) {
        opacity = value;
        view->setAlpha(opacity);
    }, 0.0f, 1.0f, 300  // Fundido de entrada
);

// Iniciar animaciones
brls::Application::getAnimationManager()->start(animX);
brls::Application::getAnimationManager()->start(animOpacity);
```

## Temas y Estilos

### Definición de Estilos

```cpp
// En tu archivo de recursos o inicialización
brls::Style style = brls::Application::getStyle();

// Personalizar colores
style.Colors[brls::ThemeColor::BACKGROUND] = nvgRGB(20, 20, 30);
style.Colors[brls::ThemeColor::TEXT] = nvgRGB(255, 255, 255);

// Personalizar dimensiones
style.AppletFrame.headerHeight = 60;
style.AppletFrame.footerHeight = 60;

// Aplicar el estilo
brls::Application::setStyle(style);
```

## Internacionalización

### Archivo de Idiomas

Crea un archivo JSON para las traducciones (ej. `es_ES.json`):

```json
{
    "app/title": "Mi Aplicación",
    "app/greeting": "¡Hola, {name}!",
    "menu/start": "Comenzar",
    "menu/settings": "Ajustes"
}
```

### Uso en Código

```cpp
// Cargar archivo de idioma
brls::i18n::loadTranslations("es_ES");

// Obtener texto traducido
std::string title = brls::i18n::getStr("app/title");

// Con parámetros
std::string greeting = brls::i18n::getStr("app/greeting", {"name": "Usuario"});
```

## Ejemplos Prácticos

### Pantalla de Login

```cpp
class LoginView : public brls::View {
public:
    LoginView() {
        // Configurar diseño
        this->setLayout(new brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL));
        
        // Título
        auto* title = new brls::Label("Iniciar Sesión");
        title->setFontSize(32);
        this->addView(title);
        
        // Campo de usuario
        auto* userField = new brls::TextInput();
        userField->setPlaceholder("Usuario");
        this->addView(userField);
        
        // Campo de contraseña
        auto* passField = new brls::TextInput();
        passField->setPlaceholder("Contraseña");
        passField->setInputType(brls::TextInputType::PASSWORD);
        this->addView(passField);
        
        // Botón de inicio de sesión
        auto* loginBtn = new brls::Button("Iniciar Sesión");
        loginBtn->setAction([userField, passField](View* view) {
            // Validar credenciales
            if (userField->getValue().empty() || passField->getValue().empty()) {
                brls::Application::notify("Por favor complete todos los campos");
                return true;
            }
            
            // Lógica de inicio de sesión...
            brls::Application::notify("¡Bienvenido!");
            return true;
        });
        
        this->addView(loginBtn);
    }
};
```

### Lista con Elementos Personalizados

```cpp
class CustomListItem : public brls::Box {
public:
    CustomListItem(const std::string& title, const std::string& subtitle) {
        this->setHeight(80);
        this->setPadding(20);
        
        // Contenedor principal
        auto* container = new brls::BoxLayout(brls::BoxLayoutOrientation::HORIZONTAL);
        
        // Icono
        auto* icon = new brls::Rectangle();
        icon->setSize(40, 40);
        icon->setCornerRadius(20);
        icon->setColor(nvgRGB(66, 165, 245));
        
        // Contenedor de texto
        auto* textContainer = new brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL);
        textContainer->setPaddingLeft(20);
        
        auto* titleLabel = new brls::Label(title);
        titleLabel->setFontSize(20);
        
        auto* subtitleLabel = new brls::Label(subtitle);
        subtitleLabel->setFontSize(14);
        subtitleLabel->setTextColor(nvgRGB(150, 150, 150));
        
        textContainer->addView(titleLabel);
        textContainer->addView(subtitleLabel);
        
        container->addView(icon);
        container->addView(textContainer);
        
        this->addView(container);
    }
};

// Uso en una lista
void populateList(brls::List* list) {
    list->addView(new brls::Header("Elementos Personalizados"));
    
    // Añadir elementos a la lista
    for (int i = 0; i < 10; i++) {
        auto* item = new brls::ListItem(
            new CustomListItem(
                "Elemento " + std::to_string(i+1),
                "Descripción del elemento " + std::to_string(i+1)
            )
        );
        
        item->setAction([i](View* view) {
            brls::Application::notify("Seleccionado: " + std::to_string(i+1));
            return true;
        });
        
        list->addView(item);
    }
}
```

## Plataformas Soportadas

Borealis es compatible con múltiples plataformas:

- **Nintendo Switch**
- **PlayStation 4**
- **PlayStation Vita**
- **Escritorio (Windows/macOS/Linux)**
- **iOS/Android** (experimental)

### Consideraciones Específicas por Plataforma

#### PlayStation Vita
- Requiere firmware 3.60+ con HENkaku/h-encore
- Soporte para controles táctiles y físicos
- Optimizado para la pantalla de 960x544

#### Nintendo Switch
- Soporte para controles Joy-Con y Pro Controller
- Modo portátil y TV
- Soporte para HD Rumble

## Recursos Adicionales

- [Repositorio Oficial de Borealis](https://github.com/xfangfang/borealis)
- [Documentación de la API](https://xfangfang.github.io/borealis/)
- [Ejemplos y Demos](https://github.com/xfangfang/borealis-examples)

---

Esta guía cubre los conceptos básicos para comenzar con Borealis. Para obtener más información, consulta la documentación oficial y los ejemplos proporcionados en el repositorio.
