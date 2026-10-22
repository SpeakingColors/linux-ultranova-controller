#!/bin/sh
# Runs the pure C++ core tests against this repo's map, with address and undefined behaviour checks when the compiler has them.
cd "$(dirname "$0")/.." || exit 1
c++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o /tmp/core_test tests/test_core.cpp 2>/dev/null \
  || c++ -std=c++17 -Wall -Wextra -o /tmp/core_test tests/test_core.cpp || exit 1
/tmp/core_test map/params.csv
