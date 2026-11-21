#include "SDLDMD/SDLDMD.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "DMDUtil/Config.h"
#include "DMDUtil/Logger.h"
#include "xbrz/xbrz.h"

namespace DMDUtil
{
namespace
{
std::mutex g_sdldmdSdlMutex;
int g_sdldmdInstanceCount = 0;
bool g_sdldmdOwnsVideoSubsystem = false;
SDLDMDConfig* g_installedSDLDMDConfig = nullptr;

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

const char* RenderingModeToString(SDLDMD::RenderingMode renderingMode)
{
  switch (renderingMode)
  {
    case SDLDMD::RenderingMode::Dots:
      return "dots";
    case SDLDMD::RenderingMode::Square:
      return "squares";
    case SDLDMD::RenderingMode::Scale2x:
      return "scale2x";
    case SDLDMD::RenderingMode::Scale4x:
      return "scale4x";
    case SDLDMD::RenderingMode::Scale2xDots:
      return "scale2x-dots";
    case SDLDMD::RenderingMode::Scale4xDots:
      return "scale4x-dots";
    case SDLDMD::RenderingMode::Scale2xSquares:
      return "scale2x-squares";
    case SDLDMD::RenderingMode::Scale4xSquares:
      return "scale4x-squares";
    case SDLDMD::RenderingMode::SmoothScaling:
      return "smooth";
    case SDLDMD::RenderingMode::XBRZ:
      return "xbrz";
  }

  return "unknown";
}

const char* RotationToString(SDLDMD::Rotation rotation)
{
  switch (rotation)
  {
    case SDLDMD::Rotation::Rotate0:
      return "0";
    case SDLDMD::Rotation::Rotate90:
      return "90";
    case SDLDMD::Rotation::Rotate180:
      return "180";
    case SDLDMD::Rotation::Rotate270:
      return "270";
  }

  return "unknown";
}

bool SetEnvIfUnset(const char* name, const char* value)
{
  if (name == nullptr || value == nullptr) return false;
  if (std::getenv(name) != nullptr) return true;
#if defined(_WIN32) || defined(_WIN64)
  return _putenv_s(name, value) == 0;
#else
  return setenv(name, value, 0) == 0;
#endif
}

const char* GetDefaultVideoDriver(const char* preferredVideoDriver)
{
#if defined(__linux__)
  if (preferredVideoDriver && *preferredVideoDriver)
  {
    return preferredVideoDriver;
  }

  if (SDL_getenv("SDL_VIDEODRIVER") == nullptr && SDL_getenv("DISPLAY") == nullptr &&
      SDL_getenv("WAYLAND_DISPLAY") == nullptr)
  {
    return "kmsdrm";
  }
#else
  (void)preferredVideoDriver;
#endif
  return preferredVideoDriver;
}

SDLDMDConfig* GetInstalledSDLDMDConfig() { return g_installedSDLDMDConfig; }

bool ResolveWindowPositionForScreen(int screenIndex, int windowX, int windowY, int* pResolvedX, int* pResolvedY)
{
  if (pResolvedX == nullptr || pResolvedY == nullptr)
  {
    return false;
  }

  *pResolvedX = windowX;
  *pResolvedY = windowY;
  if (screenIndex < 0)
  {
    return true;
  }

  int numDisplays = 0;
  SDL_DisplayID* pDisplays = SDL_GetDisplays(&numDisplays);
  if (pDisplays == nullptr)
  {
    return false;
  }

  const bool validDisplay = screenIndex < numDisplays;
  SDL_DisplayID displayId = 0;
  if (validDisplay)
  {
    displayId = pDisplays[screenIndex];
  }
  SDL_free(pDisplays);

  if (!validDisplay)
  {
    SDL_SetError("Invalid screen index %d", screenIndex);
    return false;
  }

  SDL_Rect displayBounds;
  if (!SDL_GetDisplayBounds(displayId, &displayBounds))
  {
    return false;
  }

  const int relativeX = (windowX == SDL_WINDOWPOS_UNDEFINED) ? 0 : windowX;
  const int relativeY = (windowY == SDL_WINDOWPOS_UNDEFINED) ? 0 : windowY;
  *pResolvedX = displayBounds.x + relativeX;
  *pResolvedY = displayBounds.y + relativeY;
  return true;
}
}  // namespace

bool SDLDMD::CreateRendererWithFallbacks()
{
  // Desktop OpenGL first where there is a desktop. Under KMS/DRM (zedmdos on a
  // Raspberry Pi) there is only OpenGL ES: trying OpenGL first there built and
  // threw away a failed context on every start, and on a Pi that is a
  // noticeable part of bringing the DMD up.
  static const char* kDesktopCandidates[] = {"opengl", "opengles2", nullptr};
  static const char* kKmsdrmCandidates[] = {"opengles2", "opengl", nullptr};
  const char* const videoDriver = SDL_GetCurrentVideoDriver();
  const bool kmsdrm = videoDriver != nullptr && SDL_strcasecmp(videoDriver, "kmsdrm") == 0;
  const char* const* candidates = kmsdrm ? kKmsdrmCandidates : kDesktopCandidates;

  std::string lastError;
  for (const char* const* candidate = candidates;; ++candidate)
  {
    const char* const rendererName = *candidate;
    m_pRenderer = SDL_CreateRenderer(m_pWindow, rendererName);
    if (m_pRenderer)
    {
      const char* const selectedRendererName = SDL_GetRendererName(m_pRenderer);
      m_rendererName = selectedRendererName ? selectedRendererName : "";
      return true;
    }

    if (const char* const error = SDL_GetError(); error && *error)
    {
      if (!lastError.empty()) lastError += "; ";
      lastError += rendererName ? rendererName : "default";
      lastError += ": ";
      lastError += error;
    }
    // The list ends with nullptr, SDL's own choice, tried last.
    if (rendererName == nullptr) break;
  }

  m_error = lastError.empty() ? "SDL couldn't create a renderer." : lastError;
  return false;
}

SDLDMD::SDLDMD(const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags, uint16_t width,
               uint16_t height, int screenIndex, int windowX, int windowY, RenderingMode renderingMode,
               Rotation rotation, const char* preferredVideoDriver)
    : RGB24DMD(width, height),
      m_pRenderer(nullptr),
      m_renderingMode(renderingMode),
      m_rotation(rotation),
      m_requestedWidth(windowWidth),
      m_requestedHeight(windowHeight),
      m_requestedX(windowX),
      m_requestedY(windowY)
{
  {
    std::lock_guard<std::mutex> lock(g_sdldmdSdlMutex);
    const char* const videoDriver = GetDefaultVideoDriver(preferredVideoDriver);
    if (videoDriver && *videoDriver && SDL_getenv("SDL_VIDEODRIVER") == nullptr)
    {
      SetEnvIfUnset("SDL_VIDEODRIVER", videoDriver);
    }

    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
    {
      if (!SDL_Init(SDL_INIT_VIDEO))
      {
        m_error = SDL_GetError();
        return;
      }
      g_sdldmdOwnsVideoSubsystem = true;
      m_managesSDLVideo = true;
    }

    ++g_sdldmdInstanceCount;
    m_registeredSDLInstance = true;
  }

  m_pWindow = SDL_CreateWindow(title, windowWidth, windowHeight, windowFlags);
  if (!m_pWindow)
  {
    m_error = SDL_GetError();
    m_pWindow = nullptr;
    m_pRenderer = nullptr;
    return;
  }

  int resolvedWindowX = windowX;
  int resolvedWindowY = windowY;
  if (!ResolveWindowPositionForScreen(screenIndex, windowX, windowY, &resolvedWindowX, &resolvedWindowY))
  {
    m_error = SDL_GetError();
    SDL_DestroyWindow(m_pWindow);
    m_pWindow = nullptr;
    m_pRenderer = nullptr;
    return;
  }

  const bool hasExplicitWindowPosition =
      screenIndex >= 0 || windowX != SDL_WINDOWPOS_UNDEFINED || windowY != SDL_WINDOWPOS_UNDEFINED;
  if (hasExplicitWindowPosition)
  {
    SDL_SetWindowPosition(m_pWindow, resolvedWindowX, resolvedWindowY);
  }

  if (!CreateRendererWithFallbacks())
  {
    SDL_DestroyWindow(m_pWindow);
    m_pWindow = nullptr;
    m_pRenderer = nullptr;
    return;
  }

  if (!hasExplicitWindowPosition)
  {
    SDL_SetWindowPosition(m_pWindow, 0, 0);
  }
  while (!SDL_SyncWindow(m_pWindow));

  Log(DMDUtil_LogLevel_INFO, "SDLDMD rotation=%s", RotationToString(m_rotation));
}

SDLDMD::~SDLDMD()
{
  if (m_pFrameTexture) SDL_DestroyTexture(m_pFrameTexture);
  if (m_pMask) SDL_DestroyTexture(m_pMask);
  if (m_pIntermediate) SDL_DestroyTexture(m_pIntermediate);
  if (m_pXbrzTexture) SDL_DestroyTexture(m_pXbrzTexture);
  m_pFrameTexture = nullptr;
  m_pMask = nullptr;
  m_pIntermediate = nullptr;
  m_pXbrzTexture = nullptr;
  if (m_pRenderer) SDL_DestroyRenderer(m_pRenderer);
  if (m_pWindow) SDL_DestroyWindow(m_pWindow);
  m_pRenderer = nullptr;
  m_pWindow = nullptr;

  std::lock_guard<std::mutex> lock(g_sdldmdSdlMutex);
  if (m_registeredSDLInstance)
  {
    --g_sdldmdInstanceCount;
    m_registeredSDLInstance = false;
  }
  if (m_managesSDLVideo && g_sdldmdOwnsVideoSubsystem && g_sdldmdInstanceCount == 0)
  {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    g_sdldmdOwnsVideoSubsystem = false;
    m_managesSDLVideo = false;
  }
}

SDLDMD* CreateSDLDMD(DMD& dmd, const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags,
                     uint16_t width, uint16_t height, int screenIndex, int windowX, int windowY,
                     SDLDMD::RenderingMode renderingMode, SDLDMD::Rotation rotation, const char* preferredVideoDriver)
{
  const char* const requestedVideoDriver = GetDefaultVideoDriver(preferredVideoDriver);
  Log(DMDUtil_LogLevel_INFO,
      "SDLDMD setup: title='%s' window=%ux%u dmd=%ux%u screen=%d x=%d y=%d renderer=%s rotation=%s videoDriver=%s "
      "flags=0x%x",
      title ? title : "", windowWidth, windowHeight, width, height, screenIndex, windowX, windowY,
      RenderingModeToString(renderingMode), RotationToString(rotation),
      (requestedVideoDriver && *requestedVideoDriver) ? requestedVideoDriver : "default", windowFlags);

  SDLDMD* const pSDLDMD = new SDLDMD(title, windowWidth, windowHeight, windowFlags, width, height, screenIndex, windowX,
                                     windowY, renderingMode, rotation, preferredVideoDriver);
  if (!pSDLDMD->IsReady())
  {
    SDL_SetError("%s", pSDLDMD->GetError() ? pSDLDMD->GetError() : "Unknown SDL DMD error");
    delete pSDLDMD;
    return nullptr;
  }

  dmd.AddRGB24DMD(pSDLDMD);
  const char* const actualVideoDriver = SDL_GetCurrentVideoDriver();
  Log(DMDUtil_LogLevel_INFO, "SDLDMD setup succeeded: renderer=%s hardware=%d videoDriver=%s",
      pSDLDMD->GetRendererName() ? pSDLDMD->GetRendererName() : "", pSDLDMD->IsHardwareAccelerated() ? 1 : 0,
      (actualVideoDriver && *actualVideoDriver) ? actualVideoDriver : "unknown");
  return pSDLDMD;
}

SDLDMD* CreateSDLDMD(DMD& dmd, const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags,
                     uint16_t width, uint16_t height, int screenIndex, int windowX, int windowY,
                     SDLDMD::RenderingMode renderingMode, const char* preferredVideoDriver)
{
  return CreateSDLDMD(dmd, title, windowWidth, windowHeight, windowFlags, width, height, screenIndex, windowX, windowY,
                      renderingMode, SDLDMD::Rotation::Rotate0, preferredVideoDriver);
}

SDLDMDConfig* InstallSDLDMDConfig()
{
  if (SDLDMDConfig* const pConfig = GetInstalledSDLDMDConfig())
  {
    Log(DMDUtil_LogLevel_INFO, "LCD-DMD config already installed");
    return pConfig;
  }

  SDLDMDConfig* const pConfig = new SDLDMDConfig();
  Config::SetInstance(pConfig);
  g_installedSDLDMDConfig = pConfig;
  Log(DMDUtil_LogLevel_INFO, "Installed LCD-DMD config");
  return pConfig;
}

SDLDMD* CreateSDLDMDFromConfig(DMD& dmd, const char* title, uint16_t width, uint16_t height, uint32_t windowFlags,
                               const char* preferredVideoDriver)
{
  constexpr uint16_t kHDWidth = 256;
  constexpr uint16_t kHDHeight = 64;

  SDL_ClearError();
  SDLDMDConfig* const pConfig = GetInstalledSDLDMDConfig();
  if (pConfig == nullptr)
  {
    Log(DMDUtil_LogLevel_ERROR, "LCD-DMD config is not installed");
    return nullptr;
  }

  if (!pConfig->IsLCDDMDEnabled())
  {
    Log(DMDUtil_LogLevel_INFO, "LCD-DMD output disabled in config");
    return nullptr;
  }

  if (pConfig->GetLCDDMDWidth() <= 0 || pConfig->GetLCDDMDHeight() <= 0)
  {
    SDL_SetError("Invalid LCD-DMD size %dx%d", pConfig->GetLCDDMDWidth(), pConfig->GetLCDDMDHeight());
    return nullptr;
  }

  SDLDMD::RenderingMode renderingMode = SDLDMD::RenderingMode::Dots;
  if (!ParseSDLDMDRenderingMode(pConfig->GetLCDDMDRenderer(), &renderingMode))
  {
    SDL_SetError("Unsupported LCD-DMD renderer '%s'", pConfig->GetLCDDMDRenderer());
    return nullptr;
  }

  SDLDMD::Rotation rotation = SDLDMD::Rotation::Rotate0;
  if (!ParseSDLDMDRotation(std::to_string(pConfig->GetLCDDMDRotation()).c_str(), &rotation))
  {
    SDL_SetError("Unsupported LCD-DMD rotation '%d'", pConfig->GetLCDDMDRotation());
    return nullptr;
  }

  const uint16_t logicalWidth = pConfig->IsLCDDMDHD() ? kHDWidth : width;
  const uint16_t logicalHeight = pConfig->IsLCDDMDHD() ? kHDHeight : height;

  Log(DMDUtil_LogLevel_INFO,
      "Creating LCD-DMD output: %dx%d screen=%d x=%d y=%d renderer=%s rotation=%d hd=%d logical=%ux%u",
      pConfig->GetLCDDMDWidth(), pConfig->GetLCDDMDHeight(), pConfig->GetLCDDMDScreen(), pConfig->GetLCDDMDX(),
      pConfig->GetLCDDMDY(), pConfig->GetLCDDMDRenderer(), pConfig->GetLCDDMDRotation(), pConfig->IsLCDDMDHD() ? 1 : 0,
      logicalWidth, logicalHeight);

  return CreateSDLDMD(dmd, title, static_cast<uint16_t>(pConfig->GetLCDDMDWidth()),
                      static_cast<uint16_t>(pConfig->GetLCDDMDHeight()), windowFlags, logicalWidth, logicalHeight,
                      pConfig->GetLCDDMDScreen(), pConfig->GetLCDDMDX(), pConfig->GetLCDDMDY(), renderingMode, rotation,
                      preferredVideoDriver);
}

bool DestroySDLDMD(DMD& dmd, SDLDMD* pSDLDMD) { return dmd.DestroyRGB24DMD(pSDLDMD); }

bool SetSDLDMDRenderingMode(SDLDMD* pSDLDMD, SDLDMD::RenderingMode renderingMode)
{
  if (pSDLDMD == nullptr) return false;
  pSDLDMD->SetRenderingMode(renderingMode);
  return true;
}

bool ParseSDLDMDRenderingMode(const char* value, SDLDMD::RenderingMode* pRenderingMode)
{
  if (value == nullptr || pRenderingMode == nullptr) return false;

  const std::string renderer = ToLower(value);
  if (renderer == "dots")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Dots;
    return true;
  }
  if (renderer == "squares" || renderer == "square")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Square;
    return true;
  }
  if (renderer == "scale2x")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale2x;
    return true;
  }
  if (renderer == "scale4x")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale4x;
    return true;
  }
  if (renderer == "scale2x-dots" || renderer == "scale2xdots")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale2xDots;
    return true;
  }
  if (renderer == "scale4x-dots" || renderer == "scale4xdots")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale4xDots;
    return true;
  }
  if (renderer == "scale2x-squares" || renderer == "scale2xsquares")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale2xSquares;
    return true;
  }
  if (renderer == "scale4x-squares" || renderer == "scale4xsquares")
  {
    *pRenderingMode = SDLDMD::RenderingMode::Scale4xSquares;
    return true;
  }
  if (renderer == "smooth" || renderer == "smoothscaling" || renderer == "smooth-scaling")
  {
    *pRenderingMode = SDLDMD::RenderingMode::SmoothScaling;
    return true;
  }
  if (renderer == "xbrz")
  {
    *pRenderingMode = SDLDMD::RenderingMode::XBRZ;
    return true;
  }

  return false;
}

