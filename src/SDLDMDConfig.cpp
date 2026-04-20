#include "SDLDMD/SDLDMDConfig.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace DMDUtil
{

namespace
{
std::string Trim(const std::string& value)
{
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
  {
    ++start;
  }

  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
  {
    --end;
  }

  return value.substr(start, end - start);
}

bool ParseIniBool(const std::string& value, bool defaultValue)
{
  std::string normalized = Trim(value);
  for (char& c : normalized)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") return true;
  if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") return false;
  return defaultValue;
}
}  // namespace

SDLDMDConfig::SDLDMDConfig() = default;

void SDLDMDConfig::parseConfigFile(const char* path)
{
  Config::parseConfigFile(path);

  m_lcddmdEnabled = false;
  m_lcddmdHD = false;
  m_lcddmdX = 0;
  m_lcddmdY = 0;
  m_lcddmdWidth = 1024;
  m_lcddmdHeight = 256;
  m_lcddmdScreen = -1;
  m_lcddmdRotation = 0;
  m_lcddmdRenderer = "dots";

  if (path == nullptr)
  {
    return;
  }

  std::ifstream file(path);
  if (!file.good())
  {
    return;
  }

  std::string line;
  std::string section;
  while (std::getline(file, line))
  {
    const size_t commentPos = line.find('#');
    if (commentPos != std::string::npos)
    {
      line = line.substr(0, commentPos);
    }

    line = Trim(line);
    if (line.empty()) continue;

    if (line.front() == '[' && line.back() == ']')
    {
      section = Trim(line.substr(1, line.size() - 2));
      continue;
    }

    if (section != "LCD-DMD") continue;

    const size_t separatorPos = line.find('=');
    if (separatorPos == std::string::npos) continue;

    const std::string key = Trim(line.substr(0, separatorPos));
    const std::string value = Trim(line.substr(separatorPos + 1));

    if (key == "Enabled")
    {
      m_lcddmdEnabled = ParseIniBool(value, m_lcddmdEnabled);
    }
    else if (key == "HD")
    {
      m_lcddmdHD = ParseIniBool(value, m_lcddmdHD);
    }
    else if (key == "Screen")
    {
      m_lcddmdScreen = std::atoi(value.c_str());
    }
    else if (key == "X")
    {
      m_lcddmdX = std::atoi(value.c_str());
    }
    else if (key == "Y")
    {
      m_lcddmdY = std::atoi(value.c_str());
    }
    else if (key == "Width")
    {
      m_lcddmdWidth = std::atoi(value.c_str());
    }
    else if (key == "Height")
    {
      m_lcddmdHeight = std::atoi(value.c_str());
    }
    else if (key == "Renderer")
    {
      m_lcddmdRenderer = value;
    }
    else if (key == "Rotation")
    {
      m_lcddmdRotation = std::atoi(value.c_str());
    }
  }
}

}  // namespace DMDUtil
