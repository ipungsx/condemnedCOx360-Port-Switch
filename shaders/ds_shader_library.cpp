#include "ds_shader_library.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
/*
 * xxHash reads with memcpy (XXH_FORCE_MEMORY_ACCESS 0). With the method it picks for GCC (1) it reads
 * through 64- and 32-bit pointers without may_alias, and GCC may hoist that read above the write of the
 * data being hashed (strict aliasing). That made the texture key read claves[4] before writing it, and
 * the same texture was created several times. Same fingerprint values; on AArch64, the same LDR.
 */
#if defined(XXH_IMPLEM_13a8737387)
#error "xxhash.h ya se ha incluido con su implementacion antes de este punto: XXH_FORCE_MEMORY_ACCESS 0 llegaria tarde"
#endif
#undef XXH_FORCE_MEMORY_ACCESS
#define XXH_FORCE_MEMORY_ACCESS 0
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace ds {
namespace {
constexpr std::array<uint8_t, 8> kFirma{'D','S','2','S','P','V',0,0};
constexpr size_t kMaxArchivo = 64 * 1024 * 1024;
constexpr size_t kMaxShaders = 4096;
constexpr size_t kMaxOriginal = 64 * 1024;
constexpr size_t kMaxSpirv = 4 * 1024 * 1024;

void Exigir(bool condicion, const char* error) {
  if (!condicion) throw std::runtime_error(error);
}

uint32_t BE(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
         uint32_t(p[2]) << 8 | p[3];
}

struct Lector {
  std::span<const uint8_t> datos;
  size_t posicion = 0;
  std::span<const uint8_t> Tomar(size_t n) {
    Exigir(n <= datos.size() - posicion, "Paquete de shaders truncado");
    auto r = datos.subspan(posicion, n);
    posicion += n;
    return r;
  }
  uint32_t U32() {
    auto p = Tomar(4);
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  }
  uint64_t U64() {
    const uint64_t bajo = U32();
    return bajo | uint64_t(U32()) << 32;
  }
};

void U32(std::vector<uint8_t>& d, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) d.push_back(uint8_t(v >> (i * 8)));
}
void U64(std::vector<uint8_t>& d, uint64_t v) {
  U32(d, uint32_t(v)); U32(d, uint32_t(v >> 32));
}

void Validar(Shader& s) {
  Exigir(s.original.size() >= 24 && s.original.size() <= kMaxOriginal,
         "Longitud del contenedor fuera de limites");
  const uint32_t firma = BE(s.original.data());
  // Format 2008 (0x102A1100, +1 for a vertex shader) or 2005 (0x102A0E00, Condemned). The physical part is
  // 4-byte words.
  Exigir((firma & ~1u) == 0x102A1100 || (firma & ~1u) == 0x102A0E00, "Firma de contenedor desconocida");
  const uint32_t virtuales = BE(s.original.data() + 4);
  const uint32_t fisicos = BE(s.original.data() + 8);
  Exigir(virtuales >= 24 && fisicos && !(fisicos % 4) &&
             uint64_t(virtuales) + fisicos == s.original.size(),
         "Longitudes del contenedor incoherentes");
  s.vertices = (firma & 1) != 0;
  s.huella = XXH3_64bits(s.original.data(), s.original.size());
  s.normalizado = Normalizar(s.original);
  Exigir(s.spirv.size() >= 5 && s.spirv.size() <= kMaxSpirv / 4,
         "Longitud SPIR-V fuera de limites");
  Exigir(s.spirv[0] == 0x07230203 && s.spirv[4] == 0,
         "Cabecera SPIR-V incorrecta");
  // This checks the wrapper; it does not replace spirv-val in the packager.
  bool entrada = false;
  for (size_t i = 5; i < s.spirv.size();) {
    const uint32_t palabras = s.spirv[i] >> 16, op = s.spirv[i] & 65535;
    Exigir(palabras && palabras <= s.spirv.size() - i, "Instruccion SPIR-V truncada");
    if (op == 15) {  // OpEntryPoint: modelo de ejecucion, id, nombre e interfaz.
      Exigir(!entrada && palabras >= 5 && s.spirv[i+1] == (s.vertices ? 0u : 4u) &&
                 s.spirv[i+3] == 0x6e69616d && s.spirv[i+4] == 0,
             "La etapa o la entrada main de SPIR-V no coincide con el contenedor");
      entrada = true;
    }
    i += palabras;
  }
  Exigir(entrada, "SPIR-V sin entrada main");
}

bool Menor(const Shader& a, const Shader& b) {
  if (a.huella != b.huella) return a.huella < b.huella;
  return a.original < b.original;
}
}  // namespace

