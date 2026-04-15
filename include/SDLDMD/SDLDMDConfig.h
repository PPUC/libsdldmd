#pragma once

#ifdef _MSC_VER
#define SDLDMDAPI __declspec(dllexport)
#else
#define SDLDMDAPI __attribute__((visibility("default")))
#endif

#include <string>

#include "DMDUtil/Config.h"

namespace DMDUtil
{

class SDLDMDAPI SDLDMDConfig : public Config
{
 public:
  SDLDMDConfig();

  void parseConfigFile(const char* path) override;

  bool IsLCDDMDEnabled() const { return m_lcddmdEnabled; }
  int GetLCDDMDX() const { return m_lcddmdX; }
  int GetLCDDMDY() const { return m_lcddmdY; }
  int GetLCDDMDWidth() const { return m_lcddmdWidth; }
  int GetLCDDMDHeight() const { return m_lcddmdHeight; }
  int GetLCDDMDScreen() const { return m_lcddmdScreen; }
  const char* GetLCDDMDRenderer() const { return m_lcddmdRenderer.c_str(); }

 private:
  bool m_lcddmdEnabled = false;
  int m_lcddmdX = 0;
  int m_lcddmdY = 0;
  int m_lcddmdWidth = 1024;
  int m_lcddmdHeight = 256;
  int m_lcddmdScreen = -1;
  std::string m_lcddmdRenderer = "dots";
};

}  // namespace DMDUtil
