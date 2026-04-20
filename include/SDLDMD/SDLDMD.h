#pragma once

#define SDLDMD_VERSION_MAJOR 0
#define SDLDMD_VERSION_MINOR 1
#define SDLDMD_VERSION_PATCH 0

#ifdef _MSC_VER
#define SDLDMDAPI __declspec(dllexport)
#else
#define SDLDMDAPI __attribute__((visibility("default")))
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "DMDUtil/DMD.h"
#include "DMDUtil/RGB24DMD.h"

#include "SDLDMD/SDLDMDConfig.h"

namespace DMDUtil
{

class SDLDMD : public RGB24DMD
{
 public:
  enum RenderingMode : int
  {
    Dots = 0,
    Square = 1,
    Scale2x = 2,
    Scale4x = 3,
    Scale2xDots = 4,
    Scale4xDots = 5,
    Scale2xSquares = 6,
    Scale4xSquares = 7,
    SmoothScaling = 8,
    XBRZ = 9,
  };
  enum Rotation : int
  {
    Rotate0 = 0,
    Rotate90 = 1,
    Rotate180 = 2,
    Rotate270 = 3,
  };

  SDLDMD(const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags, uint16_t width,
         uint16_t height, int screenIndex = -1, int windowX = SDL_WINDOWPOS_UNDEFINED,
         int windowY = SDL_WINDOWPOS_UNDEFINED,
         RenderingMode renderingMode = RenderingMode::Dots, Rotation rotation = Rotation::Rotate0,
         const char* preferredVideoDriver = nullptr);

  ~SDLDMD() override;

  void Update(uint8_t* pData, uint16_t width = 0, uint16_t height = 0) override;

  void SetRenderingMode(RenderingMode mode) { m_renderingMode = mode; }
  bool IsReady() const { return m_pRenderer != nullptr; }
  const char* GetError() const { return m_error.c_str(); }
  const char* GetRendererName() const { return m_rendererName.c_str(); }
  bool IsHardwareAccelerated() const { return !m_rendererName.empty() && m_rendererName != "software"; }

 private:
  void RotateFrameIfNeeded(uint8_t* pData, uint16_t width, uint16_t height, uint8_t** ppRenderData, uint16_t* pRenderWidth,
                           uint16_t* pRenderHeight);
  SDL_Window* m_pWindow = nullptr;
  SDL_Renderer* m_pRenderer;
  std::string m_error;
  std::string m_rendererName;
  int m_renderingMode = RenderingMode::Dots;
  Rotation m_rotation = Rotation::Rotate0;
  bool m_managesSDLVideo = false;
  bool m_registeredSDLInstance = false;
  std::vector<uint8_t> m_rotatedBuffer;

  bool CreateRendererWithFallbacks();
  void RenderDots(uint8_t* pData, uint16_t width, uint16_t height);
  void RenderSquares(uint8_t* pData, uint16_t width, uint16_t height);
  void RenderScaledDots(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor);
  void RenderScaledSquares(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor);
  void RenderScaledNearest(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor);
  void RenderSmoothScaling(uint8_t* pData, uint16_t width, uint16_t height);
  void RenderXBRZ(uint8_t* pData, uint16_t width, uint16_t height);
};

SDLDMDAPI SDLDMD* CreateSDLDMD(DMD& dmd, const char* title, uint16_t windowWidth, uint16_t windowHeight,
                               uint32_t windowFlags, uint16_t width, uint16_t height,
                               int screenIndex = -1, int windowX = SDL_WINDOWPOS_UNDEFINED,
                               int windowY = SDL_WINDOWPOS_UNDEFINED,
                               SDLDMD::RenderingMode renderingMode = SDLDMD::RenderingMode::Dots,
                               SDLDMD::Rotation rotation = SDLDMD::Rotation::Rotate0,
                               const char* preferredVideoDriver = nullptr);
SDLDMDAPI SDLDMD* CreateSDLDMD(DMD& dmd, const char* title, uint16_t windowWidth, uint16_t windowHeight,
                               uint32_t windowFlags, uint16_t width, uint16_t height,
                               int screenIndex, int windowX, int windowY,
                               SDLDMD::RenderingMode renderingMode,
                               const char* preferredVideoDriver);
SDLDMDAPI SDLDMDConfig* InstallSDLDMDConfig();
SDLDMDAPI SDLDMD* CreateSDLDMDFromConfig(DMD& dmd, const char* title, uint16_t width = 128, uint16_t height = 32,
                                         uint32_t windowFlags = 0,
                                         const char* preferredVideoDriver = nullptr);
SDLDMDAPI bool DestroySDLDMD(DMD& dmd, SDLDMD* pSDLDMD);
SDLDMDAPI bool SetSDLDMDRenderingMode(SDLDMD* pSDLDMD, SDLDMD::RenderingMode renderingMode);
SDLDMDAPI bool ParseSDLDMDRenderingMode(const char* value, SDLDMD::RenderingMode* pRenderingMode);
SDLDMDAPI bool ParseSDLDMDRotation(const char* value, SDLDMD::Rotation* pRotation);

}  // namespace DMDUtil
