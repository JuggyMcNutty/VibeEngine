#!/usr/bin/env bash
# Tools for engine work on an x86_64 Linux host, unpacked into the deps/ that
# Port Ex Machina's repositories share -- nothing is installed on the machine.
# Pinned Arch Linux packages, like the sysroot's headers; vibe/docs/ENGINE.md
# says how they are used. The repositories' parent folder is $DX_ROOT, or the
# one this clone is in; the workspace is its port-ex-machina/.
#
#   deps/perf           Linux perf and the four libraries it needs
#   deps/vulkan-layers  the Khronos validation layer, with a manifest in
#                       layers/ that points at it by absolute path
set -euo pipefail
DX_ROOT="${DX_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
[ -f "$DX_ROOT/port-ex-machina/scripts/lib/common.sh" ] ||
    { echo "error: no Port Ex Machina workspace at $DX_ROOT/port-ex-machina (set DX_ROOT)" >&2; exit 1; }
. "$DX_ROOT/port-ex-machina/scripts/lib/common.sh"

perf="$DX_DEPS/perf"
say "perf -> $perf"
dx_fetch_arch_pkg "$perf" perf          7.2.7-1                  x86_64 usr/bin/perf
dx_fetch_arch_pkg "$perf" slang         2.3.3-4                  x86_64 usr/lib
dx_fetch_arch_pkg "$perf" numactl       2.0.19-1                 x86_64 usr/lib
dx_fetch_arch_pkg "$perf" libpfm        4.13.0+r83+g91970fe-1    x86_64 usr/lib
dx_fetch_arch_pkg "$perf" libtraceevent 1:1.9.0-1                x86_64 usr/lib
LD_LIBRARY_PATH="$perf/usr/lib" "$perf/usr/bin/perf" --version >/dev/null ||
    die "perf does not run -- a library it needs is missing from $perf/usr/lib"

layers="$DX_DEPS/vulkan-layers"
say "validation layer -> $layers"
dx_fetch_arch_pkg "$layers" vulkan-validation-layers 1.4.357.0-1 x86_64 usr/lib usr/share/vulkan
# The packaged manifest names the library without a path, for a system install.
mkdir -p "$layers/layers"
sed "s|\"libVkLayer_khronos_validation.so\"|\"$layers/usr/lib/libVkLayer_khronos_validation.so\"|" \
    "$layers/usr/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json" \
    > "$layers/layers/VkLayer_khronos_validation.json"

say "perf:       LD_LIBRARY_PATH=$perf/usr/lib $perf/usr/bin/perf ..."
say "validation: VK_LAYER_PATH=$layers/layers VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true <engine> ..."
