# Render de Video con Borealis (estado actual)

Este documento fue actualizado para reflejar la implementación actual en el árbol `moonlight` (octubre 2025).

Resumen corto:
- El objetivo original de "zero-copy" (decoder -> phycont -> GXM directo sin memcpy) se ha dejado como diseño/objetivo, pero en la implementación actual se eligió una ruta simplificada y estable: decodificación -> texturas `vita2d` y dibujo por `vita2d` o NanoVG via `nvgxmCreateImageFromHandle`.
- La implementación actual se basa en doble buffer de `vita2d_texture` (`frame_textures[2]`, macros `FRAME_FRONT()` / `FRAME_BACK()`) y un mecanismo sencillo de entrega de frames (`VideoFrameHolder`).
- El componente `VideoManager` mantiene por defecto el modo `legacy` (ruta probada). `ffmpeg` existe como opción/placeholder pero no está activado por defecto.

---

## Estado actual (implementación real)

- Pipeline: decoder (sceAvcdec / Limelight callbacks) produce frames RGBA (o YUV experimental). Los frames se ponen en `vita2d_texture` (o en un buffer físico que luego se copia) y el renderer consume la textura para presentarla.
- No hay una implementación completa y estable de "VideoPlane" zero-copy en el repo actual; el código contiene marcadores y documentación que describen la meta, pero la ruta activa es la basada en `vita2d`.
- El render soporta dos rutas de dibujado:
   1. `vita2d_draw_texture_tint_part_scale(...)` — ruta legacy que dibuja la textura con `vita2d`.
   2. `NanoVG (nvgxmCreateImageFromHandle)` — crea un `nvg image` a partir del `SceGxmTexture` (obtenido desde la `vita2d_texture`) y dibuja con `nvgImagePattern`. Esto permite integrar el frame en la composición de Borealis.

Elementos clave en el código actual:
- `moonlight/src/video/legacy/modules/vita_globals.hpp`: define `frame_textures[2]`, `frame_front_idx`, `frame_back_idx`, macros `FRAME_FRONT()` y `FRAME_BACK()` y flags de configuración (p. ej. `single_frame_buffer`, `video_fullscreen_stretch`).
- `VitaVideoRenderer` (`VitaVideoRenderer.cpp`): implementa `draw(...)` (vita2d) y `drawNVG(...)` (NanoVG). En `drawNVG` se crea / reutiliza un `nvg image` con `nvgxmCreateImageFromHandle(vg, gxmTex)` y se pinta mediante `nvgImagePattern`.
- `VideoManager` (`VideoManager.cpp`): administra el modo de render (`legacy` por defecto; `ffmpeg` es una opción condicional) y expone callbacks para Limelight. Controla la inicialización y el inicio/parada del subsistema de video.
- `VideoFrameHolder` (`VideoFrameHolder.cpp`): fachada thread-safe mínima que recibe `vita2d_texture*` (y el `SceGxmTexture*`) mediante `pushTexture()` y lo entrega con `popLatest()` para que el renderer lo consuma.

---

## Flujo actual (detalle)

1. El decoder (a través de los callbacks configurados por `VideoManager::getDecoderCallbacks()`) decodifica frames a RGBA o produce buffers YUV en modo experimental.
2. El código crea/actualiza la `vita2d_texture` correspondiente y actualiza los índices `frame_front_idx` / `frame_back_idx` (doble buffer). En algunos casos se usa una etapa física intermedia (`decoder_output_phys_ptr`) seguida de `memcpy` hacia la textura si el decoder no puede escribir directamente en la memoria de textura.
3. `VideoFrameHolder::pushTexture()` es llamado con la textura lista; el renderer (desde `VitaVideoRenderer`) consulta `FRAME_FRONT()` o consume el `VideoFrameHolder` según la integración concreta y dibuja:
    - Ruta vita2d: `vita2d_draw_texture_tint_part_scale(tex, ox, oy, sx, sy, ex, ey, scaleX, scaleY, tint)`
    - Ruta NanoVG: si hay un `NVGcontext* vg` activo, se llama a `nvgxmCreateImageFromHandle(vg, &gxmTex)` para obtener `nvgImageId` y se pinta con `nvgImagePattern`.
