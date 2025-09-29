# Render de Video con Borealis (Plan Zero-Copy)

Este documento describe la arquitectura de renderizado de video en PlayStation Vita para Moonlight, cubriendo:
- Estado actual (fallback memcpy)
- Objetivo zero-copy integrado con Borealis/NanoVG
- Estructuras y flujo de doble buffer
- Estrategia de fallback y detección de pitch
- Métricas e instrumentación previstas

---
## 1. Objetivos

| Objetivo | Descripción | Métrica de Éxito |
|----------|-------------|------------------|
| Reducir latencia | Eliminar copia CPU RGBA intermedia | -0.4 ms (≥720p) |
| Simplificar pipeline | Unificar bajo NanoVG/Borealis | Un solo path draw |
| Robustez | Fallback automático si falla zero-copy | 0 crashes / 1000 frames |
| Extensibilidad | Base para futuro YUV + shader | Código modular con `VideoPlane` |

---
## 2. Componentes Principales

| Componente | Rol |
|------------|-----|
| Decoder (sceAvcdec) | Produce frames decodificados (RGBA/NV12 futuro) |
| VideoPlane | Encapsula buffer phycont + textura GXM + imageId NanoVG |
| Doble Buffer (front/back) | Evita que GPU lea mientras se escribe próximo frame |
| NanoVG (backend GXM) | Composición final dentro de Borealis |
| Fallback memcpy | Ruta segura actual (copy → textura vita2d) |
| Instrumentación | Logs, tiempos, contadores de errores |

---
## 3. Estado Actual (Fallback Memcpy)

```
+-----------+      +----------------------+      +------------------+      +-----------+
| Decoder   | ---> | Buffer Físico Linear | ---> | memcpy (CPU RGBA) | ---> | Textura   |
| (Avcdec)  |      | (phycont intermedio) |      |                  |      | vita2d    |
+-----------+      +----------------------+      +------------------+      +-----------+
                                                              |
                                                              v
                                                      +----------------+
                                                      | GPU Present    |
                                                      +----------------+
```
Características:
- 1 copia CPU por frame (intermedio → textura vita2d)
- Doble buffer implementado solo en destino final (FRONT/BACK de textura)
- Render fuera de Borealis (composición separada)

---
## 4. Objetivo Zero-Copy (Arquitectura Meta)

```
          (sin copia CPU)
+-----------+        +------------------+        +-----------------------+        +----------------+
| Decoder   |  --->  | VideoPlane[BACK] |  swap  | VideoPlane[FRONT]     |  --->  | NanoVG/Borealis|
| (Avcdec)  |        | (phycont + GXM)  | <----> | (solo lectura GPU)    |        | (draw pattern) |
+-----------+        +------------------+        +-----------------------+        +----------------+
```
Notas:
- Decoder escribe directamente en la memoria que GPU ya puede texturizar.
- NanoVG usa `nvgImagePattern` sobre `imageId` asociado al plano FRONT.
- Eliminada la textura vita2d intermedia.

---
## 5. Flujo de Doble Buffer (Zero-Copy)

```
Inicial: front = 0, back = 1

Frame N:
  Decoder -> planes[back]
  swap(front, back)
  Render usa planes[front]

Secuencia temporal:

   Tiempo --->

   Write:   [----N----]        [----N+1--]        [----N+2--]
             to back(1)         to back(0)          to back(1)

   Read :        front(0)            front(1)            front(0)
```
Garantía: No se escribe en el buffer que el GPU está leyendo (salvo frame pacing patológico). Posible extensión: triple buffer.

---
## 6. Diagrama de Estados Simplificado

```
              +------------------+
              |  ZeroCopy Ready? |
              +---------+--------+
                        | yes
                        v
                +---------------+
                |  Decode OK?   |
                +---+-------+---+
                    |       |
                 yes|       |no (error / pitch / mem)
                    v       v
        +----------------+  +---------------------------+
        | Swap & Present |  | Increment fail counter    |
        +-------+--------+  | If >= threshold ->        |
                |           |  Activate Fallback        |
                v           +---------------------------+
        (Loop next frame)
```

---
## 7. Estructura `VideoPlane` (Propuesta)

```c++
struct VideoPlane {
    uint32_t width;        // visible width
    uint32_t height;       // visible height
    uint32_t pitchBytes;   // bytes por fila (alineado)
    void*    cpuPtr;       // base phycont
    SceUID   memBlock;     // handle del bloque
    SceGxmTexture gxmTex;  // textura GXM inicializada linear
    int      nvgImageId;   // id en NanoVG (>=0 válido, -1 no creado)
    bool     inUse;        // marca activa
    uint64_t lastWriteFrame; // frameId para debug
    uint32_t debugSignature; // sentinel
    void invalidate();
    void clear(uint32_t rgba); // debug
};
```

---
## 8. Pitch Auto-Detector

Algoritmo previsto (primer fallo):
1. Intentar decode con `pitch = width` (asumiendo unidad = pixels) → éxito => modo PIXELS.
2. Si error 0x80620005 → reintentar con `pitch = width * 4` → éxito => modo BYTES fijado.
3. Persistir elección (evitar reintentos por frame).
4. Si ambos fallan consecutivamente → activar fallback memcpy.

