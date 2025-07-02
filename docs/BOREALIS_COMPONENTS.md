# Mapa de Componentes Borealis

Este documento describe los componentes de la interfaz de usuario de Borealis utilizados en el proyecto, junto con sus atributos comunes y ejemplos de uso.

## Estructura Básica

### AppletFrame
El contenedor principal de la aplicación.

**Atributos comunes:**
- `iconInterpolation`: Interpolación del ícono (ej: "linear")
- `footerHidden`: Muestra/oculta el pie de página (true/false)

**Ejemplo:**
```xml
<brls:AppletFrame
    iconInterpolation="linear"
    footerHidden="false">
    <!-- Contenido -->
</brls:AppletFrame>
```

### TabFrame
Contenedor de pestañas.

**Atributos comunes:**
- `title`: Título mostrado en la barra superior
- `icon`: Ruta al ícono

**Ejemplo:**
```xml
<brls:TabFrame
    title="@i18n/demo/title"
    icon="@res/img/icon.png">
    <!-- Pestañas -->
</brls:TabFrame>
```

### Tab
Pestaña individual dentro de un TabFrame.

**Atributos comunes:**
- `label`: Etiqueta de la pestaña

**Ejemplo:**
```xml
<brls:Tab label="@i18n/demo/tabs/components">
    <!-- Contenido de la pestaña -->
</brls:Tab>
```

## Contenedores

### Box
Contenedor flexible para agrupar elementos.

**Atributos comunes:**
- `width`: Ancho (ej: "auto", "100%", "200px")
- `height`: Alto
- `axis`: Dirección de los elementos hijos ("row" o "column")
- `paddingLeft`, `paddingRight`, `paddingTop`, `paddingBottom`: Relleno
- `marginBottom`: Margen inferior
- `alignItems`: Alineación de elementos ("center", "flex-start", "flex-end")

**Ejemplo:**
```xml
<brls:Box
    width="auto"
    height="auto"
    axis="row"
    alignItems="center">
    <!-- Elementos -->
</brls:Box>
```

### ScrollingFrame
Área desplazable.

**Atributos comunes:**
- `width`: Ancho
- `height`: Alto
- `grow`: Factor de crecimiento (ej: "1.0")

**Ejemplo:**
```xml
<brls:ScrollingFrame
    width="auto"
    height="auto"
    grow="1.0">
    <!-- Contenido desplazable -->
</brls:ScrollingFrame>
```

### HScrollingFrame
Área desplazable horizontalmente.

**Ejemplo:**
```xml
<brls:HScrollingFrame
    width="auto"
    height="240"
    grow="1.0">
    <!-- Contenido horizontal -->
</brls:HScrollingFrame>
```

## Componentes de Interfaz

### Label
Muestra texto.

**Atributos comunes:**
- `text`: Texto a mostrar
- `fontSize`: Tamaño de fuente
- `horizontalAlign`: Alineación horizontal ("left", "center", "right")
- `singleLine`: Si es true, el texto se muestra en una sola línea

**Ejemplo:**
```xml
<brls:Label
    text="@i18n/demo/welcome"
    fontSize="24"
    horizontalAlign="center"/>
```

### Header
Encabezado de sección.

**Atributos comunes:**
- `title`: Título del encabezado
- `marginBottom`: Margen inferior
- `paddingTop`, `paddingBottom`: Relleno superior e inferior

**Ejemplo:**
```xml
<brls:Header
    title="Sección de Configuración"
    marginBottom="20px"/>
```

### Button
Botón interactivo.

**Atributos comunes:**
- `text`: Texto del botón
- `style`: Estilo (ej: "primary", "secondary")
- `onClick`: Acción al hacer clic
- `width`, `height`: Dimensiones del botón

**Ejemplo:**
```xml
<brls:Button
    text="@i18n/common/button_ok"
    style="primary"
    onClick="onOkClick"/>
```

### Separator
Línea separadora.

**Atributos comunes:**
- `marginTop`, `marginBottom`: Márgenes

**Ejemplo:**
```xml
<brls:Separator marginTop="10px" marginBottom="10px"/>
```

## Componentes de Entrada

### BooleanCell
Interruptor de encendido/apagado.

**Atributos comunes:**
- `title`: Título descriptivo
- `value`: Valor actual (true/false)
- `onChange`: Función llamada al cambiar el valor

**Ejemplo:**
```xml
<brls:BooleanCell
    title="Modo depuración"
    value="true"
    onChange="onDebugChange"/>
```

### SelectorCell
Selector desplegable.

**Atributos comunes:**
- `title`: Título descriptivo
- `values`: Lista de valores disponibles
- `selected`: Índice del valor seleccionado
- `onSelect`: Función llamada al seleccionar un valor

