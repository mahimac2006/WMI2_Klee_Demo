#!/usr/bin/env bash
set -euxo pipefail

# Run KLEE on WMI-2 (leak) demo. Build wmi2_demo.bc first with ./build_wmi2.sh

klee --search=bfs --max-time=60s --exit-on-error-type=Assert wmi2_demo.bc
echo "[OK] KLEE finished; see klee-out-* for errors/tests"
