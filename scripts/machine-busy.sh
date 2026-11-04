#!/bin/sh
# RULES 6a-1 gate: exit 0 (prints "busy") while another build or test run
# uses the machine, exit 1 (prints "free") otherwise. fs-uae rigs do not count.
# Wait loop: while sh scripts/machine-busy.sh >/dev/null; do sleep 30; done
if LC_ALL=C ps -Ao command | grep -E "^(cmake --build|ctest|ninja|.*/clang |.*/clang\+\+ |.*vitest|.*jest)" | grep -v grep | grep -q .; then
  echo busy; exit 0
else
  echo free; exit 1
fi
