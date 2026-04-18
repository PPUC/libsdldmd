#include "SDLDMD/SDLDMD.h"

#include <algorithm>
#include <cctype>
#include <cstring>
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
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
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
#if defined(__linux__) && !defined(__ANDROID__)
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

bool IsFullscreenVideoDriver(const char* videoDriver)
{
#if defined(__linux__) && !defined(__ANDROID__)
  return videoDriver != nullptr && std::strcmp(videoDriver, "kmsdrm") == 0;
#else
  (void)videoDriver;
  return false;
#endif
}

SDLDMDConfig* GetInstalledSDLDMDConfig()
{
  return g_installedSDLDMDConfig;
}

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

void LogDisplaySetup(SDL_Window* pWindow, SDL_Renderer* pRenderer, int requestedScreenIndex)
{
  int numDisplays = 0;
  SDL_DisplayID* pDisplays = SDL_GetDisplays(&numDisplays);
  if (pDisplays != nullptr)
  {
    Log(DMDUtil_LogLevel_INFO, "SDLDMD displays detected: count=%d requestedScreen=%d", numDisplays, requestedScreenIndex);
    for (int i = 0; i < numDisplays; ++i)
    {
      SDL_Rect displayBounds;
      const bool hasBounds = SDL_GetDisplayBounds(pDisplays[i], &displayBounds);

      const SDL_DisplayMode* pMode = SDL_GetCurrentDisplayMode(pDisplays[i]);
      const bool hasMode = pMode != nullptr;

      Log(DMDUtil_LogLevel_INFO,
          "SDLDMD display[%d]: id=%u bounds=%dx%d+%d+%d mode=%dx%d@%.2f",
          i,
          static_cast<unsigned int>(pDisplays[i]),
          hasBounds ? displayBounds.w : -1,
          hasBounds ? displayBounds.h : -1,
          hasBounds ? displayBounds.x : -1,
          hasBounds ? displayBounds.y : -1,
          hasMode ? pMode->w : -1,
          hasMode ? pMode->h : -1,
          hasMode ? static_cast<double>(pMode->refresh_rate) : -1.0);
    }
    SDL_free(pDisplays);
  }
  else if (const char* const error = SDL_GetError(); error && *error)
  {
    Log(DMDUtil_LogLevel_INFO, "SDLDMD displays query failed: %s", error);
  }

  if (pWindow != nullptr)
  {
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSizeInPixels(pWindow, &windowWidth, &windowHeight);
    Log(DMDUtil_LogLevel_INFO, "SDLDMD window pixels: %dx%d", windowWidth, windowHeight);
  }

  if (pRenderer != nullptr)
  {
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetRenderOutputSize(pRenderer, &outputWidth, &outputHeight);
    Log(DMDUtil_LogLevel_INFO, "SDLDMD render output: %dx%d", outputWidth, outputHeight);
  }
}

bool SyncWindowAndWaitUntilShown(SDL_Window* pWindow)
{
  if (pWindow == nullptr)
  {
    return false;
  }

  const Uint64 deadline = SDL_GetTicks() + 3000;
  while (SDL_GetTicks() < deadline)
  {
    if (SDL_SyncWindow(pWindow))
    {
      return true;
    }
    SDL_Delay(10);
  }

  if (const char* const error = SDL_GetError(); error && *error)
  {
    Log(DMDUtil_LogLevel_INFO, "SDLDMD window sync timed out: %s", error);
  }
  else
  {
    Log(DMDUtil_LogLevel_INFO, "SDLDMD window sync timed out without SDL error");
  }
  return false;
}
}  // namespace

bool SDLDMD::CreateRendererWithFallbacks()
{
#if defined(__ANDROID__) || defined(TARGET_OS_IPHONE) || defined(TARGET_OS_TV)
  static const char* kRendererCandidates[] = {"opengles2", nullptr};
#else
  static const char* kRendererCandidates[] = {"opengl", "opengles2", nullptr};
#endif

  std::string lastError;
  for (const char* rendererName : kRendererCandidates)
  {
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
  }

  m_error = lastError.empty() ? "SDL couldn't create a renderer." : lastError;
  return false;
}

