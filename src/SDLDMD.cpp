#include "SDLDMD/SDLDMD.h"

#include <vector>

#include "xbrz/xbrz.h"

namespace DMDUtil
{
SDLDMD::SDLDMD(const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags, uint16_t width,
               uint16_t height)
    : RGB24DMD(width, height), m_pRenderer(nullptr)
{
  if (!SDL_CreateWindowAndRenderer(title, windowWidth, windowHeight, windowFlags, &m_pWindow, &m_pRenderer))
  {
    m_error = SDL_GetError();
    m_pWindow = nullptr;
    m_pRenderer = nullptr;
    return;
  }

  SDL_SetWindowPosition(m_pWindow, 0, 0);
  while (!SDL_SyncWindow(m_pWindow));
}

SDLDMD::~SDLDMD()
{
  if (m_pRenderer) SDL_DestroyRenderer(m_pRenderer);
  if (m_pWindow) SDL_DestroyWindow(m_pWindow);
  m_pRenderer = nullptr;
  m_pWindow = nullptr;
}

SDLDMD* CreateSDLDMD(DMD& dmd, const char* title, uint16_t windowWidth, uint16_t windowHeight, uint32_t windowFlags,
                     uint16_t width, uint16_t height)
{
  SDLDMD* const pSDLDMD = new SDLDMD(title, windowWidth, windowHeight, windowFlags, width, height);
  if (!pSDLDMD->IsReady())
  {
    SDL_SetError("%s", pSDLDMD->GetError() ? pSDLDMD->GetError() : "Unknown SDL DMD error");
    delete pSDLDMD;
    return nullptr;
  }

  dmd.AddRGB24DMD(pSDLDMD);
  return pSDLDMD;
}

bool DestroySDLDMD(DMD& dmd, SDLDMD* pSDLDMD) { return dmd.DestroyRGB24DMD(pSDLDMD); }

void SDLDMD::Update(uint8_t* pData, uint16_t width, uint16_t height)
{
  RGB24DMD::Update(pData, width, height);
  if (!m_update) return;

  switch (m_renderingMode)
  {
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