**Ejemplo:**
```xml
<brls:SelectorCell
    title="Idioma"
    values="Español,Inglés,Francés"
    selected="0"
    onSelect="onLanguageSelect"/>
```

### InputCell
Campo de entrada de texto.

**Atributos comunes:**
- `title`: Etiqueta del campo
- `value`: Valor actual
- `placeholder`: Texto de marcador de posición
- `onChange`: Función llamada al cambiar el valor

**Ejemplo:**
```xml
<brls:InputCell
    title="Nombre de usuario"
    placeholder="Ingrese su nombre"
    onChange="onUsernameChange"/>
```

### InputNumericCell
Campo de entrada numérica.

**Atributos comunes:**
- `title`: Etiqueta del campo
- `value`: Valor numérico actual
- `min`, `max`: Valores mínimo y máximo
- `onChange`: Función llamada al cambiar el valor

**Ejemplo:**
```xml
<brls:InputNumericCell
    title="Edad"
    min="0"
    max="120"
    onChange="onAgeChange"/>
```

### SliderCell
Control deslizante.

**Atributos comunes:**
- `title`: Etiqueta del control
- `value`: Valor actual
- `min`, `max`: Rango de valores
- `onChange`: Función llamada al cambiar el valor

**Ejemplo:**
```xml
<brls:SliderCell
    title="Volumen"
    min="0"
    max="100"
    value="50"
    onChange="onVolumeChange"/>
```

### DetailCell
Elemento de lista con título y valor.

**Atributos comunes:**
- `title`: Título del elemento
- `detail`: Valor mostrado a la derecha
- `onClick`: Función llamada al hacer clic

**Ejemplo:**
```xml
<brls:DetailCell
    title="Versión"
    detail="1.0.0"
    onClick="onVersionClick"/>
```

### RadioCell
Botón de opción.

**Atributos comunes:**
- `title`: Etiqueta del botón
- `selected`: Si está seleccionado
- `groupName`: Nombre del grupo al que pertenece
- `onClick`: Función llamada al hacer clic

**Ejemplo:**
```xml
<brls:RadioCell
    title="Opción 1"
    groupName="opciones"
    selected="true"
    onClick="onOption1Click"/>
```

## Componentes Personalizados

### CaptionedImage
Imagen con título (componente personalizado).

**Atributos comunes:**
- `image`: Ruta a la imagen
- `caption`: Texto del título
- `imageWidth`, `imageHeight`: Dimensiones de la imagen

**Ejemplo:**
```xml
<CaptionedImage
    image="@res/img/example.png"
    caption="Ejemplo de imagen"
    imageWidth="200"
    imageHeight="150"/>
```

## Estilos Comunes

### Unidades de Medida
- `px`: Píxeles (ej: "10px")
- `%`: Porcentaje (ej: "100%")
- `auto`: Tamaño automático

### Referencias a Recursos
- `@res/`: Prefijo para recursos de la aplicación
- `@i18n/`: Prefijo para cadenas de internacionalización
- `@style/`: Referencia a estilos definidos

## Manejo de Imágenes y Recursos Gráficos

El proyecto utiliza un sistema organizado para manejar imágenes y recursos gráficos, con soporte para diferentes resoluciones y temas (claro/oscuro).

### Estructura de Directorios

```
resources/
├── img/                    # Imágenes de la aplicación
│   ├── borealis_96.png     # Ícono de la aplicación
│   ├── borealis_256.png    # Logo grande
│   ├── tiles.png           # Imágenes de ejemplo
│   └── pokemon/           # Sprites de Pokémon
│       ├── 001.png        # Imágenes completas
│       └── thumbnails/    # Miniaturas
│           └── 001.png    
└── sys/                   # Iconos del sistema
    ├── wifi_0_light.png   # Iconos de WiFi (tema claro)
    ├── wifi_0_dark.png    # Iconos de WiFi (tema oscuro)
    ├── ethernet_light.png # Iconos de red
    └── battery_*.png      # Estados de batería
```

### Componentes para Imágenes

#### Image
Componente básico para mostrar imágenes.

**Atributos comunes:**
- `image`: Ruta a la imagen (requerido)
- `width`, `height`: Dimensiones de la imagen
- `imageWidth`, `imageHeight`: Tamaño de renderizado
- `scalingType`: Tipo de escalado ("downscale", "upscale", "stretch", "crop")
- `focusable`: Si la imagen puede recibir foco

**Ejemplo:**
```xml
<brls:Image
    image="@res/img/example.png"
    width="200"
    height="150"
    scalingType="downscale"
    focusable="true"/>
```

#### CaptionedImage
Componente personalizado que muestra una imagen con un título debajo.