bool ParseSDLDMDRotation(const char* value, SDLDMD::Rotation* pRotation)
{
  if (value == nullptr || pRotation == nullptr) return false;

  const std::string rotation = ToLower(value);
  if (rotation == "0")
  {
    *pRotation = SDLDMD::Rotation::Rotate0;
    return true;
  }
  if (rotation == "90")
  {
    *pRotation = SDLDMD::Rotation::Rotate90;
    return true;
  }
  if (rotation == "180")
  {
    *pRotation = SDLDMD::Rotation::Rotate180;
    return true;
  }
  if (rotation == "270")
  {
    *pRotation = SDLDMD::Rotation::Rotate270;
    return true;
  }

  return false;
}

void SDLDMD::RotateFrameIfNeeded(uint8_t* pData, uint16_t width, uint16_t height, uint8_t** ppRenderData,
                                 uint16_t* pRenderWidth, uint16_t* pRenderHeight)
{
  if (ppRenderData == nullptr || pRenderWidth == nullptr || pRenderHeight == nullptr)
  {
    return;
  }

  *ppRenderData = pData;
  *pRenderWidth = width;
  *pRenderHeight = height;

  if (m_rotation == Rotation::Rotate0 || pData == nullptr)
  {
    return;
  }

  const bool quarterTurn = m_rotation == Rotation::Rotate90 || m_rotation == Rotation::Rotate270;
  const uint16_t rotatedWidth = quarterTurn ? height : width;
  const uint16_t rotatedHeight = quarterTurn ? width : height;
  m_rotatedBuffer.assign(static_cast<size_t>(rotatedWidth) * rotatedHeight * 3u, 0);

  for (uint16_t y = 0; y < height; ++y)
  {
    for (uint16_t x = 0; x < width; ++x)
    {
      uint16_t rotatedX = x;
      uint16_t rotatedY = y;
      switch (m_rotation)
      {
        case Rotation::Rotate90:
          rotatedX = static_cast<uint16_t>(height - 1 - y);
          rotatedY = x;
          break;
        case Rotation::Rotate180:
          rotatedX = static_cast<uint16_t>(width - 1 - x);
          rotatedY = static_cast<uint16_t>(height - 1 - y);
          break;
        case Rotation::Rotate270:
          rotatedX = y;
          rotatedY = static_cast<uint16_t>(width - 1 - x);
          break;
        default:
          break;
      }

      const size_t srcIndex = (static_cast<size_t>(y) * width + x) * 3u;
      const size_t dstIndex = (static_cast<size_t>(rotatedY) * rotatedWidth + rotatedX) * 3u;
      m_rotatedBuffer[dstIndex] = pData[srcIndex];
      m_rotatedBuffer[dstIndex + 1] = pData[srcIndex + 1];
      m_rotatedBuffer[dstIndex + 2] = pData[srcIndex + 2];
    }
  }

  *ppRenderData = m_rotatedBuffer.data();
  *pRenderWidth = rotatedWidth;
  *pRenderHeight = rotatedHeight;
}

