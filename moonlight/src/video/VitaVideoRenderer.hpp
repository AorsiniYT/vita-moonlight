// VitaVideoRenderer.hpp - Renderer unificado para presentar video vía NanoVG
#pragma once

#include <borealis.hpp>
#include <stdint.h>

class VitaVideoRenderer {
public:
	static VitaVideoRenderer& instance();

	// Dibuja el frame de video actual (si existe) dentro del rect (x,y,w,h)
	void draw(NVGcontext* vg, float x, float y, float w, float h, float alpha=1.0f);

	// Fuerza invalidar la imagen NVG (p.ej. cambio de tamaño)
	void invalidate();

private:
	VitaVideoRenderer() = default;
};
