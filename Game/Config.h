#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
public:
  Config()
  {
    reload();
  }

  // загружает настройки из файла settings.json заново
  void reload()
  {
    std::ifstream fin(project_path + "settings.json");
    fin >> config;
    fin.close();
  }

  // оператор () позволяет красиво получать значения настроек по пути, к примеру: config("Bot", "IsWhiteBot") вместо config["Bot"]["IsWhiteBot"]
  auto operator()(const string &setting_dir, const string &setting_name) const
  {
    return config[setting_dir][setting_name];
  }

private:
  json config;
};