void SDLDMD::Update(uint8_t* pData, uint16_t width, uint16_t height)
{
  std::lock_guard<std::mutex> lock(m_frameMutex);
  RGB24DMD::Update(pData, width, height);
  if (!m_update) return;
  m_update = false;

  m_pendingFrame.assign(m_pData, m_pData + m_length);
  m_framePending = true;
}

bool SDLDMD::Render()
{
  if (!m_pRenderer) return false;

  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_framePending) return false;
    m_framePending = false;
    m_renderFrame.swap(m_pendingFrame);
  }

  uint8_t* pRenderData = m_renderFrame.data();
  uint16_t renderWidth = m_width;
  uint16_t renderHeight = m_height;
  RotateFrameIfNeeded(m_renderFrame.data(), m_width, m_height, &pRenderData, &renderWidth, &renderHeight);

  const SDL_FRect target = GetTargetRect(renderWidth, renderHeight);

  // Everything outside the DMD is black, every frame: KMS/DRM flips between
  // buffers, and a margin left uncleared shows whatever the other one held.
  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);

  switch (m_renderingMode)
  {
    case RenderingMode::Square:
      RenderSquares(pRenderData, renderWidth, renderHeight, 1, target);
      break;

    case RenderingMode::Scale2x:
      RenderScaled(pRenderData, renderWidth, renderHeight, 2, SDL_SCALEMODE_NEAREST, target);
      break;

    case RenderingMode::Scale4x:
      RenderScaled(pRenderData, renderWidth, renderHeight, 4, SDL_SCALEMODE_NEAREST, target);
      break;

    case RenderingMode::Scale2xDots:
      RenderDots(pRenderData, renderWidth, renderHeight, 2, target);
      break;

    case RenderingMode::Scale4xDots:
      RenderDots(pRenderData, renderWidth, renderHeight, 4, target);
      break;

    case RenderingMode::Scale2xSquares:
      RenderSquares(pRenderData, renderWidth, renderHeight, 2, target);
      break;

    case RenderingMode::Scale4xSquares:
      RenderSquares(pRenderData, renderWidth, renderHeight, 4, target);
      break;

    case RenderingMode::SmoothScaling:
      RenderScaled(pRenderData, renderWidth, renderHeight, 4, SDL_SCALEMODE_LINEAR, target);
      break;

    case RenderingMode::XBRZ:
      RenderXBRZ(pRenderData, renderWidth, renderHeight, target);
      break;

    default:
      RenderDots(pRenderData, renderWidth, renderHeight, 1, target);
      break;
  }

  SDL_RenderPresent(m_pRenderer);
  return true;
}

