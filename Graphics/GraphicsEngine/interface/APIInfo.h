/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Diligent API information

#define DILIGENT_API_VERSION 240051

#include "../../../Primitives/interface/BasicTypes.h"

namespace Diligent
{

/// Diligent API Info. This tructure can be used to verify API compatibility.
struct APIInfo
{
    size_t StructSize                     = 0;
    int    APIVersion                     = 0;
    size_t RenderTargetBlendDescSize      = 0;
    size_t BlendStateDescSize             = 0;
    size_t BufferDescSize                 = 0;
    size_t BufferDataSize                 = 0;
    size_t BufferFormatSize               = 0;
    size_t BufferViewDescSize             = 0;
    size_t StencilOpDescSize              = 0;
    size_t DepthStencilStateDescSize      = 0;
    size_t SamplerCapsSize                = 0;
    size_t TextureCapsSize                = 0;
    size_t DeviceCapsSize                 = 0;
    size_t DrawAttribsSize                = 0;
    size_t DispatchComputeAttribsSize     = 0;
    size_t ViewportSize                   = 0;
    size_t RectSize                       = 0;
    size_t CopyTextureAttribsSize         = 0;
    size_t DeviceObjectAttribsSize        = 0;
    size_t AdapterAttribsSize             = 0;
    size_t DisplayModeAttribsSize         = 0;
    size_t SwapChainDescSize              = 0;
    size_t FullScreenModeDescSize         = 0;
    size_t EngineCreateInfoSize           = 0;
    size_t EngineGLCreateInfoSize         = 0;
    size_t EngineD3D11CreateInfoSize      = 0;
    size_t EngineD3D12CreateInfoSize      = 0;
    size_t EngineVkCreateInfoSize         = 0;
    size_t EngineMtlCreateInfoSize        = 0;
    size_t BoxSize                        = 0;
    size_t TextureFormatAttribsSize       = 0;
    size_t TextureFormatInfoSize          = 0;
    size_t TextureFormatInfoExtSize       = 0;
    size_t StateTransitionDescSize        = 0;
    size_t LayoutElementSize              = 0;
    size_t InputLayoutDescSize            = 0;
    size_t SampleDescSize                 = 0;
    size_t ShaderResourceVariableDescSize = 0;
    size_t StaticSamplerDescSize          = 0;
    size_t PipelineResourceLayoutDescSize = 0;
    size_t GraphicsPipelineDescSize       = 0;
    size_t ComputePipelineDescSize        = 0;
    size_t PipelineStateDescSize          = 0;
    size_t RasterizerStateDescSize        = 0;
    size_t ResourceMappingEntrySize       = 0;
    size_t ResourceMappingDescSize        = 0;
    size_t SamplerDescSize                = 0;
    size_t ShaderDescSize                 = 0;
    size_t ShaderMacroSize                = 0;
    size_t ShaderCreateInfoSize           = 0;
    size_t ShaderResourceDescSize         = 0;
    size_t DepthStencilClearValueSize     = 0;
    size_t OptimizedClearValueSize        = 0;
    size_t TextureDescSize                = 0;
    size_t TextureSubResDataSize          = 0;
    size_t TextureDataSize                = 0;
    size_t MappedTextureSubresourceSize   = 0;
    size_t TextureViewDescSize            = 0;
};

} // namespace Diligent
