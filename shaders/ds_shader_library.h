#pragma once

// The deko3d shader library (ds2_shaders.dslib): each original Xenos container with its SPIR-V. Built on the PC
// by ds_empaquetar, read by deadspace2/src/ds2_shaders.cpp on the console and by dksh/ds_dksh.

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace ds {

// The original container also identifies constants and interface. The physical
// code alone is not enough. It is looked up before the driver creates its object.
struct Shader {
  std::vector<uint8_t> original;
  // The container in the 2008 layout every reader here expects: the original itself, or for the 2005 format
  // (Condemned, 0x102A0E00/01) its conversion (Normalizar). Lookups still use `original`.
  std::vector<uint8_t> normalizado;
  std::vector<uint32_t> spirv;
  uint64_t huella = 0;
  bool vertices = false;
};

class BibliotecaShaders {
 public:
  // Transactional load: an invalid file does not replace the library.
  // Pointers returned by Buscar last until the next successful load.
  void Cargar(std::span<const uint8_t> archivo);
  void Cargar(const std::filesystem::path& archivo);
  const Shader* Buscar(std::span<const uint8_t> original) const;
  const std::vector<Shader>& shaders() const { return shaders_; }

 private:
  std::vector<Shader> shaders_;
};

// A 2005-format container (XDK of 2005, signature 0x102A0E00 PS / 0x102A0E01 VS) rewritten in the 2008 layout
// (XenosRecomp's shader.h: 9-word header, Shader/PixelShader/VertexShader, definitions with physical offsets);
// a 2008 container is returned unchanged. Throws std::runtime_error on a malformed container.
std::vector<uint8_t> Normalizar(std::span<const uint8_t> original);
bool EsFormato2005(std::span<const uint8_t> original);

// Versioned local format, little-endian, with integrity checks.
// It contains private data derived from the game; it is not embedded in the executable.
std::vector<uint8_t> EmpaquetarShaders(std::vector<Shader> shaders);

}  // namespace ds
