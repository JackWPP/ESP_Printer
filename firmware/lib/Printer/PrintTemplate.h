#pragma once
#include <Arduino.h>

namespace timeprint {

struct PrintTemplate {
  const char* id;
  const char* name;
  const char* message;
};

static const PrintTemplate kTemplates[] PROGMEM = {
  {"default",   "默认",   "专注的每一刻都值得记录 :)"},
  {"warm",      "温暖",   "慢慢来，比较快 :)"},
  {"cheer",     "加油",   "你比你想象的更强大 :)"},
  {"zen",       "禅意",   "此刻即永恒 :)"},
  {"growth",    "成长",   "你正在成为更好的自己 :)"},
  {"time",      "时光",   "时间会记住你的努力 :)"},
  {"today",     "今天",   "今天也认真地度过了一天 :)"},
  {"reward",    "奖励",   "专注本身就是奖励 :)"},
  {"small",     "微光",   "每一点积累都不白费 :)"},
  {"rest",      "休息",   "辛苦了，去喝杯水吧 :)"},
  {"seed",      "播种",   "种一棵树最好的时间是现在 :)"},
  {"wave",      "浪潮",   "日拱一卒，功不唐捐 :)"},
  {"spark",     "星火",   "你安静努力的样子很好看 :)"},
  {"mountain",  "远山",   "不必着急，山就在那里 :)"},
  {"echo",      "回响",   "认真的人会被世界温柔以待 :)"},
  {"drift",     "漂流",   "允许自己慢一点也没关系 :)"},
  {"light",     "微风",   "能坐下来专注，已经很厉害了 :)"},
  {"book",      "书页",   "又翻过了一页，继续吧 :)"},
  {"change",    "蜕变",   "所有了不起的改变，都从安静的坚持开始 :)"},
  {"second",    "一秒",   "你付出的每一秒，都在悄悄塑造未来的你 :)"},
  {"focus",     "此刻",   "世界很大，但此刻只有你和这件事 :)"},
  {"start",     "开始",   "比起完美，开始本身更重要 :)"},
  {"yesterday", "昨天",   "今天的你，比昨天多了一份沉淀 :)"},
  {"path",      "长路",   "走过的路都算数，未来的你会感谢现在 :)"},
  {"breathe",   "呼吸",   "累了就深呼吸，然后继续往前走 :)"},
  {"star",      "繁星",   "你不需要和别人比，做自己的星星就好 :)"},
  {"courage",   "勇气",   "能坚持到现在，你已经很有勇气了 :)"},
  {"pace",      "节奏",   "找到自己的节奏，比追赶别人更重要 :)"},
  {"craft",     "匠心",   "把手头的事做到极致，就是最好的修行 :)"},
  {"quiet",     "安静",   "安静的力量，往往被低估了 :)"},
  {"dawn",      "破晓",   "黎明前最暗，但光一定会来 :)"},
  {"canvas",    "画布",   "每一天都是新的画布，由你来涂色 :)"},
  {"roots",     "扎根",   "向下扎根够深，才能向上生长够高 :)"},
  {"compass",   "方向",   "不必走得多快，方向对了就不怕远 :)"},
  {"gift",      "礼物",   "此刻的专注，是你送给未来的礼物 :)"},
};

static const int kTemplateCount = sizeof(kTemplates) / sizeof(kTemplates[0]);

inline const PrintTemplate* findTemplate(const char* id) {
  for (int i = 0; i < kTemplateCount; ++i) {
    if (strcmp(kTemplates[i].id, id) == 0) return &kTemplates[i];
  }
  return &kTemplates[0];
}

}  // namespace timeprint