// The rectangle of the output the DMD is drawn into, in output pixels.
//
// First the area: the whole output when the window is the size that was asked
// for, otherwise the requested rectangle of the screen. Then the DMD itself,
// as large as fits in that area without changing its aspect ratio, centred in
// it. Stretching it to the area is never right: a 128x32 DMD on a 16:9 screen
// came out three times too tall.
SDL_FRect SDLDMD::GetTargetRect(uint16_t width, uint16_t height) const
{
  int outputWidth = 0;
  int outputHeight = 0;
  SDL_GetCurrentRenderOutputSize(m_pRenderer, &outputWidth, &outputHeight);

  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(m_pWindow, &windowWidth, &windowHeight);

  SDL_FRect area = {0.0f, 0.0f, static_cast<float>(outputWidth), static_cast<float>(outputHeight)};

  const bool windowIsAsRequested = windowWidth == m_requestedWidth && windowHeight == m_requestedHeight;
  if (!windowIsAsRequested && windowWidth > 0 && windowHeight > 0 && m_requestedWidth > 0 && m_requestedHeight > 0)
  {
    // Requested in window coordinates; the output may have more pixels than
    // that on a high-density display.
    const float scaleX = static_cast<float>(outputWidth) / windowWidth;
    const float scaleY = static_cast<float>(outputHeight) / windowHeight;
    const float requestedWidth = m_requestedWidth * scaleX;
    const float requestedHeight = m_requestedHeight * scaleY;

    // No position means centred, as a window without one would be.
    const bool centreX = SDL_WINDOWPOS_ISUNDEFINED(m_requestedX) || SDL_WINDOWPOS_ISCENTERED(m_requestedX);
    const bool centreY = SDL_WINDOWPOS_ISUNDEFINED(m_requestedY) || SDL_WINDOWPOS_ISCENTERED(m_requestedY);
    const float requestedX = centreX ? (outputWidth - requestedWidth) / 2.0f : m_requestedX * scaleX;
    const float requestedY = centreY ? (outputHeight - requestedHeight) / 2.0f : m_requestedY * scaleY;

    // Clipped to the screen. A rectangle entirely off it is a configuration
    // mistake, and filling the screen shows that something is wrong far
    // better than drawing nothing would.
    const float left = std::max(0.0f, requestedX);
    const float top = std::max(0.0f, requestedY);
    const float right = std::min(static_cast<float>(outputWidth), requestedX + requestedWidth);
    const float bottom = std::min(static_cast<float>(outputHeight), requestedY + requestedHeight);
    if (right > left && bottom > top)
    {
      area = {left, top, right - left, bottom - top};
    }
  }

  if (width == 0 || height == 0 || area.w <= 0.0f || area.h <= 0.0f)
  {
    return area;
  }

  const float scale = std::min(area.w / width, area.h / height);
  const float targetWidth = width * scale;
  const float targetHeight = height * scale;
  return {area.x + (area.w - targetWidth) / 2.0f, area.y + (area.h - targetHeight) / 2.0f, targetWidth, targetHeight};
}

