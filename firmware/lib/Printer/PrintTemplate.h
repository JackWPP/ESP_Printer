#pragma once
#include <Arduino.h>

namespace timeprint {

struct PrintTemplate {
  const char* id;
  const char* name;
  const char* message;
};

static const PrintTemplate kTemplates[] PROGMEM = {
  {"default", "默认",   "专注的每一刻都值得记录 :)"},
  {"warm",    "温暖",   "慢慢来，比较快 :)"},
  {"cheer",   "加油",   "你比你想象的更强大 :)"},
  {"zen",     "禅意",   "此刻即永恒 :)"},
  {"simple",  "简洁",   ":)"},
};

static const int kTemplateCount = sizeof(kTemplates) / sizeof(kTemplates[0]);

inline const PrintTemplate* findTemplate(const char* id) {
  for (int i = 0; i < kTemplateCount; ++i) {
    if (strcmp(kTemplates[i].id, id) == 0) return &kTemplates[i];
  }
  return &kTemplates[0];
}

}  // namespace timeprint