void BibliotecaShaders::Cargar(std::span<const uint8_t> archivo) {
  Exigir(archivo.size() >= 24 && archivo.size() <= kMaxArchivo,
         "Tamano del paquete de shaders fuera de limites");
  Lector l{archivo};
  auto firma = l.Tomar(8);
  Exigir(std::equal(firma.begin(), firma.end(), kFirma.begin()), "Firma de paquete desconocida");
  Exigir(l.U32() == 1, "Version de paquete no admitida");
  const uint32_t cantidad = l.U32();
  Exigir(cantidad && cantidad <= kMaxShaders, "Cantidad de shaders fuera de limites");
  const uint64_t huella = l.U64();
  Exigir(XXH3_64bits(archivo.data() + 24, archivo.size() - 24) == huella,
         "Paquete de shaders alterado");
  std::vector<Shader> nuevos;
  nuevos.reserve(cantidad);
  for (uint32_t i = 0; i < cantidad; ++i) {
    const uint32_t original = l.U32(), palabras = l.U32();
    const uint64_t esperada = l.U64();
    Exigir(original >= 24 && original <= kMaxOriginal &&
               palabras >= 5 && palabras <= kMaxSpirv / 4, "Entrada demasiado grande");
    Shader s;
    auto datos = l.Tomar(original);
    s.original.assign(datos.begin(), datos.end());
    // Reading the whole range first avoids allocations with a truncated file.
    Lector codigo{l.Tomar(size_t(palabras) * 4)};
    s.spirv.reserve(palabras);
    for (uint32_t j = 0; j < palabras; ++j) s.spirv.push_back(codigo.U32());
    Validar(s);
    Exigir(s.huella == esperada, "Huella de contenedor incorrecta");
    if (!nuevos.empty()) Exigir(Menor(nuevos.back(), s), "Entradas repetidas o desordenadas");
    nuevos.push_back(std::move(s));
  }
  Exigir(l.posicion == archivo.size(), "Datos sobrantes en el paquete de shaders");
  shaders_ = std::move(nuevos);
}

void BibliotecaShaders::Cargar(const std::filesystem::path& archivo) {
  std::ifstream f(archivo, std::ios::binary | std::ios::ate);
  Exigir(bool(f), "No se pudo abrir el paquete de shaders");
  const auto n = f.tellg();
  Exigir(n >= 24 && n <= std::streamoff(kMaxArchivo), "Tamano del paquete fuera de limites");
  std::vector<uint8_t> datos(static_cast<size_t>(n));
  f.seekg(0);
  Exigir(bool(f.read(reinterpret_cast<char*>(datos.data()), datos.size())), "Lectura incompleta del paquete");
  Cargar(datos);
}

const Shader* BibliotecaShaders::Buscar(std::span<const uint8_t> original) const {
  if (original.size() < 24 || original.size() > kMaxOriginal) return nullptr;
  const uint64_t huella = XXH3_64bits(original.data(), original.size());
  auto it = std::lower_bound(shaders_.begin(), shaders_.end(), huella,
      [](const Shader& s, uint64_t h) { return s.huella < h; });
  // A hash collision can never pick another shader: the bytes are compared.
  for (; it != shaders_.end() && it->huella == huella; ++it)
    if (it->original.size() == original.size() &&
        std::equal(original.begin(), original.end(), it->original.begin())) return &*it;
  return nullptr;
}


namespace {
uint32_t LeerBE(std::span<const uint8_t> d, size_t p) {
  Exigir(p + 4 <= d.size(), "Contenedor 2005 truncado");
  return uint32_t(d[p]) << 24 | uint32_t(d[p + 1]) << 16 | uint32_t(d[p + 2]) << 8 | d[p + 3];
}
void PonerBE(std::vector<uint8_t>& d, uint32_t v) {
  for (int i = 3; i >= 0; --i) d.push_back(uint8_t(v >> (i * 8)));
}
void CambiarBE(std::vector<uint8_t>& d, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i) d[p + i] = uint8_t(v >> ((3 - i) * 8));
}
}  // namespace

bool EsFormato2005(std::span<const uint8_t> o) {
  return o.size() >= 24 && (LeerBE(o, 0) & ~1u) == 0x102A0E00u;
}

