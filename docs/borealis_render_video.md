# Arquitectura de Render de Video en Moonlight-Vita con Borealis



**Versión:** 1.0 (Octubre 2025)  **Versión:** 1.0 (Octubre 2025)  

**Estado:** Implementación Legacy Estable con Prototipo Zero-Copy Documentado  **Estado:** Implementación Legacy Estable con Prototipo Zero-Copy Documentado  

**Objetivo:** Documento técnico detallado para desarrolladores, cubriendo pipeline de video, integración Borealis, y optimizaciones futuras.**Objetivo:** Documento técnico detallado para desarrolladores, cubriendo pipeline de video, integración Borealis, y optimizaciones futuras.



## Resumen Ejecutivo## Resumen Ejecutivo



El subsistema de video en Moonlight-Vita implementa un pipeline de streaming de baja latencia para PS Vita, integrando decodificación hardware (sceAvcdec), buffering doble, y render vía vita2d o NanoVG. La ruta actual (legacy) prioriza estabilidad sobre rendimiento óptimo, con un prototipo zero-copy (phycont -> GXM directo) como mejora futura. Borealis maneja la composición UI, permitiendo overlays sobre video.El subsistema de video en Moonlight-Vita implementa un pipeline de streaming de baja latencia para PS Vita, integrando decodificación hardware (sceAvcdec), buffering doble, y render vía vita2d o NanoVG. La ruta actual (legacy) prioriza estabilidad sobre rendimiento óptimo, con un prototipo zero-copy (phycont -> GXM directo) como mejora futura. Borealis maneja la composición UI, permitiendo overlays sobre video.



**Métricas Clave (Baseline Legacy):****Métricas Clave (Baseline Legacy):**

- Latencia típica: 50-100ms (decode + present).- Latencia típica: 50-100ms (decode + present).

---
# Arquitectura de Render de Video en Moonlight‑Vita (Borealis)

Versión: 1.0 — Octubre 2025

Propósito: documento técnico conciso para desarrolladores que describe la arquitectura actual de render de video, la ruta legacy, el prototipo zero‑copy y recomendaciones prácticas para prototipado y medición.

---

## Resumen ejecutivo

La implementación actual prioriza estabilidad: el pipeline decodifica con el acelerador del Vita (sceAvcdec), escribe frames a texturas `vita2d` (doble buffer) y los renderiza con `vita2d` o integrándolos en Borealis vía NanoVG (`nvgxmCreateImageFromHandle`). Existe diseño y código experimental para un prototipo zero‑copy (phycont -> GXM -> NVG) pero no está activado por defecto.

Puntos rápidos:
- Latencia típica (legacy): 50–100 ms (decode + present)
- Mecanismo principal: doble buffering con `VideoFrameHolder` para handoff thread‑safe

---

## Visión general de la arquitectura

Top‑level:

```
[Decoder Thread (sceAvcdec / Limelight)] -> [VideoFrameHolder] -> [UI Thread (Borealis/NVG)]
```

- Decoder Thread: recibe paquetes, decodifica (H.264) a RGBA/YUV o escribe en un buffer físico (phycont) en modos experimentales.
- VideoFrameHolder: entrega thread‑safe de texturas / descriptors entre el decoder y la UI.
- UI Thread: `VitaVideoRenderer` consume `FRAME_FRONT()` y dibuja con `vita2d` o crea un `NVG image` desde el `SceGxmTexture` (via `nvgxmCreateImageFromHandle`) para integrarlo en la composición Borealis.

Dependencias principales: `moonlight-common-c`, `borealis`, Vita SDK (`sceAvcdec`, `vita2d`, `SceGxm`).

---

## Componentes clave (resumen)

| Componente | Responsabilidad | Archivo |
|---|---:|---|
| VideoManager | Inicialización, modos de render, callbacks de decoder | `src/video/VideoManager.cpp` |
| VitaVideoRenderer | Dibujo (vita2d / NVG), estadística y fallback | `src/video/VitaVideoRenderer.cpp` |
| VideoFrameHolder | Handoff thread‑safe de texturas/descriptor | `src/video/VideoFrameHolder.cpp` |
| vita_decode | Callbacks de decode, swap buffers, fallback phys | `src/video/legacy/modules/vita_decode.cpp` |
| vita_globals | Buffers globales y macros (FRAME_FRONT/BACK) | `src/video/legacy/modules/vita_globals.hpp` |

Nota: las rutas de archivo arriba son relativas al árbol `moonlight` del repo.

---

## Flujo de datos — Ruta legacy (actual)

Diagrama simplificado:

```
Decoder Thread                         VideoFrameHolder                      UI Thread
vitavideo_submit_decode_unit()  ->     pushTexture(tex)  ->                 VitaVideoRenderer::draw()/drawNVG()
  (sceAvcdec decodes into FRAME_BACK())     (swap indices)                       if NVG: nvgxmCreateImageFromHandle()
                                            (notify)                            nvgImagePattern() -> nvgFill()
                                                                              else: vita2d_draw_texture_tint_part_scale()
```