// The frame as a texture at its own resolution, created once per size.
//
// RGBX rather than the RGB24 the frames come in: OpenGL ES has no 24-bit
// texture format, so SDL backed an RGB24 texture with a 32-bit one and
// converted every upload through a scratch buffer of its own. Converting
// straight into the locked texture is the same work done once.
bool SDLDMD::EnsureFrameTexture(uint16_t width, uint16_t height)
{
  if (m_pFrameTexture && m_frameTextureWidth == width && m_frameTextureHeight == height) return true;
  if (m_pFrameTexture) SDL_DestroyTexture(m_pFrameTexture);
  m_pFrameTexture = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGBX32, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!m_pFrameTexture)
  {
    m_frameTextureWidth = 0;
    m_frameTextureHeight = 0;
    return false;
  }
  m_frameTextureWidth = width;
  m_frameTextureHeight = height;
  return true;
}

bool SDLDMD::UploadFrame(const uint8_t* pData, uint16_t width, uint16_t height)
{
  if (width == 0 || height == 0 || !EnsureFrameTexture(width, height)) return false;
  void* pixels = nullptr;
  int pitch = 0;
  if (!SDL_LockTexture(m_pFrameTexture, nullptr, &pixels, &pitch)) return false;
  for (uint16_t y = 0; y < height; ++y)
  {
    const uint8_t* src = pData + static_cast<size_t>(y) * width * 3u;
    uint8_t* dst = static_cast<uint8_t*>(pixels) + static_cast<size_t>(y) * pitch;
    for (uint16_t x = 0; x < width; ++x)
    {
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
      dst[3] = 255;
      src += 3;
      dst += 4;
    }
  }
  SDL_UnlockTexture(m_pFrameTexture);
  return true;
}