SDLDMD::SDLDMD(const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags, uint16_t width,
               uint16_t height, int screenIndex, int windowX, int windowY, RenderingMode renderingMode,
               const char* preferredVideoDriver)
    : RGB24DMD(width, height), m_pRenderer(nullptr), m_renderingMode(renderingMode)
{
  {
    std::lock_guard<std::mutex> lock(g_sdldmdSdlMutex);
    const char* const videoDriver = GetDefaultVideoDriver(preferredVideoDriver);
    if (videoDriver && *videoDriver && SDL_getenv("SDL_VIDEODRIVER") == nullptr)
    {
      SetEnvIfUnset("SDL_VIDEODRIVER", videoDriver);
    }

    if (IsFullscreenVideoDriver(videoDriver))
    {
      windowFlags |= SDL_WINDOW_FULLSCREEN;
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

    SDL_HideCursor();

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

  if ((windowFlags & SDL_WINDOW_FULLSCREEN) == 0 && !hasExplicitWindowPosition)
  {
    SDL_SetWindowPosition(m_pWindow, 0, 0);
  }
  if (!SyncWindowAndWaitUntilShown(m_pWindow))
  {
    Log(DMDUtil_LogLevel_INFO, "SDLDMD continuing after incomplete window sync");
  }
  LogDisplaySetup(m_pWindow, m_pRenderer, screenIndex);
}

SDLDMD::~SDLDMD()
{
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
                     SDLDMD::RenderingMode renderingMode,
                     const char* preferredVideoDriver)
{
  const char* const requestedVideoDriver = GetDefaultVideoDriver(preferredVideoDriver);
  Log(DMDUtil_LogLevel_INFO,
      "SDLDMD setup: title='%s' window=%ux%u dmd=%ux%u screen=%d x=%d y=%d renderer=%s videoDriver=%s flags=0x%x",
      title ? title : "",
      windowWidth,
      windowHeight,
      width,
      height,
      screenIndex,
      windowX,
      windowY,
      RenderingModeToString(renderingMode),
      (requestedVideoDriver && *requestedVideoDriver) ? requestedVideoDriver : "default",
      windowFlags);

  SDLDMD* const pSDLDMD =
      new SDLDMD(title, windowWidth, windowHeight, windowFlags, width, height, screenIndex, windowX, windowY, renderingMode,
                 preferredVideoDriver);
  if (!pSDLDMD->IsReady())
  {
    SDL_SetError("%s", pSDLDMD->GetError() ? pSDLDMD->GetError() : "Unknown SDL DMD error");
    delete pSDLDMD;
    return nullptr;
  }

  dmd.AddRGB24DMD(pSDLDMD);
  const char* const actualVideoDriver = SDL_GetCurrentVideoDriver();
  Log(DMDUtil_LogLevel_INFO,
      "SDLDMD setup succeeded: renderer=%s hardware=%d videoDriver=%s",
      pSDLDMD->GetRendererName() ? pSDLDMD->GetRendererName() : "",
      pSDLDMD->IsHardwareAccelerated() ? 1 : 0,
      (actualVideoDriver && *actualVideoDriver) ? actualVideoDriver : "unknown");
  return pSDLDMD;
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

  Log(DMDUtil_LogLevel_INFO,
      "Creating LCD-DMD output: %dx%d screen=%d x=%d y=%d renderer=%s",
      pConfig->GetLCDDMDWidth(),
      pConfig->GetLCDDMDHeight(),
      pConfig->GetLCDDMDScreen(),
      pConfig->GetLCDDMDX(),
      pConfig->GetLCDDMDY(),
      pConfig->GetLCDDMDRenderer());

  return CreateSDLDMD(dmd, title, static_cast<uint16_t>(pConfig->GetLCDDMDWidth()),
                      static_cast<uint16_t>(pConfig->GetLCDDMDHeight()), windowFlags, width, height,
                      pConfig->GetLCDDMDScreen(),
                      pConfig->GetLCDDMDX(), pConfig->GetLCDDMDY(), renderingMode, preferredVideoDriver);
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

void SDLDMD::Update(uint8_t* pData, uint16_t width, uint16_t height)
{
  RGB24DMD::Update(pData, width, height);
  if (!m_update) return;

  if (!m_loggedFirstFrame)
  {
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetRenderOutputSize(m_pRenderer, &outputWidth, &outputHeight);
    Log(DMDUtil_LogLevel_INFO,
        "SDLDMD received first frame: frame=%ux%u output=%dx%d renderer=%s",
        m_width,
        m_height,
        outputWidth,
        outputHeight,
        m_rendererName.empty() ? "" : m_rendererName.c_str());
    m_loggedFirstFrame = true;
  }

  switch (m_renderingMode)
  {
    case RenderingMode::Square:
      RenderSquares(m_pData, m_width, m_height);
      break;

    case RenderingMode::Scale2x:
      RenderScaledNearest(m_pData, m_width, m_height, 2);
      break;

    case RenderingMode::Scale4x:
      RenderScaledNearest(m_pData, m_width, m_height, 4);
      break;

    case RenderingMode::Scale2xDots:
      RenderScaledDots(m_pData, m_width, m_height, 2);
      break;

    case RenderingMode::Scale4xDots:
      RenderScaledDots(m_pData, m_width, m_height, 4);
      break;

    case RenderingMode::Scale2xSquares:
      RenderScaledSquares(m_pData, m_width, m_height, 2);
      break;

    case RenderingMode::Scale4xSquares:
      RenderScaledSquares(m_pData, m_width, m_height, 4);
      break;

    case RenderingMode::SmoothScaling:
      RenderSmoothScaling(m_pData, m_width, m_height);
      break;

    case RenderingMode::XBRZ:
      RenderXBRZ(m_pData, m_width, m_height);
      break;

    default:
      RenderDots(m_pData, m_width, m_height);
      break;
  }
}

void SDLDMD::RenderDots(uint8_t* pData, uint16_t width, uint16_t height)
{
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetRenderOutputSize(m_pRenderer, &windowWidth, &windowHeight);

  float pixelWidth = (float)windowWidth / width;
  float pixelRadius = pixelWidth / 2.0f;

  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);

  for (int y = 0; y < height; y++)
  {
    float centerY = y * pixelWidth + pixelWidth / 2.0f;

    for (int x = 0; x < width; x++)
    {
      int idx = (y * width + x) * 3;
      uint8_t r = pData[idx];
      uint8_t g = pData[idx + 1];
      uint8_t b = pData[idx + 2];

      if (r == 0 && g == 0 && b == 0) continue;

      float centerX = x * pixelWidth + pixelWidth / 2.0f;

      SDL_SetRenderDrawColor(m_pRenderer, r, g, b, 255);

      int radius = (int)pixelRadius;
      int centerXi = (int)centerX;
      int centerYi = (int)centerY;

      for (int dy = -radius; dy <= radius; dy++)
      {
        for (int dx = -radius; dx <= radius; dx++)
        {
          if (dx * dx + dy * dy <= radius * radius)
          {
            SDL_RenderPoint(m_pRenderer, centerXi + dx, centerYi + dy);
          }
        }
      }
    }
  }

  SDL_RenderPresent(m_pRenderer);
}

void SDLDMD::RenderScaledDots(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor)
{
  const uint16_t scaledWidth = static_cast<uint16_t>(width * scaleFactor);
  const uint16_t scaledHeight = static_cast<uint16_t>(height * scaleFactor);
  std::vector<uint8_t> scaledBuffer(static_cast<size_t>(scaledWidth) * scaledHeight * 3u);

  for (uint16_t y = 0; y < height; ++y)
  {
    for (uint16_t x = 0; x < width; ++x)
    {
      const size_t srcIndex = (static_cast<size_t>(y) * width + x) * 3u;
      for (int dy = 0; dy < scaleFactor; ++dy)
      {
        for (int dx = 0; dx < scaleFactor; ++dx)
        {
          const uint16_t dstX = static_cast<uint16_t>(x * scaleFactor + dx);
          const uint16_t dstY = static_cast<uint16_t>(y * scaleFactor + dy);
          const size_t dstIndex = (static_cast<size_t>(dstY) * scaledWidth + dstX) * 3u;
          scaledBuffer[dstIndex] = pData[srcIndex];
          scaledBuffer[dstIndex + 1] = pData[srcIndex + 1];
          scaledBuffer[dstIndex + 2] = pData[srcIndex + 2];
        }
      }
    }
  }

  RenderDots(scaledBuffer.data(), scaledWidth, scaledHeight);
}

void SDLDMD::RenderScaledSquares(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor)
{
  const uint16_t scaledWidth = static_cast<uint16_t>(width * scaleFactor);
  const uint16_t scaledHeight = static_cast<uint16_t>(height * scaleFactor);
  std::vector<uint8_t> scaledBuffer(static_cast<size_t>(scaledWidth) * scaledHeight * 3u);

  for (uint16_t y = 0; y < height; ++y)
  {
    for (uint16_t x = 0; x < width; ++x)
    {
      const size_t srcIndex = (static_cast<size_t>(y) * width + x) * 3u;
      for (int dy = 0; dy < scaleFactor; ++dy)
      {
        for (int dx = 0; dx < scaleFactor; ++dx)
        {
          const uint16_t dstX = static_cast<uint16_t>(x * scaleFactor + dx);
          const uint16_t dstY = static_cast<uint16_t>(y * scaleFactor + dy);
          const size_t dstIndex = (static_cast<size_t>(dstY) * scaledWidth + dstX) * 3u;
          scaledBuffer[dstIndex] = pData[srcIndex];
          scaledBuffer[dstIndex + 1] = pData[srcIndex + 1];
          scaledBuffer[dstIndex + 2] = pData[srcIndex + 2];
        }
      }
    }
  }

  RenderSquares(scaledBuffer.data(), scaledWidth, scaledHeight);
}

void SDLDMD::RenderSquares(uint8_t* pData, uint16_t width, uint16_t height)
{
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetRenderOutputSize(m_pRenderer, &windowWidth, &windowHeight);

  const float pixelWidth = static_cast<float>(windowWidth) / width;
  const float pixelHeight = static_cast<float>(windowHeight) / height;

  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      const int idx = (y * width + x) * 3;
      const uint8_t r = pData[idx];
      const uint8_t g = pData[idx + 1];
      const uint8_t b = pData[idx + 2];

      if (r == 0 && g == 0 && b == 0) continue;

      SDL_SetRenderDrawColor(m_pRenderer, r, g, b, 255);

      SDL_FRect rect = {
          x * pixelWidth,
          y * pixelHeight,
          pixelWidth,
          pixelHeight,
      };

      // Keep a 1px black border between adjacent pixels.
      if (rect.w > 1.0f)
      {
        rect.w -= 1.0f;
      }
      if (rect.h > 1.0f)
      {
        rect.h -= 1.0f;
      }

      SDL_RenderFillRect(m_pRenderer, &rect);
    }
  }

  SDL_RenderPresent(m_pRenderer);
}

