#include "vita_globals.hpp"
#ifdef BOREALIS_USE_GXM
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <psp2/gxm.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <borealis/core/application.hpp>
#include <borealis/extern/nanovg/nanovg.h>
#include "network/NetworkOptimizations.hpp"

// Definir constantes que pueden faltar
#ifndef SCE_VIDEODEC_TYPE_HW_AVCDEC
#define SCE_VIDEODEC_TYPE_HW_AVCDEC ((SceVideodecType)0x1001)
#endif

// Tiempo monotónico en ms
uint64_t vita_monotonic_ms() {
	return sceKernelGetSystemTimeWide() / 1000ULL;
}

// Thread pacer (limita FPS y setea need_drop)
int vita_pacer_thread_main(SceSize args, void* argp) {
	(void)args; (void)argp;
	VITA_DEBUG_LOG("[Video][PACER] thread iniciado");
	// Ajustar afinidad: permitir que el pacer use dos núcleos si disponibles (user 0 y 1)
	sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0 | SCE_KERNEL_CPU_MASK_USER_1);
	uint64_t last_50_tick = vita_monotonic_ms();
	uint64_t last_500_tick = last_50_tick;
	// Sincronizar target fps con configuración actual (curr_fps[1] se actualiza externamente)
	vita_netopt_set_target_fps(curr_fps[1] > 0 ? curr_fps[1] : 60);
	while (active_pacer_thread) {
		sceKernelDelayThread(5 * 1000); // sleep corto (~5ms) para granularidad sin busy-wait
		uint64_t now = vita_monotonic_ms();

		// Tick granular 50ms (loss window / conexión)
		if (now - last_50_tick >= 50) {
			vita_netopt_tick_50ms();
			last_50_tick = now;
		}
		// Tick backoff general (reusa anterior función)
		if (now - last_500_tick >= 500) {
			vita_netopt_tick();
			last_500_tick = now;
		}

		// Consumir drop budget adaptativo y cargar en need_drop (legacy variable) para compatibilidad
		unsigned drops = vita_netopt_consume_drop_budget();
		if (drops) {
			need_drop += (int)drops;
			if (need_drop < 0) need_drop = 0;
		}
	}
	VITA_DEBUG_LOG("[Video][PACER] thread saliendo");
	return 0;
}

extern "C" void vita_cleanup() {
	VITA_DEBUG_LOG("[Video] vita_cleanup llamado");
	// Signal pacer thread to stop and wait for it to finish to avoid racing
	// with vita2d rendering / GXM driver cleanup. This mirrors the legacy
	// moonlight implementation which waits for the thread end before deleting it.
	if (active_pacer_thread) {
		active_pacer_thread = false;
		if (pacer_thread >= 0) {
			SceInt32 wait_ret = 0;
			SceUInt timeout = 10000000; // 10s in microseconds
			// Wait for pacer thread to exit gracefully
			sceKernelWaitThreadEnd(pacer_thread, &wait_ret, &timeout);
			// Delete thread handle
			sceKernelDeleteThread(pacer_thread);
			pacer_thread = -1;
		}
	}
	// Ensure any pending vita2d rendering work is finished before we start
	// tearing down decoder/GXM resources. Calling vita2d_wait_rendering_done()
	// here prevents races where the GPU is still processing frames while we
	// call sceAvcdecDeleteDecoder / sceVideodecTermLibrary.
	if (vita2d_inited) {
		VITA_DEBUG_LOG("[Video] Waiting for rendering to finish (post-pacer) before decoder teardown");
		vita2d_wait_rendering_done();
	}
	// Do a soft-stop only: we must NOT terminate the AVC library or free
	// decoder/textures/memblocks here because Borealis may still hold
	// references into the GXM context (NVG images). Termination and full
	// resource free are performed in vita_full_teardown() at application
	// exit.

	// Terminate the AVC library to allow re-initialization in next session
	if (init) {
		sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
	}

	// Mark video subsystem as not initialized and return after waiting for
	// rendering to quiesce.
	video_status = VITA_VIDEO_NOT_INIT;
	VITA_DEBUG_LOG("[Video] soft cleanup completed (AVC termination done, full teardown deferred)");
}

// Stubs (placeholder)
void vitavideo_draw_fps() {}
void vitavideo_draw_indicators() {}

#endif // BOREALIS_USE_GXM