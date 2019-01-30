/*     Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
/// Type conversion routines

#include "stl/array.h"
#include "stl/vector.h"
#include "GraphicsTypes.h"

namespace Diligent
{

VkFormat TexFormatToVkFormat(TEXTURE_FORMAT TexFmt);
TEXTURE_FORMAT VkFormatToTexFormat(VkFormat VkFmt);

VkFormat TypeToVkFormat(VALUE_TYPE ValType, Uint32 NumComponents, Bool bIsNormalized);

VkPipelineRasterizationStateCreateInfo RasterizerStateDesc_To_VkRasterizationStateCI(const RasterizerStateDesc &RasterizerDesc);
VkPipelineDepthStencilStateCreateInfo  DepthStencilStateDesc_To_VkDepthStencilStateCI(const DepthStencilStateDesc &DepthStencilDesc);
void BlendStateDesc_To_VkBlendStateCI(const BlendStateDesc&                              BSDesc, 
                                      VkPipelineColorBlendStateCreateInfo&               ColorBlendStateCI,
                                      stl::vector<VkPipelineColorBlendAttachmentState>&  ColorBlendAttachments);

void InputLayoutDesc_To_VkVertexInputStateCI(const InputLayoutDesc&                                             LayoutDesc, 
                                             VkPipelineVertexInputStateCreateInfo&                              VertexInputStateCI,
                                             stl::array<VkVertexInputBindingDescription, iMaxLayoutElements>&   BindingDescriptions,
                                             stl::array<VkVertexInputAttributeDescription, iMaxLayoutElements>& AttributeDescription);

void PrimitiveTopology_To_VkPrimitiveTopologyAndPatchCPCount(PRIMITIVE_TOPOLOGY   PrimTopology, 
                                                             VkPrimitiveTopology& VkPrimTopology, 
                                                             uint32_t&            PatchControlPoints);

VkCompareOp ComparisonFuncToVkCompareOp(COMPARISON_FUNCTION CmpFunc);
VkFilter FilterTypeToVkFilter(FILTER_TYPE FilterType);
VkSamplerMipmapMode FilterTypeToVkMipmapMode(FILTER_TYPE FilterType);
VkSamplerAddressMode AddressModeToVkAddressMode(TEXTURE_ADDRESS_MODE AddressMode);
VkBorderColor BorderColorToVkBorderColor(const Float32 BorderColor[]);

VkAccessFlags ResourceStateFlagsToVkAccessFlags(RESOURCE_STATE StateFlags);
VkImageLayout ResourceStateToVkImageLayout(RESOURCE_STATE StateFlag);

RESOURCE_STATE VkAccessFlagsToResourceStates(VkAccessFlags AccessFlags);
RESOURCE_STATE VkImageLayoutToResourceState(VkImageLayout Layout);

}