namespace
{
// Where cell `index` of `cells` starts in a span of `pixels` pixels: rounded
// to the nearest whole pixel, in integers so that the colours and the gaps of
// the square renderer are cut at exactly the same places.
int CellEdge(int index, int pixels, int cells)
{
  return static_cast<int>((2ll * index * pixels + cells) / (2ll * cells));
}
}  // namespace

// Black everywhere but the lit part of each cell, built once per size and
// shape, `gridWidth` x `gridHeight` cells over `maskWidth` x `maskHeight`
// pixels.
//
// A dot is the circle the per-pixel drawing used: radius cell / 2, rounded
// down, centred on the cell's middle pixel, in a mask of whole cells that is
// then stretched to the target.
//
// A square is lit but for a one pixel black line on the right and at the
// bottom of each cell, as the per-pixel rectangles were. That mask is built at
// the target's own size, drawn one to one, with its gaps on the cell edges of
// CellEdge() -- the edges RenderSquares() draws the colours to. A cell under
// two pixels wide has no gap across, as a rectangle that narrow had none.
bool SDLDMD::EnsureMask(uint16_t gridWidth, uint16_t gridHeight, int maskWidth, int maskHeight, MaskShape shape)
{
  if (m_pMask && m_maskGridWidth == gridWidth && m_maskGridHeight == gridHeight && m_maskWidth == maskWidth &&
      m_maskHeight == maskHeight && m_maskShape == shape)
  {
    return true;
  }
  if (m_pMask) SDL_DestroyTexture(m_pMask);
  m_pMask = nullptr;
  if (gridWidth == 0 || gridHeight == 0 || maskWidth <= 0 || maskHeight <= 0) return false;

  std::vector<uint8_t> pixels(static_cast<size_t>(maskWidth) * maskHeight * 4u, 0);
  if (shape == MaskShape::Dot)
  {
    const int cell = maskWidth / gridWidth;
    const int radius = cell / 2;
    const int centre = static_cast<int>(cell / 2.0f);
    for (int y = 0; y < maskHeight; ++y)
    {
      uint8_t* row = &pixels[static_cast<size_t>(y) * maskWidth * 4u];
      const int dy = y % cell - centre;
      for (int x = 0; x < maskWidth; ++x)
      {
        const int dx = x % cell - centre;
        row[x * 4 + 3] = dx * dx + dy * dy <= radius * radius ? 0 : 255;
      }
    }
  }
  else
  {
    // For each column and each row: 255 on the last pixel of a cell.
    auto gaps = [](int span, int cells)
    {
      std::vector<uint8_t> gap(static_cast<size_t>(span), 0);
      for (int c = 0; c < cells; ++c)
      {
        const int start = CellEdge(c, span, cells);
        const int end = CellEdge(c + 1, span, cells);
        if (end - start >= 2) gap[static_cast<size_t>(end - 1)] = 255;
      }
      return gap;
    };
    const std::vector<uint8_t> columns = gaps(maskWidth, gridWidth);
    const std::vector<uint8_t> rows = gaps(maskHeight, gridHeight);
    for (int y = 0; y < maskHeight; ++y)
    {
      uint8_t* row = &pixels[static_cast<size_t>(y) * maskWidth * 4u];
      for (int x = 0; x < maskWidth; ++x)
      {
        row[x * 4 + 3] = std::max(columns[static_cast<size_t>(x)], rows[static_cast<size_t>(y)]);
      }
    }
  }

  m_pMask = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, maskWidth, maskHeight);
  if (!m_pMask) return false;
  SDL_UpdateTexture(m_pMask, nullptr, pixels.data(), maskWidth * 4);
  SDL_SetTextureBlendMode(m_pMask, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(m_pMask, SDL_SCALEMODE_NEAREST);
  m_maskGridWidth = gridWidth;
  m_maskGridHeight = gridHeight;
  m_maskWidth = maskWidth;
  m_maskHeight = maskHeight;
  m_maskShape = shape;
  return true;
}

