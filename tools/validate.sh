#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
test_dir="$(mktemp -d /tmp/xiangya-host.XXXXXX)"
trap 'rm -rf -- "$test_dir"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_garden_model.c main/garden_model.c -o "$test_dir/model"
"$test_dir/model"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_garden_companion.c main/garden_companion.c -o "$test_dir/companion"
"$test_dir/companion"
echo 'Host tests: PASS'
command -v idf.py >/dev/null || { echo 'Activate ESP-IDF 5.5.3 first' >&2; exit 1; }
mkdir -p build
SDKCONFIG_DEFAULTS="$root/sdkconfig.defaults" idf.py -B "$root/build" -D "SDKCONFIG=$root/build/sdkconfig" build
idf.py -B "$root/build" merge-bin -o "$root/build/FoloToy-AI-Passport-full.bin"
python3 tools/verify_firmware.py "$root/build"
PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create "$root/build" --archive-root "$root/build/firmware"
echo 'Firmware build: PASS'
