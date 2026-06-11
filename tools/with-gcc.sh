#!/usr/bin/env bash
# 在调用 platformio（以及其他需要宿主 gcc/g++ 的工具）前把 WinLibs MinGW64
# 加到 PATH 头部。给那些不读 ~/.bashrc 的非交互 shell 使用，比如某些 CI 或
# 自动化包装器。用户日常 Git Bash 终端可以忽略此脚本。
#
# 用法：
#   tools/with-gcc.sh pio test -e native
#   tools/with-gcc.sh pio run -e esp32s3
#
# 脚本在 exec 之前 export PATH，不影响调用者环境。

set -euo pipefail

GCC_BIN="/c/Users/WPP_JKW/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"

if [ ! -x "$GCC_BIN/gcc.exe" ]; then
  echo "错误：未在 $GCC_BIN 找到 gcc.exe。" >&2
  echo "请先在 WinLibs POSX UCRT 镜像安装 MinGW64：" >&2
  echo "  winget install --id BrechtSanders.WinLibs.POSIX.UCRT --scope user" >&2
  exit 1
fi

export PATH="$GCC_BIN:$PATH"
exec "$@"
