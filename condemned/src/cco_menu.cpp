// condemned - the port's graphics options menu (modelled on ds2-nx's ds2_menu.cpp).
//
// ZL + ZR + Minus opens it over the game; the deko3d presenter paints it (sdk-patches/0012 and 0024: no ImGui, which
// would move presentation to the UI thread) and while it is open the game sees the controller idle. Up / Down choose
// the row, Left / Right change it (felt at once where the renderer can), A saves to cco_opciones.toml next to the NRO,
// B closes. The GPU clock and the texture filtering are read when the game starts, so they apply at the next start.

#include "cco_menu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/filtro_mando.h>
#include <rex/logging.h>
#include <rex/ui/superposicion.h>

#if defined(__SWITCH__)
#include <switch.h>
#endif

REXCVAR_DEFINE_BOOL(cco_fxaa, false, "CCO", "FXAA in the output pass (options menu)");
REXCVAR_DEFINE_BOOL(cco_grano_pelicula, true, "CCO", "The film grain of the composite pass (options menu)");
REXCVAR_DEFINE_BOOL(cco_luz_volumetrica, true, "CCO",
                    "The volumetric light shafts (options menu). false = not drawn, a little less GPU");
REXCVAR_DEFINE_INT32(cco_brillo, 0, "CCO", "Brightness in the output pass, -5 to +5 (options menu)").range(-5, 5);
REXCVAR_DEFINE_INT32(cco_gamma, 100, "CCO", "Gamma in the output pass in hundredths, 80 to 120 (options menu)")
    .range(80, 120);

REXCVAR_DECLARE(int32_t, cco_gpu_portatil_mhz);        // cco_app.h
REXCVAR_DECLARE(int32_t, nfsmw_nativo_anisotropico);   // sdk/src/x360/deko/texturas.cpp
REXCVAR_DECLARE(double, input_vibracion_fuerza);        // sdk/src/input/switch/switch_input_driver.cpp
REXCVAR_DECLARE(bool, switch_contador_fps);              // sdk/src/input/switch/switch_input_driver.cpp

