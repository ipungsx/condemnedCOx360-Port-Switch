// deadspace2 - the recompiled code's "interrupts off" regions (cco_cpu.cpp).

#pragma once

namespace cco::cpu {

// Before the game's first thread runs: reads cco_msr_cerrojo_global.
void Configurar();

// The build (version, commit) and the console (firmware, Atmosphere, title takeover or applet, handheld or docked) at
// the top of the log: what a player's log needs to say before anything else.
void InformarSistema();

}  // namespace cco::cpu