void SDLDMD::RenderScaledNearest(uint8_t* pData, uint16_t width, uint16_t height, int scaleFactor)
{
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetRenderOutputSize(m_pRenderer, &windowWidth, &windowHeight);

  SDL_Texture* srcTexture =
      SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, width, height);
  SDL_UpdateTexture(srcTexture, nullptr, pData, width * 3);

  const int intermediateW = width * scaleFactor;
  const int intermediateH = height * scaleFactor;
  SDL_Texture* intermediate =
      SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, intermediateW, intermediateH);

  SDL_SetRenderTarget(m_pRenderer, intermediate);
  SDL_SetTextureScaleMode(srcTexture, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);
  SDL_RenderTexture(m_pRenderer, srcTexture, nullptr, nullptr);

  SDL_SetRenderTarget(m_pRenderer, nullptr);
  SDL_SetTextureScaleMode(intermediate, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderDrawColor(m_pRenderer, 0, 0, 0, 255);
  SDL_RenderClear(m_pRenderer);
  SDL_FRect dest = {0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
  SDL_RenderTexture(m_pRenderer, intermediate, nullptr, &dest);
  SDL_RenderPresent(m_pRenderer);

  SDL_DestroyTexture(intermediate);
  SDL_DestroyTexture(srcTexture);
}

void SDLDMD::RenderSmoothScaling(uint8_t* pData, uint16_t width, uint16_t height)
{
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetRenderOutputSize(m_pRenderer, &windowWidth, &windowHeight);

  SDL_Texture* srcTexture =
      SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, width, height);
  SDL_UpdateTexture(srcTexture, nullptr, pData, width * 3);

  int intermediateW = width * 4;
  int intermediateH = height * 4;
  SDL_Texture* intermediate =
      SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, intermediateW, intermediateH);

  SDL_SetRenderTarget(m_pRenderer, intermediate);
  SDL_SetTextureScaleMode(srcTexture, SDL_SCALEMODE_NEAREST);
  SDL_RenderTexture(m_pRenderer, srcTexture, nullptr, nullptr);

  SDL_SetRenderTarget(m_pRenderer, nullptr);
  SDL_SetTextureScaleMode(intermediate, SDL_SCALEMODE_LINEAR);
  SDL_FRect dest = {0.0f, 0.0f, (float)windowWidth, (float)windowHeight};
  SDL_RenderTexture(m_pRenderer, intermediate, nullptr, &dest);

  SDL_RenderPresent(m_pRenderer);

  SDL_DestroyTexture(srcTexture);
  SDL_DestroyTexture(intermediate);
}