Puntos técnicos:
- Decoder escribe en `FRAME_BACK()` (vita2d_texture) o en un buffer phys cuando se usa fallback.
- Se realiza swap de índices (front/back) y se llama `VideoFrameHolder::pushTexture()`.
- El renderer toma `FRAME_FRONT()`. Si se usa NVG, se crea/reusa `nvgImageId` mediante `nvgxmCreateImageFromHandle` y se pinta con `nvgImagePattern`.

Comportamiento defensivo:
- `FRAME_FRONT()` puede ser NULL al inicio: `VitaVideoRenderer` lo comprueba.
- `nvgxmCreateImageFromHandle` puede fallar; en ese caso, el renderer debe caer al dibujo con `vita2d`.

---

## Flujo de datos — Prototipo zero‑copy (experimental)

Objetivo: eliminar memcpy CPU→GPU al decodificar directamente en memoria phycont mapeada para lectura por GXM.

Secuencia propuesta:

1. Reservar phycont: `sceKernelAllocMemBlock(...)` y obtener base con `sceKernelGetMemBlockBase()`.
2. Mapear a GXM: `sceGxmMapMemory(phys_ptr)`.
3. Configurar decoder para escribir en `phys_ptr`.
4. Crear `SceGxmTexture` apuntando a `phys_ptr` (pitch = width*4 para RGBA).
5. UI: `nvgxmCreateImageFromHandle(vg, &gxmTex)` y dibujar sin memcpy.

Riesgos técnicos: coherencia de caché, sincronización GPU/CPU, alineamiento/pitch, y cleanup (unmap/free).

---

## Comparativa de modos (resumen)

| Aspecto | Legacy | FFmpeg (experimental) | Zero‑copy (prototipo) |
|---|---:|---:|---:|
| Backend decode | `sceAvcdec` (HW) | FFmpeg (SW) | `sceAvcdec` (HW)
| Buffering | `vita2d_texture[2]` (+ memcpy fallback) | TBD | phycont directo
| Render API | `vita2d` / NanoVG | TBD | NanoVG (NVG from GXM)
| Latencia (estim.) | 50–100 ms | Desconocido | 30–70 ms
| Estabilidad | Alta | Baja (placeholder) | Media (requiere tuning)
| Memoria aprox. | 2–4 MB buffers | TBD | 1–2 MB phycont

---

## Buffering, pitch y memoria

- Doble buffering: `frame_textures[2]` con índices atómicos `frame_front_idx`/`frame_back_idx`.
- Convención de pitch: históricamente expresado en píxeles (no bytes). Confirmar `framePitch = texture_width` si cambias dimensiones.
- Fallback común: decoder escribe en `decoder_output_phys_ptr` y luego `memcpy` a la textura cuando el decoder no puede escribir directamente a la textura.

Buenas prácticas:
- Liberar texture/phycont correctamente en cleanup (`vita2d_free_texture`, `sceGxmUnmapMemory`, `sceKernelFreeMemBlock`).
- Evitar invalidaciones de caché costosas en el hot‑path; medir antes y después.

---

## Instrumentación y métricas (qué medir)

Recomendado mínimo para comparar rutas:
- `decode_ms`: tiempo por frame en el callback de decode.
- `present_latency`: timestamp decode → timestamp present (ms).
- `frames_presented` / `current_fps`.

Ejemplos de logs a añadir en puntos críticos:
- `VITA_DEBUG_LOG("decode_ms=%d, pts=%llu", decode_ms, pts)`
- `VITA_DEBUG_LOG("NVG image created: id=%d", nvgImageId)`
- `VITA_DEBUG_LOG("Swap: front=%p back=%p stride=%d", ptr_front, ptr_back, stride)`

---

## Limitaciones actuales

- Zero‑copy: diseño presente pero no activado por defecto; requiere prototipo y validación.
- FFmpeg: soporte condicional (`BUILD_FFMPEG`) existe como placeholder, no es la ruta por defecto.
- Bottleneck principal: memcpy en fallback y sincronización entre decoder/UI.

---

## Recomendaciones prácticas (pasos siguientes)

1. Añadir métricas `decode_ms` y `present_latency` en `vita_decode` y `VitaVideoRenderer`.
2. Prototipar zero‑copy en un branch: crear pequeño PoC que reserve phycont, mapée GXM, decode directo y pinche logs/imagen de sanity.
3. Comparar rutas con datasets reproducibles (packets o vídeo de prueba) y reportar mejora de latencia/CPU.

---

## Referencias de código (entradas clave)

- `moonlight/src/video/legacy/modules/vita_globals.hpp`
- `moonlight/src/video/legacy/modules/vita_decode.cpp`
- `moonlight/src/video/VitaVideoRenderer.cpp`
- `moonlight/src/video/VideoManager.cpp`
- `moonlight/src/video/VideoFrameHolder.cpp`

---