**Atributos comunes:**
- `image`: Ruta a la imagen
- `caption`: Texto descriptivo
- `imageWidth`, `imageHeight`: Tamaño de la imagen
- `captionColor`: Color del texto (referencia a tema)

**Ejemplo:**
```xml
<CaptionedImage
    image="@res/img/example.png"
    caption="Imagen de ejemplo"
    imageWidth="200"
    imageHeight="150"/>
```

### Rutas de Recursos

Las imágenes se referencian usando el prefijo `@res/` seguido de la ruta relativa desde el directorio `resources`:

```xml
<!-- Imagen en la raíz de img/ -->
<Image image="@res/img/icon.png"/>

<!-- Imagen en subdirectorio -->
<Image image="@res/img/pokemon/001.png"/>

<!-- Imagen con tema -->
<Image image="@res/img/sys/wifi_3_${theme}.png"/>
```

### Temas y Variantes

El soporte para temas claro/oscuro se maneja mediante sufijos en los nombres de archivo:
- `_light.png` para tema claro
- `_dark.png` para tema oscuro

**Ejemplo de uso:**
```xml
<Image image="@res/img/sys/icon_${theme}.png"/>
```

### Buenas Prácticas

1. **Organización**:
   - Usar subdirectorios lógicos para organizar las imágenes
   - Mantener nombres descriptivos y consistentes
   - Agrupar variantes de la misma imagen con sufijos claros

2. **Optimización**:
   - Usar el tamaño mínimo necesario
   - Considerar el uso de sprites para conjuntos de iconos pequeños
   - Optimizar imágenes para reducir el tamaño del paquete

3. **Internacionalización**:
   - Evitar texto en las imágenes
   - Usar el sistema de internacionalización para textos
   - Proporcionar versiones alternativas para diferentes idiomas si es necesario

4. **Accesibilidad**:
   - Siempre proporcionar alternativas de texto
   - Asegurar suficiente contraste en iconos
   - Probar con diferentes temas y tamaños de fuente

## Sistema de Internacionalización (i18n)

El proyecto utiliza un sistema de archivos JSON para la internacionalización, siguiendo la estructura de carpetas `i18n/[código-de-idioma]/[archivo].json`.

### Estructura de Archivos

```
resources/
└── i18n/
    ├── en-US/              # Inglés (Estados Unidos)
    │   ├── demo.json       # Textos de la aplicación
    │   ├── hints.json      # Sugerencias y ayudas
    │   └── scroll.json     # Textos relacionados con desplazamiento
    ├── fr/                 # Francés
    └── zh-Hans/            # Chino simplificado
```

### Uso en XML

Para usar textos traducidos en los archivos XML, se utiliza el prefijo `@i18n/` seguido de la ruta al texto:

```xml
<brls:Label text="@i18n/demo/welcome"/>
```

### Estructura de los Archivos JSON

Cada archivo de idioma sigue una estructura jerárquica. Por ejemplo, `demo.json`:

```json
{
    "title": "Borealis Demo App",
    "tabs": {
        "components": "Componentes Básicos",
        "settings": "Configuración"
    },
    "welcome": "¡Bienvenido a la demo de Borealis!",
    "components": {
        "buttons_header": "Botones",
        "button_primary": "Botón Primario"
    }
}
```

### Referenciando Textos

- Para acceder a `title`: `@i18n/demo/title`
- Para acceder a `components.buttons_header`: `@i18n/demo/components/buttons_header`
- Para acceder a `tabs.components`: `@i18n/demo/tabs/components`

### Idiomas Soportados

- `en-US`: Inglés (Estados Unidos) - Completo
- `zh-Hans`: Chino simplificado - Parcial
- `fr`: Francés - Vacío (por implementar)
- `ru`: Ruso - Solo textos del sistema

### Agregar un Nuevo Idioma

1. Crear una nueva carpeta con el código del idioma (ej: `es-ES` para español de España)
2. Copiar los archivos JSON de `en-US` como base
3. Traducir los valores manteniendo las claves
4. Asegurarse de mantener la misma estructura jerárquica

### Mejores Prácticas

1. Usar claves descriptivas y jerárquicas
2. Mantener consistencia en el formato
3. No incluir signos de puntuación en las claves
4. Usar archivos separados para diferentes dominios de la aplicación
5. Documentar las claves en el archivo en inglés

## Convenciones de Nombrado
- Los nombres de componentes personalizados usan notación PascalCase
- Los atributos usan camelCase
- Las referencias a recursos usan rutas relativas con prefijos
- Las claves de internacionalización usan snake_case

## Notas de Implementación
- Los componentes pueden anidarse según sea necesario
- El orden de los atributos no es significativo
- Los estilos pueden heredarse de componentes padres

---
Este documento se actualizará a medida que se agreguen más componentes o se descubran nuevos atributos.