// Each pixel of the frame as round dots, `grid` of them to a pixel across and
// down (scale2x-dots is a grid of 2).
//
// Two draws: the frame scaled up to the target without smoothing, and over it
// the mask. This used to draw point by point (up to 600000 SDL_RenderPoint
// calls a frame), and for the scaled variants first blew the frame up on the
// CPU into a freshly allocated buffer. The DMD is drawn from ppuc's main loop,
// so all of that was time taken from everything else it does.
void SDLDMD::RenderDots(const uint8_t* pData, uint16_t width, uint16_t height, int grid, const SDL_FRect& target)
{
  if (width == 0 || height == 0 || grid < 1) return;
  const uint16_t gridWidth = static_cast<uint16_t>(width * grid);
  const uint16_t gridHeight = static_cast<uint16_t>(height * grid);
  if (!UploadFrame(pData, width, height)) return;
  SDL_SetTextureScaleMode(m_pFrameTexture, SDL_SCALEMODE_NEAREST);
  SDL_RenderTexture(m_pRenderer, m_pFrameTexture, nullptr, &target);
  // Rounded down, as the dot radius always was, and at least two pixels:
  // below that a dot is not a dot at all.
  const int cell = std::max(2, static_cast<int>(target.w / gridWidth));
  if (!EnsureMask(gridWidth, gridHeight, gridWidth * cell, gridHeight * cell, MaskShape::Dot)) return;
  SDL_RenderTexture(m_pRenderer, m_pMask, nullptr, &target);
}

// Each pixel of the frame as squares with a one pixel gap, `grid` of them to a
// pixel across and down.
//
// The colours go out as one batch of rectangles, a rectangle per frame pixel
// with whole-pixel edges, and the gaps as a mask cut at the same edges. A frame
// texture stretched to the target cannot do it: wherever a cell is not a whole
// number of pixels, which cell the pixel on an edge gets is the renderer's
// rounding, GPUs differ in it, and a gap one pixel off the colour edge shows
// a sliver of the neighbouring square. This used to be one SDL_RenderFillRect
// per lit square, and for the scaled variants a CPU blow-up of the frame
// first.
void SDLDMD::RenderSquares(const uint8_t* pData, uint16_t width, uint16_t height, int grid, const SDL_FRect& target)
{
  if (width == 0 || height == 0 || grid < 1) return;
  const int gridWidth = width * grid;
  const int gridHeight = height * grid;
  const int left = static_cast<int>(target.x + 0.5f);
  const int top = static_cast<int>(target.y + 0.5f);
  const int spanWidth = static_cast<int>(target.w + 0.5f);
  const int spanHeight = static_cast<int>(target.h + 0.5f);
  if (spanWidth <= 0 || spanHeight <= 0) return;

  m_squareVertices.clear();
  m_squareIndices.clear();
  for (int y = 0; y < height; ++y)
  {
    const float y0 = static_cast<float>(top + CellEdge(y * grid, spanHeight, gridHeight));
    const float y1 = static_cast<float>(top + CellEdge((y + 1) * grid, spanHeight, gridHeight));
    for (int x = 0; x < width; ++x)
    {
      const uint8_t* p = pData + (static_cast<size_t>(y) * width + x) * 3u;
      if (p[0] == 0 && p[1] == 0 && p[2] == 0) continue;  // the background is black already
      const float x0 = static_cast<float>(left + CellEdge(x * grid, spanWidth, gridWidth));
      const float x1 = static_cast<float>(left + CellEdge((x + 1) * grid, spanWidth, gridWidth));
      const SDL_FColor color = {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, 1.0f};
      const int base = static_cast<int>(m_squareVertices.size());
      m_squareVertices.push_back({{x0, y0}, color, {0.0f, 0.0f}});
      m_squareVertices.push_back({{x1, y0}, color, {0.0f, 0.0f}});
      m_squareVertices.push_back({{x1, y1}, color, {0.0f, 0.0f}});
      m_squareVertices.push_back({{x0, y1}, color, {0.0f, 0.0f}});
      for (int i : {0, 1, 2, 0, 2, 3}) m_squareIndices.push_back(base + i);
    }
  }
  if (!m_squareVertices.empty())
  {
    SDL_RenderGeometry(m_pRenderer, nullptr, m_squareVertices.data(), static_cast<int>(m_squareVertices.size()),
                       m_squareIndices.data(), static_cast<int>(m_squareIndices.size()));
  }

  // Too small for a gap in either direction: the colours alone are the picture.
  if (spanWidth < 2 * gridWidth && spanHeight < 2 * gridHeight) return;
  if (!EnsureMask(static_cast<uint16_t>(gridWidth), static_cast<uint16_t>(gridHeight), spanWidth, spanHeight,
                  MaskShape::Square))
  {
    return;
  }
  const SDL_FRect maskRect = {static_cast<float>(left), static_cast<float>(top), static_cast<float>(spanWidth),
                              static_cast<float>(spanHeight)};
  SDL_RenderTexture(m_pRenderer, m_pMask, nullptr, &maskRect);
}

