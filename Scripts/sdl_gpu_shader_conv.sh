#!/bin/sh

set -eu

DXC_PATH=${DXC_PATH:-dxc}
if [ "${DXC_PATH##*.}" = "exe" ]; then
    DXC_COMMAND="wine $DXC_PATH"
else
    DXC_COMMAND="$DXC_PATH"
fi
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SHADER_DIR="$SCRIPT_DIR/../Source/relive_lib/Renderer/SDLGPU"
BUILD_DIR=${1:-"$SHADER_DIR/generated"}

mkdir -p "$BUILD_DIR"

eval "$DXC_COMMAND" -spirv -T vs_6_0 -E passthru_vertex "$SHADER_DIR/ShaderPassthru.hlsl" -Fo "$BUILD_DIR/passthru.vert.spv"
eval "$DXC_COMMAND" -spirv -T ps_6_0 -E passthru_fragment "$SHADER_DIR/ShaderPassthru.hlsl" -Fo "$BUILD_DIR/passthru.frag.spv"
eval "$DXC_COMMAND" -spirv -T ps_6_0 -E passthru_filter_fragment "$SHADER_DIR/ShaderPassthru.hlsl" -Fo "$BUILD_DIR/passthru_filter.frag.spv"
eval "$DXC_COMMAND" -spirv -T vs_6_0 -E psx_vertex "$SHADER_DIR/ShaderPsx.hlsl" -Fo "$BUILD_DIR/psx.vert.spv"
eval "$DXC_COMMAND" -spirv -T ps_6_0 -E psx_fragment "$SHADER_DIR/ShaderPsx.hlsl" -Fo "$BUILD_DIR/psx.frag.spv"
eval "$DXC_COMMAND" -spirv -T vs_6_0 -E main "$SHADER_DIR/colored.vert.hlsl" -Fo "$BUILD_DIR/colored.vert.spv"
eval "$DXC_COMMAND" -spirv -T ps_6_0 -E main "$SHADER_DIR/colored.frag.hlsl" -Fo "$BUILD_DIR/colored.frag.spv"
glslangValidator -V --target-env vulkan1.2 -S vert -e main "$SHADER_DIR/ft4.vert.glsl" -o "$BUILD_DIR/ft4.vert.spv"
glslangValidator -V --target-env vulkan1.2 -S frag -e main "$SHADER_DIR/ft4.frag.glsl" -o "$BUILD_DIR/ft4.frag.spv"

for shader in "$BUILD_DIR"/*.spv; do
    name=$(basename "$shader" .spv | tr '.-' '__')
    python3 "$SCRIPT_DIR/bin2header.py" -i "$shader" -o "$BUILD_DIR/${name}.hpp" -v "g_${name}"
done