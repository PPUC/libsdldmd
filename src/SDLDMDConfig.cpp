#include "SDLDMD/SDLDMDConfig.h"

#include <exception>

#include "ini.h"

namespace DMDUtil
{

SDLDMDConfig::SDLDMDConfig() = default;

void SDLDMDConfig::parseConfigFile(const char* path)
{
  Config::parseConfigFile(path);

  m_lcddmdEnabled = false;
  m_lcddmdX = 0;
  m_lcddmdY = 0;
  m_lcddmdWidth = 1024;
  m_lcddmdHeight = 256;
  m_lcddmdScreen = -1;
  m_lcddmdRenderer = "dots";

  try
  {
    inih::INIReader r{path};

    try
    {
      m_lcddmdEnabled = r.Get<bool>("LCD-DMD", "Enabled", false);
    }
    catch (const std::exception&)
    {
      m_lcddmdEnabled = false;
    }

    try
    {
      m_lcddmdX = r.Get<int>("LCD-DMD", "X", 0);
    }
    catch (const std::exception&)
    {
      m_lcddmdX = 0;
    }

    try
    {
      m_lcddmdY = r.Get<int>("LCD-DMD", "Y", 0);
    }
    catch (const std::exception&)
    {
      m_lcddmdY = 0;
    }

    try
    {
      m_lcddmdWidth = r.Get<int>("LCD-DMD", "Width", 1024);
    }
    catch (const std::exception&)
    {
      m_lcddmdWidth = 1024;
    }

    try
    {
      m_lcddmdHeight = r.Get<int>("LCD-DMD", "Height", 256);
    }
    catch (const std::exception&)
    {
      m_lcddmdHeight = 256;
    }

    try
    {
      m_lcddmdScreen = r.Get<int>("LCD-DMD", "Screen", -1);
    }
    catch (const std::exception&)
    {
      m_lcddmdScreen = -1;
    }

    try
    {
      m_lcddmdRenderer = r.Get<std::string>("LCD-DMD", "Renderer", "dots");
    }
    catch (const std::exception&)
    {
      m_lcddmdRenderer = "dots";
    }
  }
  catch (const std::exception&)
  {
  }
}

}  // namespace DMDUtil