bool SDLDMD::EnsureIntermediate(int width, int height)
{
  if (m_pIntermediate && m_intermediateWidth == width && m_intermediateHeight == height) return true;
  if (m_pIntermediate) SDL_DestroyTexture(m_pIntermediate);
  m_pIntermediate = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
  if (!m_pIntermediate)
  {
    m_intermediateWidth = 0;
    m_intermediateHeight = 0;
    return false;
  }
  m_intermediateWidth = width;
  m_intermediateHeight = height;
  return true;
}

// The frame blown up `factor` times without smoothing on the GPU, then drawn
// to the target with `finalScaleMode`: nearest for scale2x/scale4x, linear for
// smooth. Both textures live as long as the size does; they used to be
// created and destroyed for every frame.
void SDLDMD::RenderScaled(const uint8_t* pData, uint16_t width, uint16_t height, int factor,
                          SDL_ScaleMode finalScaleMode, const SDL_FRect& target)
{
  if (!UploadFrame(pData, width, height) || !EnsureIntermediate(width * factor, height * factor)) return;

  SDL_SetRenderTarget(m_pRenderer, m_pIntermediate);
  SDL_SetTextureScaleMode(m_pFrameTexture, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);
  SDL_RenderTexture(m_pRenderer, m_pFrameTexture, nullptr, nullptr);

  SDL_SetRenderTarget(m_pRenderer, nullptr);
  SDL_SetTextureScaleMode(m_pIntermediate, finalScaleMode);
  SDL_RenderTexture(m_pRenderer, m_pIntermediate, nullptr, &target);
}

// xBRZ runs on the CPU, so the most it can be spared is everything around it:
// its buffers and its texture are kept from frame to frame.
void SDLDMD::RenderXBRZ(const uint8_t* pData, uint16_t width, uint16_t height, const SDL_FRect& target)
{
  constexpr size_t kScaleFactor = 6;
  const int srcWidth = width;
  const int srcHeight = height;
  if (srcWidth == 0 || srcHeight == 0) return;
  const int scaledWidth = srcWidth * static_cast<int>(kScaleFactor);
  const int scaledHeight = srcHeight * static_cast<int>(kScaleFactor);

  m_xbrzSource.resize(static_cast<size_t>(srcWidth) * srcHeight);
  m_xbrzScaled.resize(static_cast<size_t>(scaledWidth) * scaledHeight);
  for (size_t i = 0; i < m_xbrzSource.size(); ++i)
  {
    const uint8_t* p = pData + i * 3u;
    m_xbrzSource[i] = (255u << 24) | (static_cast<uint32_t>(p[0]) << 16) | (static_cast<uint32_t>(p[1]) << 8) | p[2];
  }

  xbrz::ScalerCfg cfg;
  cfg.luminanceWeight = 1.0;
  cfg.equalColorTolerance = 30.0;
  cfg.centerDirectionBias = 4.0;
  cfg.dominantDirectionThreshold = 3.6;
  cfg.steepDirectionThreshold = 2.2;
  xbrz::scale(kScaleFactor, m_xbrzSource.data(), m_xbrzScaled.data(), srcWidth, srcHeight, xbrz::ColorFormat::ARGB, cfg,
              0, srcHeight);

  if (!m_pXbrzTexture || m_xbrzTextureWidth != scaledWidth || m_xbrzTextureHeight != scaledHeight)
  {
    if (m_pXbrzTexture) SDL_DestroyTexture(m_pXbrzTexture);
    // ARGB8888 is what xBRZ writes, and a format OpenGL ES takes as it is.
    m_pXbrzTexture = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, scaledWidth,
                                       scaledHeight);
    m_xbrzTextureWidth = m_pXbrzTexture ? scaledWidth : 0;
    m_xbrzTextureHeight = m_pXbrzTexture ? scaledHeight : 0;
    if (!m_pXbrzTexture) return;
  }
  SDL_UpdateTexture(m_pXbrzTexture, nullptr, m_xbrzScaled.data(), scaledWidth * static_cast<int>(sizeof(uint32_t)));
  SDL_RenderTexture(m_pRenderer, m_pXbrzTexture, nullptr, &target);
}

}  // namespace DMDUtil
