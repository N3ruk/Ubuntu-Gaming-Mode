#!/usr/bin/env python3
"""Original small SPIR-V fixtures for modern compiler shapes, not game dumps."""
import subprocess
import sys
from pathlib import Path

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
base = r"""
OpCapability Shader
%glsl = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main" %position %vertex %uv
OpDecorate %position BuiltIn Position
OpDecorate %vertex BuiltIn VertexIndex
OpDecorate %uv Location 0
%void = OpTypeVoid
%float = OpTypeFloat 32
%int = OpTypeInt 32 1
%vec2 = OpTypeVector %float 2
%vec4 = OpTypeVector %float 4
%out4 = OpTypePointer Output %vec4
%out2 = OpTypePointer Output %vec2
%inint = OpTypePointer Input %int
%position = OpVariable %out4 Output
%uv = OpVariable %out2 Output
%vertex = OpVariable %inint Input
%zero = OpConstant %float 0
%one = OpConstant %float 1
%two = OpConstant %float 2
%four = OpConstant %float 4
%izero = OpConstant %int 0
%ione = OpConstant %int 1
%itwo = OpConstant %int 2
%main_type = OpTypeFunction %void
%helper_type = OpTypeFunction %float %float
%helper = OpFunction %float None %helper_type
%arg = OpFunctionParameter %float
%helper_label = OpLabel
@HELPER@
OpFunctionEnd
%main = OpFunction %void None %main_type
%entry = OpLabel
%idx = OpLoad %int %vertex
%shifted = OpShiftLeftLogical %int %idx %ione
%ix = OpBitwiseAnd %int %shifted %itwo
%iy = OpBitwiseAnd %int %idx %itwo
%x = OpConvertSToF %float %ix
%y = OpConvertSToF %float %iy
%texcoord = OpCompositeConstruct %vec2 %x %y
OpStore %uv %texcoord
%xx = OpFMul %float %x %four
%yy = OpFMul %float %y %four
%px = OpFSub %float %xx %two
%py = OpFSub %float %yy %two
@W@
%result = OpCompositeConstruct %vec4 %px %py %zero %w
OpStore %position %result
OpReturn
OpFunctionEnd
"""
variants = {
    "modern-perspective.vert": ("OpReturnValue %arg", "%w = OpFunctionCall %float %helper %two"),
    "modern-capabilities.vert": ("OpReturnValue %arg", "%w = OpFunctionCall %float %helper %two"),
    "modern-ui.vert": ("OpReturnValue %arg", "%w = OpFunctionCall %float %helper %one"),
    "modern-cancel-ui.vert": ("%cancel = OpFSub %float %arg %arg\n%ret = OpFAdd %float %cancel %one\nOpReturnValue %ret", "%w = OpFunctionCall %float %helper %two"),
    "modern-fma-ui.vert": ("%ret = OpExtInst %float %glsl Fma %arg %zero %one\nOpReturnValue %ret", "%w = OpFunctionCall %float %helper %two"),
    "modern-impure.vert": ("OpStore %uv %constant_uv\nOpReturnValue %arg", "%w = OpFunctionCall %float %helper %two"),
    "modern-dynamic.vert": ("OpReturnValue %arg", "%input = OpLoad %vec4 %in_position\n%component = OpCompositeExtract %float %input 3\n%a = OpFunctionCall %float %helper %component\n%w = OpExtInst %float %glsl Fma %a %two %one"),
    "modern-cancel-dynamic-ui.vert": ("%cancel = OpFSub %float %arg %arg\n%ret = OpFAdd %float %cancel %one\nOpReturnValue %ret", "%input = OpLoad %vec4 %in_position\n%component = OpCompositeExtract %float %input 3\n%w = OpFunctionCall %float %helper %component"),
}
for name, (helper, expression) in variants.items():
    source = base.replace("@HELPER@", helper).replace("@W@", expression)
    if "capabilities" in name:
        source = source.replace('OpCapability Shader', 'OpCapability Shader\nOpCapability VulkanMemoryModel\nOpCapability PhysicalStorageBufferAddresses\nOpCapability SignedZeroInfNanPreserve\nOpCapability DrawParameters\nOpExtension "SPV_KHR_vulkan_memory_model"\nOpExtension "SPV_KHR_physical_storage_buffer"\nOpExtension "SPV_KHR_float_controls"\nOpExtension "SPV_KHR_shader_draw_parameters"')
        source = source.replace('OpMemoryModel Logical GLSL450', 'OpMemoryModel PhysicalStorageBuffer64 Vulkan')
    if "dynamic" in name:
        source = source.replace('%position %vertex %uv', '%position %vertex %uv %in_position')
        source = source.replace('OpDecorate %uv Location 0', 'OpDecorate %uv Location 0\nOpDecorate %in_position Location 0')
        source = source.replace('%helper = OpFunction', '%in4 = OpTypePointer Input %vec4\n%in_position = OpVariable %in4 Input\n%helper = OpFunction')
    if "impure" in name:
        source = source.replace('%helper = OpFunction', '%constant_uv = OpConstantComposite %vec2 %zero %zero\n%helper = OpFunction')
    assembly = out / (name + '.spvasm')
    binary = out / (name + '.spv')
    assembly.write_text(source)
    subprocess.run(['spirv-as', '--target-env', 'vulkan1.1', str(assembly), '-o', str(binary)], check=True)
    subprocess.run(['spirv-val', '--target-env', 'vulkan1.1', str(binary)], check=True)
