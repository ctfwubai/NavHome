#!/usr/bin/env bash
# 递归补齐 dist 目录的第三方 DLL 依赖（MSYS2 windeployqt 不拷 icu/freetype/sqlite 等第三方库）
# 用法: deploy_deps.sh [dist目录]
set -euo pipefail

MINGW="${MINGW:-/c/msys64/mingw64}"
DIST="${1:-$(dirname "$0")/dist}"
export PATH="$MINGW/bin:$PATH"

SYS_DIRS=("/c/Windows/System32" "/c/Windows/SysWOW64")

is_system() {
  local name="$1" d
  for d in "${SYS_DIRS[@]}"; do
    [ -f "$d/$name" ] && return 0
  done
  return 1
}

# 在 mingw64/bin 里按文件名（大小写不敏感）查找
find_in_mingw() {
  local name="$1" f b
  for f in "$MINGW/bin"/*; do
    b="${f##*/}"
    [ "${b,,}" = "${name,,}" ] && { echo "$f"; return 0; }
  done
  return 1
}

declare -A seen have
# 记录 dist 已存在的文件名（仅文件名，含子目录）
while IFS= read -r f; do b="${f##*/}"; have["${b,,}"]=1; done < <(find "$DIST" -type f)

queue=()
while IFS= read -r f; do queue+=("$f"); done < <(find "$DIST" -type f \( -iname '*.exe' -o -iname '*.dll' \))

copied=0
while [ "${#queue[@]}" -gt 0 ]; do
  f="${queue[0]}"; queue=("${queue[@]:1}")
  base="${f##*/}"; bl="${base,,}"
  [ -n "${seen[$bl]:-}" ] && continue
  seen[$bl]=1

  deps=$(objdump -p "$f" 2>/dev/null | sed -n 's/^\s*DLL Name: //p' | tr -d '\r')
  for d in $deps; do
    dl="${d,,}"
    [ -n "${have[$dl]:-}" ] && continue   # 已在 dist
    # 优先用 mingw64 的版本（保证 64 位 + 版本一致），
    # 不信任 System32/SysWOW64 里碰巧存在的同名 DLL（可能是 32 位或客户机没有）
    src=$(find_in_mingw "$dl" || true)
    if [ -n "$src" ]; then
      cp "$src" "$DIST/$d"
      have[$dl]=1
      copied=$((copied+1))
      echo "  + $d"
      queue+=("$DIST/$d")
      continue
    fi
    is_system "$dl" && continue            # mingw64 无、且为 Windows 系统 DLL
    echo "!! 找不到第三方库: $f -> $d" >&2
  done
done

echo "=== 完成，共补拷 $copied 个 DLL ==="
