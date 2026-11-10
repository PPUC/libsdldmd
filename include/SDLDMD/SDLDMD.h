#pragma once

#define SDLDMD_VERSION_MAJOR 0
#define SDLDMD_VERSION_MINOR 2
#define SDLDMD_VERSION_PATCH 0

#ifdef _MSC_VER
#define SDLDMDAPI __declspec(dllexport)
#else
#define SDLDMDAPI __attribute__((visibility("default")))
#endif

#include <cstdint>
#include <mutex>
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

  // Called by libdmdutil from its own output thread. It only keeps the newest
  // frame: the window and its GL context belong to the thread that created
  // them, and drawing from any other thread fails without a word on KMS/DRM
  // with OpenGL ES, which left a Raspberry Pi's screen showing whatever was on
  // it before.
  void Update(uint8_t* pData, uint16_t width = 0, uint16_t height = 0) override;

  // Draws and presents the newest frame, if one arrived since the last call.
  // Must be called on the thread that created this SDLDMD, as often as frames
  // should appear - typically once per pass of the application's main loop.
  // Returns true when a frame was presented.
  SDLDMDAPI bool Render();


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

  // The frame handed over by Update(), guarded by m_frameMutex. Render() copies
  // it out before drawing, so the output thread never waits for a present.
  std::mutex m_frameMutex;
  std::vector<uint8_t> m_pendingFrame;
  bool m_framePending = false;
  std::vector<uint8_t> m_renderFrame;

  // What was asked for, in window coordinates relative to the screen. Where
  // the window really is that size, as on a desktop, the DMD fills it. Where
  // it is not - fullscreen, or KMS/DRM, which cannot size or place a window at
  // all - the DMD is drawn into this rectangle of the screen instead.
  int m_requestedWidth = 0;
  int m_requestedHeight = 0;
  int m_requestedX = SDL_WINDOWPOS_UNDEFINED;
  int m_requestedY = SDL_WINDOWPOS_UNDEFINED;

  bool CreateRendererWithFallbacks();
  SDL_FRect GetTargetRect(uint16_t width, uint16_t height) const;
  void RenderDots(uint8_t* pData, uint16_t width, uint16_t height, const SDL_FRect& target);
  void RenderSquares(uint8_t* pData, uint16_t width, uint16_t height, const SDL_FRect& target);
  void RenderScaledDots(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor, const SDL_FRect& target);
  void RenderScaledSquares(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor,
                           const SDL_FRect& target);
  void RenderScaledNearest(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor,
                           const SDL_FRect& target);
  void RenderSmoothScaling(uint8_t* pData, uint16_t width, uint16_t height, const SDL_FRect& target);
  void RenderXBRZ(uint8_t* pData, uint16_t width, uint16_t height, const SDL_FRect& target);
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
