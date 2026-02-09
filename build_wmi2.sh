#!/usr/bin/env bash
set -euxo pipefail

: "${KLEE_INC:=/usr/local/include}"

echo "[INFO] pwd=$(pwd)"
echo "[INFO] KLEE_INC=$KLEE_INC"

test -f driver_wmi2_leak.c
test -f stubs_wmi2.c
test -f metalogin.c
test -f metalogin.h

KLEE_ARGS=()
if [ -f "$KLEE_INC/klee/klee.h" ]; then
  KLEE_ARGS=(-I"$KLEE_INC" -D__KLEE__)
else
  echo "[WARN] klee.h not found at $KLEE_INC/klee/klee.h; building without __KLEE__"
  KLEE_ARGS=()
fi

clang "${KLEE_ARGS[@]}" -I. -O0 -g -emit-llvm -c driver_wmi2_leak.c -o driver_wmi2_leak.bc
clang "${KLEE_ARGS[@]}" -I. -O0 -g -emit-llvm -c stubs_wmi2.c -o stubs_wmi2.bc

clang "${KLEE_ARGS[@]}" -DKLEE_DRIVER_BUILD -I. -O0 -g -emit-llvm -c metalogin.c -o metalogin.bc

EXTRA_BC=()
if [ -f globals.c ]; then
  clang "${KLEE_ARGS[@]}" -DKLEE_DRIVER_BUILD -I. -O0 -g -emit-llvm -c globals.c -o globals.bc
  EXTRA_BC+=(globals.bc)
fi

llvm-link driver_wmi2_leak.bc stubs_wmi2.bc metalogin.bc "${EXTRA_BC[@]}" -o wmi2_demo.bc

ls -la *.bc
echo "[OK] Built wmi2_demo.bc"
