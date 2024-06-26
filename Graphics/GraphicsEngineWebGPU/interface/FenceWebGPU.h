/*
 *  Copyright 2024 Diligent Graphics LLC
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
/// Definition of the Diligent::IFenceWebGPU interface

#include "../../GraphicsEngine/interface/Fence.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {4B0B5F76-C4C4-4DDA-BD10-85E3CFB778FC}
static DILIGENT_CONSTEXPR INTERFACE_ID IID_FenceWebGPU =
    {0x4B0B5F76, 0xC4C4, 0x4DDA, {0xBD, 0x10, 0x85, 0xE3, 0xCF, 0xB7, 0x78, 0xFC}};

#define DILIGENT_INTERFACE_NAME IFenceWebGPU
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IFenceWebGPUInclusiveMethods \
    IFenceInclusiveMethods
//  IFenceWebGPUMethods FenceWebGPU

#if DILIGENT_CPP_INTERFACE

/// Exposes WebGPU-specific functionality of a fence object.
DILIGENT_BEGIN_INTERFACE(IFenceWebGPU, IFence){};
DILIGENT_END_INTERFACE

#endif

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

typedef struct IFenceWebGPUtbl
{
    IFenceWebGPUInclusiveMethods;
} IFenceWebGPUVtbl;

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