namespace cco::menu {
namespace {

#if defined(__SWITCH__)
constexpr uint64_t kCombinacion = HidNpadButton_ZL | HidNpadButton_ZR | HidNpadButton_Minus;
constexpr uint64_t kArriba = HidNpadButton_Up | HidNpadButton_StickLUp;
constexpr uint64_t kAbajo = HidNpadButton_Down | HidNpadButton_StickLDown;
constexpr uint64_t kIzquierda = HidNpadButton_Left | HidNpadButton_StickLLeft;
constexpr uint64_t kDerecha = HidNpadButton_Right | HidNpadButton_StickLRight;
constexpr uint64_t kAceptar = HidNpadButton_A;
constexpr uint64_t kCerrar = HidNpadButton_B;
#else
constexpr uint64_t kCombinacion = 0, kArriba = 0, kAbajo = 0, kIzquierda = 0, kDerecha = 0, kAceptar = 0,
                   kCerrar = 0;
#endif

// One row: its values as shown and as saved, which one is current, and how to apply one.
struct Fila {
  const char* nombre;
  const char* cvar;  // saved as "cvar = valor"
  std::vector<std::string> textos;
  std::vector<std::string> valores;
  std::function<size_t()> actual;
  std::function<void(size_t)> aplicar;
  bool al_reiniciar;  // applies at the next start
  // A choice that applies at the next start: shown and saved, the running value left alone (some of these cvars
  // cannot change after start).
  long pendiente = -1;
  size_t Indice() const { return pendiente >= 0 ? size_t(pendiente) : actual(); }
};

size_t Cercano(const std::vector<double>& opciones, double v) {
  size_t mejor = 0;
  for (size_t i = 1; i < opciones.size(); ++i)
    if (std::fabs(opciones[i] - v) < std::fabs(opciones[mejor] - v)) mejor = i;
  return mejor;
}

std::vector<Fila>& Filas() {
  static std::vector<Fila> filas = [] {
    std::vector<Fila> f;
    // The handheld GPU clock through Horizon's own profiles (not an overclock). Docked is always 768 MHz.
    f.push_back({"GPU (HANDHELD)", "cco_gpu_portatil_mhz", {"307 MHZ", "384 MHZ", "460 MHZ"}, {"0", "384", "460"},
                 [] { return Cercano({0, 384, 460}, REXCVAR_GET(cco_gpu_portatil_mhz)); },
                 [](size_t i) {
                   static constexpr int32_t kMhz[] = {0, 384, 460};
                   REXCVAR_SET(cco_gpu_portatil_mhz, kMhz[i]);
                 }, true});
    f.push_back({"TEXTURE FILTERING", "nfsmw_nativo_anisotropico", {"TRILINEAR", "2x", "4x", "8x", "16x"},
                 {"0", "2", "4", "8", "16"},
                 [] { return Cercano({0, 2, 4, 8, 16}, REXCVAR_GET(nfsmw_nativo_anisotropico)); },
                 [](size_t i) { rex::cvar::SetFlagByName("nfsmw_nativo_anisotropico", std::array{"0", "2", "4", "8", "16"}[i]); },
                 true});
    f.push_back({"ANTI-ALIASING", "cco_fxaa", {"OFF", "FXAA"}, {"false", "true"},
                 [] { return size_t(REXCVAR_GET(cco_fxaa)); }, [](size_t i) { REXCVAR_SET(cco_fxaa, i == 1); }, false});
    f.push_back({"FILM GRAIN", "cco_grano_pelicula", {"OFF", "ON"}, {"false", "true"},
                 [] { return size_t(REXCVAR_GET(cco_grano_pelicula)); },
                 [](size_t i) { REXCVAR_SET(cco_grano_pelicula, i == 1); }, false});
    f.push_back({"VOLUMETRIC LIGHT", "cco_luz_volumetrica", {"OFF", "ON"}, {"false", "true"},
                 [] { return size_t(REXCVAR_GET(cco_luz_volumetrica)); },
                 [](size_t i) { REXCVAR_SET(cco_luz_volumetrica, i == 1); }, false});
    {
      Fila b{"BRIGHTNESS", "cco_brillo", {}, {}, [] { return size_t(REXCVAR_GET(cco_brillo) + 5); },
             [](size_t i) { REXCVAR_SET(cco_brillo, int32_t(i) - 5); }, false};
      for (int v = -5; v <= 5; ++v) {
        b.textos.push_back(v > 0 ? "+" + std::to_string(v) : std::to_string(v));
        b.valores.push_back(std::to_string(v));
      }
      f.push_back(std::move(b));
    }
    {
      Fila g{"GAMMA", "cco_gamma", {}, {}, [] { return size_t((REXCVAR_GET(cco_gamma) - 80) / 5); },
             [](size_t i) { REXCVAR_SET(cco_gamma, int32_t(80 + i * 5)); }, false};
      for (int v = 80; v <= 120; v += 5) {
        char t[8];
        std::snprintf(t, sizeof(t), "%d.%02d", v / 100, v % 100);
        g.textos.push_back(t);
        g.valores.push_back(std::to_string(v));
      }
      f.push_back(std::move(g));
    }
    f.push_back({"VIBRATION", "input_vibracion_fuerza", {"OFF", "25%", "50%", "75%", "100%"},
                 {"0.0", "0.25", "0.5", "0.75", "1.0"},
                 [] { return Cercano({0, 0.25, 0.5, 0.75, 1.0}, REXCVAR_GET(input_vibracion_fuerza)); },
                 [](size_t i) {
                   static constexpr double kFuerza[] = {0.0, 0.25, 0.5, 0.75, 1.0};
                   REXCVAR_SET(input_vibracion_fuerza, kFuerza[i]);
                 },
                 false});
    f.push_back({"FPS COUNTER", "switch_contador_fps", {"OFF", "ON"}, {"false", "true"},
                 [] { return size_t(REXCVAR_GET(switch_contador_fps)); },
                 [](size_t i) { REXCVAR_SET(switch_contador_fps, i == 1); }, false});
    return f;
  }();
  return filas;
}

struct Estado {
  std::mutex candado;
  bool abierto = false;
  size_t fila = 0;
  std::string aviso;
  std::array<uint64_t, 8> antes{};
};
Estado& ElEstado() {
  static Estado e;
  return e;
}

std::filesystem::path Fichero() { return rex::filesystem::GetExecutableFolder() / "cco_opciones.toml"; }

std::string Relleno(std::string s, size_t ancho) {
  if (s.size() < ancho) s.append(ancho - s.size(), ' ');
  return s;
}

void Pintar(const Estado& e) {
  const auto& filas = Filas();
  std::vector<std::string> lineas;
  lineas.push_back("\x02" "CONDEMNED: CRIMINAL ORIGINS - SETTINGS");
  lineas.push_back("");
  for (size_t i = 0; i < filas.size(); ++i) {
    const Fila& f = filas[i];
    const size_t v = std::min(f.Indice(), f.textos.size() - 1);
    std::string linea = std::string(i == e.fila ? "\x01" : "") + " " + Relleno(f.nombre, 20) + "< " + f.textos[v] + " >";
    if (f.al_reiniciar) linea += " !";
    lineas.push_back(linea);
  }
  lineas.push_back("");
  lineas.push_back("\x03" " ! APPLIES AT THE NEXT START. DOCKED GPU: 768 MHZ");
  lineas.push_back("\x03" " UP/DOWN: ROW  LEFT/RIGHT: CHANGE  A: SAVE  B: CLOSE");
  if (!e.aviso.empty()) lineas.push_back("\x02 " + e.aviso);
  rex::ui::FijarSuperposicion(std::move(lineas));
}

bool Guardar() {
  FILE* f = std::fopen(Fichero().string().c_str(), "w");
  if (!f) return false;
  bool ok = std::fprintf(f, "# condemned-nx: the options menu's choices (ZL + ZR + Minus). Read at start, over cco.toml.\n") > 0;
  for (const Fila& fila : Filas()) {
    const size_t v = std::min(fila.Indice(), fila.valores.size() - 1);
    ok = std::fprintf(f, "%s = %s\n", fila.cvar, fila.valores[v].c_str()) > 0 && ok;
  }
  return std::fclose(f) == 0 && ok;
}

bool Filtro(size_t ranura, uint64_t botones) {
  Estado& e = ElEstado();
  std::lock_guard<std::mutex> cerrojo(e.candado);
  const size_t s = ranura < e.antes.size() ? ranura : e.antes.size() - 1;
  const uint64_t nuevos = botones & ~e.antes[s];
  e.antes[s] = botones;
  if (!e.abierto) {
    if ((botones & kCombinacion) != kCombinacion || !(nuevos & kCombinacion)) return false;
    e.abierto = true;
    e.aviso.clear();
    REXLOG_INFO("cco: menu de opciones abierto");
    Pintar(e);
    return true;
  }
  auto& filas = Filas();
  if (nuevos & kCerrar) {
    e.abierto = false;
    rex::ui::FijarSuperposicion({});
    REXLOG_INFO("cco: menu de opciones cerrado");
    return true;
  }
  if (nuevos & (kArriba | kAbajo)) {
    const size_t n = filas.size();
    e.fila = (nuevos & kAbajo) ? (e.fila + 1) % n : (e.fila + n - 1) % n;
    e.aviso.clear();
    Pintar(e);
  } else if (nuevos & (kIzquierda | kDerecha)) {
    Fila& f = filas[e.fila];
    const size_t n = f.textos.size(), v = std::min(f.Indice(), n - 1);
    const size_t nuevo = (nuevos & kDerecha) ? std::min(v + 1, n - 1) : (v > 0 ? v - 1 : 0);
    if (nuevo != v) {
      if (f.al_reiniciar) {
        f.pendiente = long(nuevo);
      } else {
        f.aplicar(nuevo);
      }
      REXLOG_INFO("cco: opcion {} = {}", f.nombre, f.textos[nuevo]);
    }
    e.aviso = f.al_reiniciar ? "SAVE (A) AND RESTART THE GAME FOR THIS ONE" : "";
    Pintar(e);
  } else if (nuevos & kAceptar) {
    const bool ok = Guardar();
    e.aviso = ok ? "SAVED" : "COULD NOT SAVE (SD CARD?)";
    REXLOG_INFO("cco: opciones guardadas en {} ({})", Fichero().string(), ok ? "ok" : "FALLO");
    Pintar(e);
  }
  return true;  // the game does not see the controller while the menu is open
}

// FXAA, brightness and gamma in the output pass.
class Ganchos : public rex::x360::RenderHooks {
 public:
  bool Fxaa() override { return REXCVAR_GET(cco_fxaa); }
  rex::x360::PostProcess Posproceso() override {
    rex::x360::PostProcess p;
    const int32_t brillo = REXCVAR_GET(cco_brillo), gamma = REXCVAR_GET(cco_gamma);
    p.activo = brillo != 0 || gamma != 100;
    p.brillo = float(brillo) * 0.02f;
    p.gamma = float(gamma) / 100.0f;
    return p;
  }
  uint64_t VersionPosproceso() override {
    return uint64_t(REXCVAR_GET(cco_brillo) + 100) * 1000 + uint64_t(REXCVAR_GET(cco_gamma));
  }
};

}  // namespace

void CargarOpciones() {
  std::error_code ec;
  if (!std::filesystem::exists(Fichero(), ec)) return;
  rex::cvar::LoadConfig(Fichero());
  REXLOG_INFO("cco: opciones de {} (GPU portatil {} MHz, anisotropico {}, FXAA {}, grano {}, luz volumetrica {}, "
              "brillo {}, gamma {})",
              Fichero().string(), REXCVAR_GET(cco_gpu_portatil_mhz), REXCVAR_GET(nfsmw_nativo_anisotropico),
              REXCVAR_GET(cco_fxaa), REXCVAR_GET(cco_grano_pelicula), REXCVAR_GET(cco_luz_volumetrica),
              REXCVAR_GET(cco_brillo), REXCVAR_GET(cco_gamma));
}

void Instalar() {
  rex::input::FijarFiltroMando(&Filtro);
  REXLOG_INFO("cco: menu de opciones con ZL + ZR + Menos");
}

rex::x360::RenderHooks& Hooks() {
  static Ganchos ganchos;
  return ganchos;
}

}  // namespace cco::menu