// The 2005 layout (read from Condemned's 342 containers):
//   header (6 words): flags, virtualSize, physicalSize, definitionTableOffset (0: none), constantTableOffset,
//                     shaderOffset
//   definition table: 4 words, size, one word we do not use, then entries { register:16 | count:16, values } up to
//                     a zero word. Float literals: the register is a byte offset in the constant file, 16 bytes a
//                     register counted from c208, vertex constants at 0x1000 and pixel constants at 0x2000 (0x12F0 and 0x22F0
//                     are c255: the compiler puts literals at the top), count
//                     in floats. Integer constants: 0x2320 + 4 * i with one word per value, as in 2008.
//   shader:           PS { ?, ?, 0, 0, interpolatorInfo, ?, ?, outputs, interpolators[] }
//                     VS { ?, 0, 0, 0, interpolatorInfo, 0, 1, elementCount, ?, 0, ?, elements[], interpolators[] }
//                     (elements and interpolators encoded as in 2008)
//   physical part:    the microcode only (a multiple of 12 bytes); the literals live in the definition table.
// The 2008 container keeps the constant table as it is (its offsets are relative to itself), moves the literals into
// the physical part after the microcode and writes new Shader and DefinitionTable structures.
std::vector<uint8_t> Normalizar(std::span<const uint8_t> o) {
  if (!EsFormato2005(o)) return {o.begin(), o.end()};
  const uint32_t flags = LeerBE(o, 0), virt = LeerBE(o, 4), phys = LeerBE(o, 8);
  const uint32_t def = LeerBE(o, 12), ctab = LeerBE(o, 16), sh = LeerBE(o, 20);
  const bool vs = flags & 1;
  Exigir(virt >= 24 && uint64_t(virt) + phys == o.size() && phys % 12 == 0, "Contenedor 2005 incoherente");
  Exigir(ctab >= 24 && ctab < virt && sh >= 24 && sh + 0x20 <= virt && (!def || (def >= 24 && def < virt)),
         "Offsets del contenedor 2005 fuera de la parte virtual");
  constexpr uint32_t kDelta = 0x24 - 0x18;  // the 2008 header is 3 words longer

  // Literals: (register in the 2008 numbering, 4 words each).
  struct Literal { uint32_t reg; uint32_t count; std::vector<uint32_t> valores; };
  std::vector<Literal> literales;
  std::vector<uint32_t> enteros;  // int4 definitions as in 2008: { register:16 | count:16, values[count] }...
  if (def) {
    size_t p = def + 24;  // 5 words of header and one more we do not use
    while (p + 4 <= virt) {
      const uint32_t rc = LeerBE(o, p);
      if (rc == 0) break;
      const uint32_t base = rc >> 16, n = rc & 0xFFFF;
      Exigir(n && n <= 1024 && p + 4 + size_t(n) * 4 <= virt, "Definicion 2005 fuera de limites");
      if (base >= 0x2320 && base < 0x23A0) {  // integer (loop) constants i0..i31: 0x2320 + 4 * i
        // XenosRecomp steps over 2 + count words per int4 entry: the 2008 entries carry one more word.
        for (uint32_t i = 0; i <= n; ++i) enteros.push_back(LeerBE(o, p + i * 4));
        enteros.push_back(0);
      } else {
        Literal l;
        l.reg = 208 + ((base & 0x0FFF) >> 4) + (vs ? 0 : 256);  // 0x22F0 / 0x12F0 = c255: offset 0 is c208
        l.count = n;
        for (uint32_t i = 0; i < n; ++i) l.valores.push_back(LeerBE(o, p + 4 + i * 4));
        while (l.valores.size() % 4) l.valores.push_back(0);
        literales.push_back(std::move(l));
      }
      p += 4 + size_t(n) * 4;
    }
  }

  const uint32_t interp = LeerBE(o, sh + 16);
  const uint32_t ninterp = (interp >> 5) & 0x1F;
  std::vector<uint32_t> cuerpo;  // the 2008 Shader structure
  cuerpo.push_back(0);           // physicalOffset: the microcode starts the physical part
  cuerpo.push_back(phys);        // size
  cuerpo.push_back(0);
  cuerpo.push_back(0xFF00);      // fieldC: no svPos register (the 2005 header has none we can read)
  cuerpo.push_back(0);
  cuerpo.push_back(interp);
  if (vs) {
    const uint32_t nelem = LeerBE(o, sh + 28);
    Exigir(nelem <= 64 && sh + 44 + size_t(nelem + ninterp) * 4 <= virt, "Elementos 2005 fuera de limites");
    cuerpo.push_back(0);         // field18: no words before the elements
    cuerpo.push_back(nelem);
    cuerpo.push_back(0);
    for (uint32_t i = 0; i < nelem + ninterp; ++i) cuerpo.push_back(LeerBE(o, sh + 44 + i * 4));
  } else {
    Exigir(sh + 32 + size_t(ninterp) * 4 <= virt, "Interpoladores 2005 fuera de limites");
    cuerpo.push_back(0);
    cuerpo.push_back(LeerBE(o, sh + 28));  // outputs
    for (uint32_t i = 0; i < ninterp; ++i) cuerpo.push_back(LeerBE(o, sh + 32 + i * 4));
  }

  std::vector<uint8_t> r;
  r.reserve(o.size() + 256);
  for (int i = 0; i < 9; ++i) PonerBE(r, 0);  // header, filled below
  r.insert(r.end(), o.begin() + 24, o.begin() + virt);  // definition table, constant table, old shader
  while (r.size() % 4) r.push_back(0);
  const uint32_t nuevoShader = uint32_t(r.size());
  for (uint32_t w : cuerpo) PonerBE(r, w);
  uint32_t nuevaDef = 0;
  if (!literales.empty() || !enteros.empty()) {
    nuevaDef = uint32_t(r.size());
    for (int i = 0; i < 4; ++i) PonerBE(r, 0);
    PonerBE(r, uint32_t(literales.size() * 8 + enteros.size() * 4 + 8));
    uint32_t offset = phys;  // literals after the microcode, in the physical part
    for (const auto& l : literales) {
      PonerBE(r, l.reg << 16 | l.count);
      PonerBE(r, offset);
      offset += uint32_t(l.valores.size() * 4);
    }
    PonerBE(r, 0);  // end of the float4 definitions
    for (uint32_t w : enteros) PonerBE(r, w);
    PonerBE(r, 0);  // end of the int4 definitions
  }
  const uint32_t nuevaVirt = uint32_t(r.size());
  r.insert(r.end(), o.begin() + virt, o.end());  // microcode
  for (const auto& l : literales)
    for (uint32_t v : l.valores) PonerBE(r, v);
  CambiarBE(r, 0, 0x102A1100u | (vs ? 1u : 0u));
  CambiarBE(r, 4, nuevaVirt);
  CambiarBE(r, 8, uint32_t(r.size() - nuevaVirt));
  CambiarBE(r, 12, 0);
  CambiarBE(r, 16, ctab + kDelta);
  CambiarBE(r, 20, nuevaDef);
  CambiarBE(r, 24, nuevoShader);
  return r;
}

