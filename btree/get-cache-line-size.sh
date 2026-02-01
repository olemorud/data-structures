#!/bin/sh
# best-effort cache line size in bytes

set -u

is_uint() {
  case "$1" in
    ''|*[!0-9]*) return 1 ;;
    *) return 0 ;;
  esac
}

emit_if_ok() {
  v=$1
  if is_uint "$v" && [ "$v" -gt 0 ] 2>/dev/null; then
    echo "$v"
    exit 0
  fi
}

# BSD/macOS: sysctl
if command -v sysctl >/dev/null 2>&1; then
  emit_if_ok "$(sysctl -n hw.cachelinesize 2>/dev/null)"
fi

# getconf keys (glibc/linux)
if command -v getconf >/dev/null 2>&1; then
  for k in LEVEL1_DCACHE_LINESIZE LEVEL2_CACHE_LINESIZE LEVEL3_CACHE_LINESIZE LEVEL4_CACHE_LINESIZE; do
    v="$(getconf "$k" 2>/dev/null)"
    emit_if_ok "$v"
  done
fi

# linux sysfs
for f in \
  /sys/devices/system/cpu/cpu0/cache/index*/coherency_line_size \
  /sys/devices/system/cpu/cpu*/cache/index*/coherency_line_size
do
  [ -r "$f" ] || continue
  emit_if_ok "$(cat "$f" 2>/dev/null)"
done

# last resort fallback
echo 64
exit 0

