# Uso correcto de CapUnlocker/CoreUnlocker y optimización de hilos en PSVita

## 1. Detección del plugin
Para detectar si CapUnlocker (o CoreUnlocker) está activo, se debe usar la función interna:

```c
extern SceUID _vshKernelSearchModuleByName(const char *name, SceUInt64 *unk);

SceUInt64 unk;
SceUID modid = _vshKernelSearchModuleByName("CapUnlocker", &unk);
if (modid >= 0) {
    // CapUnlocker está presente
} else {
    // No está presente
}
```

El nombre puede variar según el plugin: "CapUnlocker", "CoreUnlocker80000H", etc.

## 2. Afinidad y prioridad de hilos
- Para aprovechar todos los núcleos, el homebrew debe cambiar la afinidad del hilo crítico usando:
  ```c
  sceKernelChangeThreadCpuAffinityMask(SCE_KERNEL_THREAD_ID_SELF, 0xF); // 0xF = núcleos 0-3
  ```
- Para cambiar la prioridad:
  ```c
  sceKernelChangeThreadPriority(SCE_KERNEL_THREAD_ID_SELF, 64); // 64 = prioridad alta
  ```
- **Importante:** Esto solo funciona si CapUnlocker/CoreUnlocker está activo. Si no, el sistema puede ignorar el cambio o limitarlo.

## 3. Hilos nativos vs std::thread
- Para control total, usa hilos nativos (`sceKernelCreateThread`).
- `std::thread` puede no mapear 1:1 a hilos del kernel, por lo que la afinidad podría no aplicarse correctamente.

## 4. Ejemplo de flujo recomendado
1. Detectar CapUnlocker/CoreUnlocker.
2. Si está presente, cambiar afinidad/prioridad de los hilos críticos.
3. (Opcional) Permitir al usuario elegir si quiere forzar el uso de todos los núcleos.

## 5. Referencias
- ThreadOptimizer: ejemplo de detección y advertencia si falta el plugin.
- Otros homebrew (ej: wiliwili) permiten configurar el uso de todos los núcleos desde la UI/config.

---

**Resumen:**
- Detecta CapUnlocker/CoreUnlocker con `_vshKernelSearchModuleByName`.
- Cambia afinidad/prioridad solo si está presente.
- Usa hilos nativos para control total.
- Documenta y/o expón la opción al usuario si es relevante.
