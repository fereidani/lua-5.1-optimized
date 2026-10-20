#!/usr/bin/env bash
set -euo pipefail

find src etc -type f \( -name '*.c' -o -name '*.h' \) -exec clang-format -style=Google -i {} +