void SDLDMD::RenderXBRZ(uint8_t* pData, uint16_t width, uint16_t height)
{
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetRenderOutputSize(m_pRenderer, &windowWidth, &windowHeight);

  const int srcWidth = width;
  const int srcHeight = height;
  std::vector<uint32_t> srcImage(srcWidth * srcHeight);

  for (int y = 0; y < srcHeight; ++y)
  {
    for (int x = 0; x < srcWidth; ++x)
    {
      int i = y * srcWidth + x;
      int j = i * 3;
      uint8_t r = pData[j];
      uint8_t g = pData[j + 1];
      uint8_t b = pData[j + 2];
      srcImage[i] = (255u << 24) | (r << 16) | (g << 8) | b;
    }
  }

  xbrz::ScalerCfg cfg;
  cfg.luminanceWeight = 1.0;
  cfg.equalColorTolerance = 30.0;
  cfg.centerDirectionBias = 4.0;
  cfg.dominantDirectionThreshold = 3.6;
  cfg.steepDirectionThreshold = 2.2;

  const size_t scaleFactor = 6;
  const int scaledWidth = srcWidth * scaleFactor;
  const int scaledHeight = srcHeight * scaleFactor;
  std::vector<uint32_t> dstImage(scaledWidth * scaledHeight);

  xbrz::scale(scaleFactor, srcImage.data(), dstImage.data(), srcWidth, srcHeight, xbrz::ColorFormat::ARGB, cfg, 0,
              srcHeight);

  SDL_Texture* texture =
      SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, scaledWidth, scaledHeight);

  SDL_UpdateTexture(texture, nullptr, dstImage.data(), scaledWidth * sizeof(uint32_t));

  SDL_RenderClear(m_pRenderer);

  SDL_FRect dstRect = {0.0f, 0.0f, (float)windowWidth, (float)windowHeight};
  SDL_RenderTexture(m_pRenderer, texture, nullptr, &dstRect);

  SDL_RenderPresent(m_pRenderer);

  SDL_DestroyTexture(texture);
}

}  // namespace DMDUtil