4. Estadísticas de presentación (frames_presented, fps, session_ms) se actualizan por el `VitaVideoRenderer` usando `update_present_stats()`.

Observaciones operativas:
- `nvgxmCreateImageFromHandle` se usa de forma defensiva: se recrea la imagen si cambia la textura o las dimensiones. Si `nvgxmCreateImageFromHandle` falla, la ruta NVG se abandona para ese frame.
- `FRAME_FRONT()` puede ser `nullptr` si aún no hay frame decodificado; `VitaVideoRenderer` lo comprueba y evita dibujar en ese caso.

---

## Buffering y pitch

- Se usa doble buffering básico (`frame_textures[2]`), y hay una flag `single_frame_buffer` cuando se quiere forzar un único buffer (FRONT == BACK).
- Importante: el pitch que pasa el código a `sceAvcdecDecode` y que se usa históricamente en `moonlight-legacy` es expresado en píxeles (no en bytes). El código actual conserva esta convención: `framePitch = image_scaling.texture_width`.
- En la práctica, si el decoder no puede escribir directamente en la textura, el sistema emplea un fallback que escribe en un buffer físico y luego realiza `memcpy` hacia la textura; esto es la ruta más estable hoy.

---

## Modo FFmpeg vs Legacy

- `VideoManager` tiene soporte para dos modos conceptuales:
   - `legacy` (0): la ruta probada que usa las APIs de Vita (sceAvcdec, vita2d) y las callbacks de Limelight.
   - `ffmpeg` (1): opción experimental / placeholder pensada para integrar FFmpeg; en el código actual está preparada condicionalmente (`#ifdef BUILD_FFMPEG`) pero no es la ruta por defecto ni está completamente implementada.
- `VideoManager::setRenderMode()` y `ensure_render_mode_cached()` mantienen y persisten el modo en `ConfigManager`.

---

## Instrumentación y telemetría

- `VitaVideoRenderer` mantiene estadísticas simples: frames decoded/presented, fps (ventana 1s), session_ms. La función `update_present_stats()` calcula `current_fps` y actualiza `g_stats`.
- Los logs del renderer escriben eventos importantes: creación de `nvg image`, errores en `nvgxmCreateImageFromHandle`, stride/texture pointers y cambios de modo.

---

## Limitaciones y estado de desarrollo

- Zero-copy (decoder -> phycont -> GXM directo) está documentado como objetivo pero actualmente no está activado: la base de código contiene ideas, tipos y comentarios relacionados, pero la implementación estable escogida fue la vía con `vita2d` + posible `memcpy` fallback.
- El camino zero-copy sigue siendo una posible mejora futura (ver `docs/` histórico y comentarios), pero su implementación requiere manejo cuidadoso de:
   - asignación de memoria física contigua (phycont), coherencia cache/invalidate, y sincronización GPU/CPU (double/triple buffering), además de detector de pitch robusto.

---

## Recomendaciones y próximos pasos prácticos

- Si buscas reducir latencia: estudiar la opción zero-copy con un prototipo mínimo que:
   1. Reserve `phycont` y cree `SceGxmTexture` apuntando a ese buffer.
   2. Haga decode directo a ese buffer y pruebe `nvgxmCreateImageFromHandle` para dibujar sin `memcpy`.
   3. Implementar doble buffer en `VideoPlane` (back/front) y un detector sencillo de pitch en píxeles.
- Mientras tanto, conservar y endurecer la ruta actual (memcpy -> `vita2d_texture`) es razonable para estabilidad.
- Añadir métricas adicionales (decode_ms, present_latency) y generar logs con counters para validar mejoras de rendimiento cuando se experimente con zero-copy.

---

## Puntos de referencia en el código (para revisar cambios)

- `moonlight/src/video/legacy/modules/vita_globals.hpp` — macros, flags y buffers globales.
- `moonlight/src/video/VitaVideoRenderer.cpp` — dibujo con `vita2d` y `drawNVG` con NanoVG.
- `moonlight/src/video/VideoManager.cpp` — modos de render, control de inicialización y callbacks.
- `moonlight/src/video/VideoFrameHolder.cpp` — entrega thread-safe de `vita2d_texture*` para el renderer.
- `moonlight/src/video/render_mode_cache.cpp` — cache atómico del modo de render.

---

Fin del documento (actualizado).