std::vector<uint8_t> EmpaquetarShaders(std::vector<Shader> shaders) {
  Exigir(!shaders.empty() && shaders.size() <= kMaxShaders, "Cantidad de shaders fuera de limites");
  for (auto& s : shaders) Validar(s);
  std::sort(shaders.begin(), shaders.end(), Menor);
  std::vector<uint8_t> cuerpo;
  uint32_t cantidad = 0;
  const Shader* anterior = nullptr;
  for (const auto& s : shaders) {
    if (anterior && anterior->original == s.original) {
      Exigir(anterior->spirv == s.spirv, "Un contenedor tiene dos traducciones distintas");
      continue;
    }
    const size_t n = 16 + s.original.size() + s.spirv.size() * 4;
    Exigir(n <= kMaxArchivo - 24 - cuerpo.size(), "Paquete demasiado grande");
    U32(cuerpo, uint32_t(s.original.size())); U32(cuerpo, uint32_t(s.spirv.size()));
    U64(cuerpo, s.huella);
    cuerpo.insert(cuerpo.end(), s.original.begin(), s.original.end());
    for (uint32_t palabra : s.spirv) U32(cuerpo, palabra);
    ++cantidad;
    anterior = &s;
  }
  std::vector<uint8_t> archivo(kFirma.begin(), kFirma.end());
  U32(archivo, 1); U32(archivo, cantidad);
  U64(archivo, XXH3_64bits(cuerpo.data(), cuerpo.size()));
  archivo.insert(archivo.end(), cuerpo.begin(), cuerpo.end());
  return archivo;
}
}  // namespace ds