Pseudocódigo:
```c++
if (!pitchModeKnown) {
   if (tryDecode(width /*as pixels*/)) pitchMode = PIXELS;
   else if (tryDecode(width*4))        pitchMode = BYTES;
   else activateFallback();
}
```

---
## 9. Fallback Dinámico

Condiciones de activación:
- N fallos consecutivos de decode direct buffer.
- Error crítico de memoria (alloc phycont).
- Cambio de resolución donde reinit falla.

Log controlado (throttling):
- Primer activación: `[VIDEO][FALLBACK] enabled reason=decode_errors`
- Luego cada 300 frames: `[VIDEO][FALLBACK] still active`.

---
## 10. Integración con Borealis/NanoVG

Llamadas objetivo (aprox.):
```c++
int id = nvgxmCreateImageFromHandle(vg, &plane.gxmTex, flags);
NVGpaint paint = nvgImagePattern(vg, dstX, dstY, drawW, drawH, 0.0f, id, 1.0f);
nvgBeginPath(vg);
nvgRect(vg, dstX, dstY, drawW, drawH);
nvgFillPaint(vg, paint);
nvgFill(vg);
```
Consideraciones:
- Resolución fuente != resolución destino (letterbox calculado antes).
- Posible selección de filtro (nearest/linear) según modo latencia.

---
## 11. Instrumentación (Resumen)

| Métrica | Método | Frecuencia |
|---------|--------|------------|
| decode_ms | timestamp alrededor de `sceAvcdecDecode` | por frame |
| present_delay_ms | decode->swap->draw diff | por frame |
| fps_decode / fps_present | contadores acumulados | cada 2 s |
| pitch_mode | una vez tras fijar | on change |
| fallback_count | contador global | on event |
| dump_head | hexdump 16 bytes primer frame | start |

Formato sugerido de log:
```
[VIDEO] frame=123 decode=1.05ms present=1.42ms pitch=2048B mode=BYTES zc=1 fb=0
```

---
## 12. Cambio de Resolución

Secuencia:
1. Detectar en header de frame nuevo (width/height difieren).
2. Pausar ingest (bloque corto).
3. Destruir planos existentes.
4. Alloc + init nuevos planos.
5. Reset pitchModeKnown.
6. Log: `[VIDEO] reinit planes 1280x720 -> 1920x1080`.

---
## 13. Flujos Comparativos

### 13.1 Fallback Actual
```
Decode -> BufferPhy -> memcpy -> Textura -> Draw -> Present
```
### 13.2 Zero-Copy Plan
```
Decode -> VideoPlane(back) -> swap -> Draw(NanoVG front) -> Present
```

---
## 14. Roadmap (Sprints)

| Sprint | Entrega | Estado |
|--------|---------|--------|
| 1 | Infra `VideoPlane`, alloc doble buffer, docs | (en progreso) |
| 2 | Integrar createImage NanoVG + logs básicos | pending |
| 3 | Pitch detector + decode direct binding | pending |
| 4 | Swap + render NanoVG (feature flag) | pending |
| 5 | Fallback dinámico + métricas FPS | pending |
| 6 | Limpieza resolución dinámica + doc final | pending |

---
## 15. Riesgos y Mitigaciones (Resumen)

| Riesgo | Mitigación |
|--------|------------|
| Pitch incorrecto | Auto-detector dual-pass + persistencia |
| Race GPU/Decoder | Doble buffer + opción triple si hiciera falta |
| Corrupción memoria | Sentinel + memset debug + asserts en swap |
| Falta coherencia cache | Verificar si Avcdec y GXM requieren invalidate/flush; añadir hooks condicionales |
| Leaks en reinit | Función centralizada destroyVideoPlane | 

---
## 16. Extensiones Futuras
- Texturas YUV (NV12) + shader conversión.
- Triple buffer configurado por flag.
- Ajuste dinámico de filtro según latencia.
- Frame pacing adaptativo.

---
## 17. Glosario Breve
| Término | Definición |
|---------|------------|
| phycont | Memoria física contigua (requerida para GXM/decoder) |
| GXM | GPU API de PS Vita |
| NanoVG | Librería de vector graphics usada por Borealis |
| VideoPlane | Abstracción de un buffer de video listo para texturizar |

---
## 18. Checklist de Validación Inicial
- [ ] Ambos `VideoPlane` alloc OK
- [ ] pitchBytes alineado a 8
- [ ] gxmTex inicializado sin error
- [ ] nvgImageId (stub -1) aceptado sin crash
- [ ] Logs de creación visibles
- [ ] Sin modificación aún del camino de decode actual

---
## 19. Notas
Este documento evoluciona junto con la implementación. Cualquier cambio estructural debe reflejarse aquí para mantener alineación entre código y diseño.

## 20. Lecciones del pipeline legacy (moonlight-legacy)

Del análisis directo de `reference/moonlight-legacy/src/video/vita.c` se desprenden datos concretos que debemos respetar al migrar el flujo a Borealis:

| Aspecto | Legacy | Implicación para la ruta nueva |
|---------|--------|---------------------------------
| Formato de pixel (`picture.frame.pixelType`) | Valor `0` → `SCE_AVCDEC_PIXELFORMAT_RGBA8888` | El decoder ya soporta RGBA directo; no requiere conversión YUV si entregamos los parámetros correctos. |
| Pitch reportado | `picture.frame.framePitch = image_scaling.texture_width;` (unidad: pixeles) | El pitch que recibe `sceAvcdecDecode` es en **pixeles**, no en bytes. Debe coincidir con el ancho real de la textura utilizada. |
| Dimensiones declaradas | `frameWidth/frameHeight = image_scaling.texture_*` | Las dimensiones que se pasan al decoder coinciden exactamente con la textura (sin padding ni crop). Cualquier padding debe reflejarse en ambos lados. |
| Textura destino | Una única `vita2d_texture` de tamaño `image_scaling.texture_width × image_scaling.texture_height` | El decoder escribe directo a la textura; no hay staging intermedio. Nuestra implementación debe garantizar que la memoria proporcionada a `pPicture[0]` sea la misma que usará el renderer. |
| Flujo de render | Tras cada `sceAvcdecDecode` con output, se llama a `vita2d_start_drawing()`, se pinta el frame y se hace `vita2d_swap_buffers()` | La textura recién escrita se consume inmediatamente en el mismo hilo. Esto asegura que no quede un buffer “sin presentar” que pueda ser reescrito, y evita estados inconsistentes del doble buffer. |
| SPS fix | `gs_sps_fix(...)` sobrescribe la NAL SPS dentro de `decoder_buffer` antes del decode | Nuestra ruta ya replica esta lógica con `gs::SpsContext::fix`, por lo que debemos mantenerla activa siempre que tratemos SPS NALs. |
| Alineación | `image_scaling.texture_*` se calcula mediante `VITA_DECODER_RESOLUTION()` (múltiplo de 16, mínimo 64) | Cualquier resolución arbitraria se normaliza antes de llegar al decoder. Debemos seguir usando los mismos macros para evitar tamaños no soportados. |

Resumen operativo: la configuración válida y probada en legacy es “textura = superficie exacta = 960×544”, pitch en pixeles (=960) y RGBA directo sin staging. Cuando adaptemos a Borealis, cualquier optimización (padding extra, crop, staging) debe hacerse garantizando que `frameWidth/frameHeight/framePitch` sigan el mismo contrato que espera `sceAvcdecDecode`.

## 21. Lecciones del pipeline Moonlight-Switch

Aunque la Vita no usa FFmpeg en la ruta actual, el port para Switch demuestra cómo implementar Zero-Copy con un renderer moderno (deko3d) y sirve como referencia para arquitecturas modulares:

| Área | En Switch | Implicación para Vita |
|------|-----------|-----------------------|
| Decoder | `FFmpegVideoDecoder` con FFmpeg; en Switch usa HW decoding NVTEGRA (`AV_PIX_FMT_NVTEGRA`) + `av_hwdevice_ctx_create` | Si en Vita se evaluara FFmpeg en el futuro, habría que replicar la configuración HW correspondiente. El concepto clave es disponer de un decoder que entregue buffers amigables para GPU. |
| Cola de frames | `AVFrameHolder` (cola thread-safe) | Puede inspirar una cola similar si queremos desacoplar decode/render cuando reactivemos doble buffer o threads separados. |
| Renderer | `DKVideoRenderer` (deko3d) crea imágenes que apuntan directamente al buffer de decode (`av_nvtegra_frame_get_fbuf_map`) y actualiza descriptores; luego dibuja un quad con shaders YUV→RGB | Equivalente al objetivo Vita: crear objetos GXM/NanoVG que referencien el buffer del decoder sin copiar. Si usamos YUV en Vita, necesitaremos matrices y offsets similares a los que maneja Switch para BT.601/709/2020. |
| Color Space | Matrices precargadas (`gl_color_matrix`) y offsets según metadata (`frame->colorspace`, `frame->color_range`) | Aporta la tabla exacta para convertir YUV a RGB en los shaders si trasladamos el enfoque. |
| Sincronización | Tras cada frame: `queue.submitCommands(cmdlist); queue.waitIdle();` | Confirmación de que el renderer bloquea hasta presentar. En Vita, debemos tratar cuidadosamente el pacing para no pisar la textura mientras la GPU la usa. |
| Estadísticas | Renderer guarda tiempos (`total_render_time`, `rendered_frames`) y calcula FPS | Podemos reutilizar ideas para la telemetría en Vita (frames renderizados, tiempos medios). |

Resumen operativo: Switch demuestra un pipeline Zero-Copy completo (decoder → GPU) soportado por una cola de frames, un renderer modular y shaders dedicados. Aunque el stack Vita difiere (sceAvcdec + GXM), la estructura por capas y los puntos de instrumentación son directamente aplicables a nuestro trabajo con Borealis.

---
Fin del documento.
