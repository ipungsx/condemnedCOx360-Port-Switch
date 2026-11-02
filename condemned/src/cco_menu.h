#pragma once

// The port's graphics options menu: ZL + ZR + Minus opens it over the game (cco_menu.cpp).

#include <rex/x360/render_hooks.h>

namespace rex {
class Runtime;
}

namespace cco::menu {

// The saved choices (cco_opciones.toml next to the NRO), applied over cco.toml. Called before the cvars are final:
// the GPU clock and the texture filtering only take effect at start.
void CargarOpciones();

// Puts the menu on the controller (the game sees the controller idle while it is open).
void Instalar();

// The renderer's image settings from the menu: FXAA, brightness and gamma in the output pass.
rex::x360::RenderHooks& Hooks();

}  // namespace cco::menu
