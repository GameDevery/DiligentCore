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

#include <iomanip>
#include "stl/utility.h"
#include "SPIRVShaderResources.h"
#include "spirv_parser.hpp"
#include "spirv_cross.hpp"
#include "ShaderBase.h"
#include "GraphicsAccessories.h"
#include "StringTools.h"

namespace Diligent
{

template<typename Type>
Type GetResourceArraySize(const spirv_cross::Compiler& Compiler,
                          const spirv_cross::Resource& Res)
{
    const auto& type = Compiler.get_type(Res.type_id);
    uint32_t arrSize = 1;
    if(!type.array.empty())
    {
        // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide#querying-array-types
        VERIFY(type.array.size() == 1, "Only one-dimensional arrays are currently supported");
        arrSize = type.array[0];
    }
    VERIFY(arrSize <= std::numeric_limits<Type>::max(), "Array size exceeds maximum representable value ", std::numeric_limits<Type>::max());
    return static_cast<Type>(arrSize);
}

static uint32_t GetDecorationOffset(const spirv_cross::Compiler& Compiler,
                                    const spirv_cross::Resource& Res,
                                    spv::Decoration Decoration)
{
    VERIFY(Compiler.has_decoration(Res.id, Decoration), "Res \'", Res.name, "\' has no requested decoration");
    uint32_t offset = 0;
    auto declared = Compiler.get_binary_offset_for_decoration(Res.id, Decoration, offset);
    VERIFY(declared, "Requested decoration is not declared"); (void)declared;
    return offset;
}

SPIRVShaderResourceAttribs::SPIRVShaderResourceAttribs(const spirv_cross::Compiler&  Compiler,
                                                       const spirv_cross::Resource&  Res, 
                                                       const char*                   _Name,
                                                       ResourceType                  _Type, 
                                                       SHADER_VARIABLE_TYPE          _VarType,
                                                       Int32                         _ImmutableSamplerInd,
                                                       Uint32                        _SepSmplrOrImgInd)noexcept :
    Name                         (_Name),
    ArraySize                    (GetResourceArraySize<decltype(ArraySize)>(Compiler, Res)),
    Type                         (_Type),
    VarType                      (_VarType),
    ImmutableSamplerInd          (_ImmutableSamplerInd >= 0 ? static_cast<decltype(ImmutableSamplerInd)>(_ImmutableSamplerInd) : InvalidImmutableSamplerInd),
    SepSmplrOrImgInd             (_SepSmplrOrImgInd),
    BindingDecorationOffset      (GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationBinding)),
    DescriptorSetDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationDescriptorSet))
{
    VERIFY(_ImmutableSamplerInd < 0 || _ImmutableSamplerInd <= std::numeric_limits<decltype(ImmutableSamplerInd)>::max(), "Static sampler index is out of representable range" );
    VERIFY(_SepSmplrOrImgInd == SPIRVShaderResourceAttribs::InvalidSepSmplrOrImgInd || _Type == ResourceType::SeparateSampler ||
           _Type == ResourceType::SeparateImage, "Only separate images or separate samplers can be assinged valid SepSmplrOrImgInd value");
}

static Int32 FindImmutableSampler(const ShaderDesc& shaderDesc, const std::string& SamplerName, const char* SamplerSuffix)
{
    for (Uint32 s=0; s < shaderDesc.NumStaticSamplers; ++s)
    {
        const auto& StSam = shaderDesc.StaticSamplers[s];
        if (StreqSuff(SamplerName.c_str(), StSam.SamplerOrTextureName, SamplerSuffix))
            return s;
    }

    return -1;
}

static spv::ExecutionModel ShaderTypeToExecutionModel(SHADER_TYPE ShaderType)
{
    switch(ShaderType)
    {
        case SHADER_TYPE_VERTEX:    return spv::ExecutionModelVertex;
        case SHADER_TYPE_HULL:      return spv::ExecutionModelTessellationControl;
        case SHADER_TYPE_DOMAIN:    return spv::ExecutionModelTessellationEvaluation;
        case SHADER_TYPE_GEOMETRY:  return spv::ExecutionModelGeometry;
        case SHADER_TYPE_PIXEL:     return spv::ExecutionModelFragment;
        case SHADER_TYPE_COMPUTE:   return spv::ExecutionModelGLCompute;

        default:
            UNEXPECTED("Unexpected shader type");
            return spv::ExecutionModelVertex;
    }
}

const std::string& GetUBName(spirv_cross::Compiler& Compiler, const spirv_cross::Resource& UB, const spirv_cross::ParsedIR::Source& IRSource)
{
    // Consider the following HLSL constant buffer:
    //
    //    cbuffer Constants
    //    {
    //        float4x4 g_WorldViewProj;
    //    };
    //
    // glslang emits SPIRV as if the following GLSL was written:
    // 
    //    uniform Constants // UB.name
    //    {
    //        float4x4 g_WorldViewProj;
    //    }; // no instance name
    //
    // DXC emits the byte code that corresponds to the following GLSL: 
    // 
    //    uniform type_Constants // UB.name
    //    {
    //        float4x4 g_WorldViewProj;
    //    }Constants; // get_name(UB.id)
    //
    //
    //                            |     glslang      |         DXC
    //  -------------------------------------------------------------------
    //  UB.name                   |   "Constants"    |   "type_Constants"
    //  Compiler.get_name(UB.id)  |   ""             |   "Constants"
    //
    // Note that for the byte code produced from GLSL, we must always 
    // use UB.name even if the instance name is present

    const auto& instance_name = Compiler.get_name(UB.id);
    return (IRSource.hlsl && !instance_name.empty()) ? instance_name : UB.name;
}

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator&      Allocator, 
                                           IRenderDevice*         pRenderDevice,
                                           stl::vector<uint32_t>  spirv_binary,
                                           const ShaderDesc&      shaderDesc,
                                           const char*            CombinedSamplerSuffix,
                                           bool                   LoadShaderStageInputs,
                                           std::string&           EntryPoint) :
    m_ShaderType(shaderDesc.ShaderType)
{
    // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
	spirv_cross::Parser parser(move(spirv_binary));
	parser.parse();
    auto ParsedIRSource = parser.get_parsed_ir().source;
    spirv_cross::Compiler Compiler(stl::move(parser.get_parsed_ir()));

    spv::ExecutionModel ExecutionModel = ShaderTypeToExecutionModel(shaderDesc.ShaderType);
    auto EntryPoints = Compiler.get_entry_points_and_stages();
    for (const auto& CurrEntryPoint : EntryPoints)
    {
        if (CurrEntryPoint.execution_model == ExecutionModel)
        {
            if (!EntryPoint.empty())
            {
                LOG_WARNING_MESSAGE("More than one entry point of type ", GetShaderTypeLiteralName(shaderDesc.ShaderType), " found in SPIRV binary for shader '", shaderDesc.Name, "'. The first one ('", EntryPoint, "') will be used.");
            }
            else
            {
                EntryPoint = CurrEntryPoint.name;
            }
        }
    }
    if (EntryPoint.empty())
    {
        LOG_ERROR_AND_THROW("Unable to find entry point of type ", GetShaderTypeLiteralName(shaderDesc.ShaderType), " in SPIRV binary for shader '", shaderDesc.Name, "'");
    }
    Compiler.set_entry_point(EntryPoint, ExecutionModel);

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = Compiler.get_shader_resources();
    
    size_t ResourceNamesPoolSize = 0;
    for(const auto &ub : resources.uniform_buffers)
        ResourceNamesPoolSize += GetUBName(Compiler, ub, ParsedIRSource).length() + 1;
    for(auto *pResType : 
        {
            &resources.storage_buffers,
            &resources.storage_images,
            &resources.sampled_images,
            &resources.atomic_counters,
            &resources.separate_images,
            &resources.separate_samplers
        })
    {
        for(const auto &res : *pResType)
            ResourceNamesPoolSize += res.name.length() + 1;
    }

    if (CombinedSamplerSuffix != nullptr)
    {
        ResourceNamesPoolSize += strlen(CombinedSamplerSuffix) + 1;
    }

    Uint32 NumShaderStageInputs = 0;

    if (resources.stage_inputs.empty())
        LoadShaderStageInputs = false;
    if (LoadShaderStageInputs)
    {
        const auto& Extensions = Compiler.get_declared_extensions();
        bool HlslFunctionality1 = false;
        for (const auto& ext : Extensions )
        {
            HlslFunctionality1 = (ext == "SPV_GOOGLE_hlsl_functionality1");
            if (HlslFunctionality1)
                break;
        }
        
        if (HlslFunctionality1)
        {
            for(const auto& Input : resources.stage_inputs)
            {
                if (Compiler.has_decoration(Input.id, spv::Decoration::DecorationHlslSemanticGOOGLE))
                {
                    const auto& Semantic = Compiler.get_decoration_string(Input.id, spv::Decoration::DecorationHlslSemanticGOOGLE);
                    ResourceNamesPoolSize += Semantic.length()+1;
                    ++NumShaderStageInputs;
                }
                else
                {
                    LOG_ERROR_MESSAGE("Shader input '", Input.name, "' does not have DecorationHlslSemanticGOOGLE decoration, which is unexpected as the shader declares SPV_GOOGLE_hlsl_functionality1 extension");
                }
            }
        }
        else
        {
            LoadShaderStageInputs = false;
            LOG_WARNING_MESSAGE("SPIRV byte code of shader '", shaderDesc.Name, "' does not use SPV_GOOGLE_hlsl_functionality1 extension. "
                                "As a result, it is not possible to get semantics of shader inputs and map them to proper locations. "
                                "The shader will still work correctly if all attributes are declared in ascending order without any gaps. "
                                "Enable SPV_GOOGLE_hlsl_functionality1 in your compiler to allow proper mapping of vertex shader inputs.");
        }
    }

    ResourceCounters ResCounters;
    ResCounters.NumUBs       = static_cast<Uint32>(resources.uniform_buffers.size());
    ResCounters.NumSBs       = static_cast<Uint32>(resources.storage_buffers.size());
    ResCounters.NumImgs      = static_cast<Uint32>(resources.storage_images.size());
    ResCounters.NumSmpldImgs = static_cast<Uint32>(resources.sampled_images.size());
    ResCounters.NumACs       = static_cast<Uint32>(resources.atomic_counters.size());
    ResCounters.NumSepSmplrs = static_cast<Uint32>(resources.separate_samplers.size());
    ResCounters.NumSepImgs   = static_cast<Uint32>(resources.separate_images.size());
    Initialize(Allocator, ResCounters, shaderDesc.NumStaticSamplers, NumShaderStageInputs, ResourceNamesPoolSize);

    {
        Uint32 CurrUB = 0;
        for (const auto &UB : resources.uniform_buffers)
        {
            const auto& name = GetUBName(Compiler, UB, ParsedIRSource);
            new (&GetUB(CurrUB++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           UB, 
                                           m_ResourceNames.CopyString(name), 
                                           SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, 
                                           GetShaderVariableType(name, shaderDesc));
        }
        VERIFY_EXPR(CurrUB == GetNumUBs());
    }

    {
        Uint32 CurrSB = 0;
        for (const auto &SB : resources.storage_buffers)
        {
            new (&GetSB(CurrSB++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SB, 
                                           m_ResourceNames.CopyString(SB.name),
                                           SPIRVShaderResourceAttribs::ResourceType::StorageBuffer,
                                           GetShaderVariableType(SB.name, shaderDesc));
        }
        VERIFY_EXPR(CurrSB == GetNumSBs());
    }

    {
        Uint32 CurrSmplImg = 0;
        for (const auto &SmplImg : resources.sampled_images)
        {
            auto ImmutableSamplerInd = FindImmutableSampler(shaderDesc, SmplImg.name, nullptr);
            const auto& type = Compiler.get_type(SmplImg.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::SampledImage;
            new (&GetSmpldImg(CurrSmplImg++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           SmplImg, 
                                           m_ResourceNames.CopyString(SmplImg.name), 
                                           ResType, 
                                           GetShaderVariableType(SmplImg.name, shaderDesc), 
                                           ImmutableSamplerInd);
        }
        VERIFY_EXPR(CurrSmplImg == GetNumSmpldImgs()); 
    }

    {
        Uint32 CurrImg = 0;
        for (const auto &Img : resources.storage_images)
        {
            const auto& type = Compiler.get_type(Img.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::StorageImage;
            new (&GetImg(CurrImg++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           Img, 
                                           m_ResourceNames.CopyString(Img.name), 
                                           ResType, 
                                           GetShaderVariableType(Img.name, shaderDesc));
        }
        VERIFY_EXPR(CurrImg == GetNumImgs());
    }

    {
        Uint32 CurrAC = 0;
        for (const auto &AC : resources.atomic_counters)
        {
            new (&GetAC(CurrAC++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           AC, 
                                           m_ResourceNames.CopyString(AC.name),
                                           SPIRVShaderResourceAttribs::ResourceType::AtomicCounter,
                                           GetShaderVariableType(AC.name, shaderDesc));
        }
        VERIFY_EXPR(CurrAC == GetNumACs());
    }

    {
        Uint32 CurrSepSmpl = 0;
        for (const auto &SepSam : resources.separate_samplers)
        {
            auto ImmutableSamplerInd = FindImmutableSampler(shaderDesc, SepSam.name, CombinedSamplerSuffix);
            // Use texture or sampler name to derive sampler type
            auto VarType = GetShaderVariableType(shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables,
                                                 [&](const char* VarName)
                                                 {
                                                     return StreqSuff(SepSam.name.c_str(), VarName, CombinedSamplerSuffix);
                                                 });

            new (&GetSepSmplr(CurrSepSmpl++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SepSam, 
                                           m_ResourceNames.CopyString(SepSam.name),
                                           SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, 
                                           VarType,
                                           ImmutableSamplerInd);
        }
        VERIFY_EXPR(CurrSepSmpl == GetNumSepSmplrs());
    }

    {
        Uint32 CurrSepImg = 0;
        for (const auto &SepImg : resources.separate_images)
        {
            Uint32 SamplerInd = SPIRVShaderResourceAttribs::InvalidSepSmplrOrImgInd;
            if (CombinedSamplerSuffix != nullptr)
            {
                auto NumSepSmpls = GetNumSepSmplrs();
                for (SamplerInd = 0; SamplerInd < NumSepSmpls; ++SamplerInd)
                {
                    auto& SepSmplr = GetSepSmplr(SamplerInd);
                    if (StreqSuff(SepSmplr.Name, SepImg.name.c_str(), CombinedSamplerSuffix))
                    {
                        SepSmplr.AssignSeparateImage(CurrSepImg);
                        // Do no assign immutable samplers to separate images as immutable
                        // samplers are permanently bound into the set layout
                        if (SepSmplr.IsImmutableSamplerAssigned())
                            SamplerInd = NumSepSmpls;
                        break;
                    }
                }
                if (SamplerInd == NumSepSmpls)
                    SamplerInd = SPIRVShaderResourceAttribs::InvalidSepSmplrOrImgInd;
            }
            auto* pNewSepImg = new (&GetSepImg(CurrSepImg++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SepImg, 
                                           m_ResourceNames.CopyString(SepImg.name),
                                           SPIRVShaderResourceAttribs::ResourceType::SeparateImage,
                                           GetShaderVariableType(SepImg.name, shaderDesc), 
                                           -1,
                                           SamplerInd);
            if (pNewSepImg->IsValidSepSamplerAssigned())
            {
#ifdef DEVELOPMENT
                const auto& SepSmplr = GetSepSmplr(pNewSepImg->GetAssignedSepSamplerInd());
                DEV_CHECK_ERR(SepSmplr.ArraySize == 1 || SepSmplr.ArraySize == pNewSepImg->ArraySize,
                              "Array size (", SepSmplr.ArraySize,") of separate sampler variable '",
                              SepSmplr.Name, "' must be equal to 1 or be the same as the array size (", pNewSepImg->ArraySize,
                              ") of separate image variable '", pNewSepImg->Name, "' it is assigned to");
#endif
            }
        }
        VERIFY_EXPR(CurrSepImg == GetNumSepImgs());
    }
    
    if (CombinedSamplerSuffix != nullptr)
    {
        m_CombinedSamplerSuffix = m_ResourceNames.CopyString(CombinedSamplerSuffix);
    }

    for (Uint32 s = 0; s < m_NumImmutableSamplers; ++s)
    {
        SamplerPtrType& pStaticSampler = GetImmutableSampler(s);
        new (std::addressof(pStaticSampler)) SamplerPtrType;
        pRenderDevice->CreateSampler(shaderDesc.StaticSamplers[s].Desc, &pStaticSampler);
    }

    if (LoadShaderStageInputs)
    {
        Uint32 CurrStageInput = 0;
        for(const auto& Input : resources.stage_inputs)
        {
            if (Compiler.has_decoration(Input.id, spv::Decoration::DecorationHlslSemanticGOOGLE))
            {
                const auto& Semantic = Compiler.get_decoration_string(Input.id, spv::Decoration::DecorationHlslSemanticGOOGLE);
                new (&GetShaderStageInputAttribs(CurrStageInput++)) 
                    SPIRVShaderStageInputAttribs(m_ResourceNames.CopyString(Semantic), GetDecorationOffset(Compiler, Input, spv::Decoration::DecorationLocation));
            }
        }
        VERIFY_EXPR(CurrStageInput == GetNumShaderStageInputs());
    }

    VERIFY(m_ResourceNames.GetRemainingSize() == 0, "Names pool must be empty");

    //LOG_INFO_MESSAGE(DumpResources());

#ifdef DEVELOPMENT
    if (shaderDesc.NumVariables != 0)
    {
        for (Uint32 v = 0; v < shaderDesc.NumVariables; ++v)
        {
            bool VariableFound = false;
            const auto* VarName = shaderDesc.VariableDesc[v].Name;
            auto VarType = shaderDesc.VariableDesc[v].Type;

            for (Uint32 res = 0; res < GetTotalResources(); ++res)
            {
                const auto& ResAttribs = GetResource(res);
                if (strcmp(ResAttribs.Name, VarName) == 0)
                {
                    VariableFound = true;
                    break;
                }
            }
            if (!VariableFound)
            {
                LOG_WARNING_MESSAGE("Variable '", VarName, "' labeled as ", GetShaderVariableTypeLiteralName(VarType), " is not found in shader '", shaderDesc.Name, "'");
            }
        }
    }

    if (shaderDesc.NumStaticSamplers != 0)
    {
        for (Uint32 s = 0; s < shaderDesc.NumStaticSamplers; ++s)
        {
            const auto* SamName = shaderDesc.StaticSamplers[s].SamplerOrTextureName;
            bool SamplerFound = false;

            // Irrespective of whether HLSL-style combined image samplers are used,
            // a static sampler can be assigned to GLSL sampled image (i.e. sampler2D g_tex)
            for (Uint32 i = 0; i < GetNumSmpldImgs(); ++i)
            {
                const auto& SmplImg = GetSmpldImg(i);
                SamplerFound = (strcmp(SmplImg.Name, SamName) == 0);
                if (SamplerFound)
                    break;
            }

            if (!SamplerFound)
            {
                // Check if static sampler is assigned to a separate sampler or
                // separate image depending on whether HLSL-style combined samplers
                // are used
                for (Uint32 i = 0; i < GetNumSepSmplrs(); ++i)
                {
                    const auto& SepSmpl = GetSepSmplr(i);
                    SamplerFound = StreqSuff(SepSmpl.Name, SamName, CombinedSamplerSuffix);
                    if (SamplerFound)
                        break;
                }
            }

            if (!SamplerFound)
            {
                LOG_WARNING_MESSAGE("Static sampler '", SamName, "' is not found in shader '", shaderDesc.Name, "'");
            }
        }
    }

    if (CombinedSamplerSuffix != nullptr)
    {
        for (Uint32 n=0; n < GetNumSepSmplrs(); ++n)
        {
            const auto& SepSmplr = GetSepSmplr(n);
            if (!SepSmplr.IsValidSepImageAssigned())
                LOG_ERROR_MESSAGE("Shader '", shaderDesc.Name, "' uses combined texture samplers, but separate sampler '", SepSmplr.Name, "' is not assigned to any texture");
        }
    }
#endif
}

void SPIRVShaderResources::Initialize(IMemoryAllocator&       Allocator, 
                                      const ResourceCounters& Counters,
                                      Uint32                  NumImmutableSamplers,
                                      Uint32                  NumShaderStageInputs,
                                      size_t                  ResourceNamesPoolSize)
{
    Uint32 CurrentOffset = 0;
    constexpr Uint32 MaxOffset = std::numeric_limits<OffsetType>::max();
    auto AdvanceOffset = [&CurrentOffset, MaxOffset](Uint32 NumResources)
    {
        VERIFY(CurrentOffset <= MaxOffset, "Current offser (", CurrentOffset, ") exceeds max allowed value (", MaxOffset, ")"); (void)MaxOffset;
        auto Offset = static_cast<OffsetType>(CurrentOffset);
        CurrentOffset += NumResources;
        return Offset;
    };

    auto UniformBufferOffset = AdvanceOffset(Counters.NumUBs); (void)UniformBufferOffset;
    m_StorageBufferOffset    = AdvanceOffset(Counters.NumSBs);
    m_StorageImageOffset     = AdvanceOffset(Counters.NumImgs);
    m_SampledImageOffset     = AdvanceOffset(Counters.NumSmpldImgs);
    m_AtomicCounterOffset    = AdvanceOffset(Counters.NumACs);
    m_SeparateSamplerOffset  = AdvanceOffset(Counters.NumSepSmplrs);
    m_SeparateImageOffset    = AdvanceOffset(Counters.NumSepImgs);
    m_TotalResources         = AdvanceOffset(0);

    VERIFY(NumImmutableSamplers <= MaxOffset, "Max offset exceeded");
    m_NumImmutableSamplers = static_cast<OffsetType>(NumImmutableSamplers);

    VERIFY(NumShaderStageInputs <= MaxOffset, "Max offset exceeded");
    m_NumShaderStageInputs = static_cast<OffsetType>(NumShaderStageInputs);

    static_assert(sizeof(SPIRVShaderResourceAttribs) % sizeof(void*) == 0, "Size of SPIRVShaderResourceAttribs struct must be multiple of sizeof(void*)");
    static_assert(sizeof(SamplerPtrType)             % sizeof(void*) == 0, "Size of SamplerPtrType must be multiple of sizeof(void*)");
    auto MemorySize = m_TotalResources       * sizeof(SPIRVShaderResourceAttribs) + 
                      m_NumImmutableSamplers * sizeof(SamplerPtrType) +
                      m_NumShaderStageInputs * sizeof(SPIRVShaderStageInputAttribs) +
                      ResourceNamesPoolSize  * sizeof(char);

    VERIFY_EXPR(GetNumUBs()       == Counters.NumUBs);
    VERIFY_EXPR(GetNumSBs()       == Counters.NumSBs);
    VERIFY_EXPR(GetNumImgs()      == Counters.NumImgs);
    VERIFY_EXPR(GetNumSmpldImgs() == Counters.NumSmpldImgs);
    VERIFY_EXPR(GetNumACs()       == Counters.NumACs);
    VERIFY_EXPR(GetNumSepSmplrs() == Counters.NumSepSmplrs);
    VERIFY_EXPR(GetNumSepImgs()   == Counters.NumSepImgs);

    if (MemorySize)
    {
        auto *pRawMem = Allocator.Allocate(MemorySize, "Memory for shader resources", __FILE__, __LINE__);
        m_MemoryBuffer = stl::unique_ptr<void, STDDeleterRawMem<void>>(pRawMem, Allocator);
        char* NamesPool = reinterpret_cast<char*>(m_MemoryBuffer.get()) + 
                          m_TotalResources       * sizeof(SPIRVShaderResourceAttribs) +
                          m_NumImmutableSamplers * sizeof(SamplerPtrType) +
                          m_NumShaderStageInputs * sizeof(SPIRVShaderStageInputAttribs);
        m_ResourceNames.AssignMemory(NamesPool, ResourceNamesPoolSize);
    }
}

SPIRVShaderResources::~SPIRVShaderResources()
{
    for (Uint32 n = 0; n < GetNumUBs(); ++n)
        GetUB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSBs(); ++n)
        GetSB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumImgs(); ++n)
        GetImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSmpldImgs(); ++n)
        GetSmpldImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumACs(); ++n)
        GetAC(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepSmplrs(); ++n)
        GetSepSmplr(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepImgs(); ++n)
        GetSepImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumImmutableSamplers(); ++n)
        GetImmutableSampler(n).~SamplerPtrType();
    
    for (Uint32 n = 0; n < GetNumShaderStageInputs(); ++n)
        GetShaderStageInputAttribs(n).~SPIRVShaderStageInputAttribs();
}

SPIRVShaderResources::ResourceCounters  SPIRVShaderResources::CountResources(const SHADER_VARIABLE_TYPE* AllowedVarTypes,
                                                                             Uint32 NumAllowedTypes)const noexcept
{
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes); (void)AllowedTypeBits;
    ResourceCounters Counters;

    ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs& UB, Uint32)
        {
            VERIFY_EXPR(UB.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer);
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            ++Counters.NumUBs;
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(SB.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer);
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            ++Counters.NumSBs;
        },
        [&](const SPIRVShaderResourceAttribs& Img, Uint32)
        {
            VERIFY_EXPR(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage || Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer);
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            ++Counters.NumImgs;
        },
        [&](const SPIRVShaderResourceAttribs& SmplImg, Uint32)
        {
            VERIFY_EXPR(SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage || SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer);
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            ++Counters.NumSmpldImgs;
        },
        [&](const SPIRVShaderResourceAttribs& AC, Uint32)
        {
            VERIFY_EXPR(AC.Type == SPIRVShaderResourceAttribs::ResourceType::AtomicCounter);
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            ++Counters.NumACs;
        },
        [&](const SPIRVShaderResourceAttribs& SepSmpl, Uint32)
        {
            VERIFY_EXPR(SepSmpl.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler);
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            ++Counters.NumSepSmplrs;
        },
        [&](const SPIRVShaderResourceAttribs& SepImg, Uint32)
        {
            VERIFY_EXPR(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            ++Counters.NumSepImgs;
        }
    );
    return Counters;
}

std::string SPIRVShaderResources::DumpResources()
{
    std::stringstream ss;
    ss << "Resource counters (" << GetTotalResources() << " total):" << std::endl << "UBs: " << GetNumUBs() << "; SBs: " 
        << GetNumSBs() << "; Imgs: " << GetNumImgs() << "; Smpl Imgs: " << GetNumSmpldImgs() << "; ACs: " << GetNumACs() 
        << "; Sep Imgs: " << GetNumSepImgs() << "; Sep Smpls: " << GetNumSepSmplrs() << '.' << std::endl
        << "Num Static Samplers: " << GetNumImmutableSamplers() << std::endl << "Resources:";
    
    Uint32 ResNum = 0;
    auto DumpResource = [&ss, &ResNum](const SPIRVShaderResourceAttribs& Res)
    {
        std::stringstream FullResNameSS;
        FullResNameSS << '\'' << Res.Name;
        if (Res.ArraySize > 1)
            FullResNameSS << '[' << Res.ArraySize << ']';
        FullResNameSS << '\'';
        ss << std::setw(32) << FullResNameSS.str();
        ss << " (" << GetShaderVariableTypeLiteralName(Res.VarType) << ")";

        if (Res.IsImmutableSamplerAssigned())
        {
            ss << " Immutable sampler: " << Res.GetImmutableSamplerInd();
        }
        ++ResNum;
    };

    ProcessResources(nullptr, 0,
        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY(UB.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Uniform Buffer  ";
            DumpResource(UB);
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY(SB.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Storage Buffer  ";
            DumpResource(SB);
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            if(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage)
                ss << std::endl << std::setw(3) << ResNum << " Storage Image   ";
            else if(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer)
                ss << std::endl << std::setw(3) << ResNum << " Storage Txl Buff";
            else
                UNEXPECTED("Unexpected resource type");
            DumpResource(Img);
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            if (SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage)
                ss << std::endl << std::setw(3) << ResNum << " Sampled Image   ";
            else if (SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer)
                ss << std::endl << std::setw(3) << ResNum << " Uniform Txl Buff";
            else
                UNEXPECTED("Unexpected resource type");
            DumpResource(SmplImg);
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY(AC.Type == SPIRVShaderResourceAttribs::ResourceType::AtomicCounter, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Atomic Cntr     ";
            DumpResource(AC);
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY(SepSmpl.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Separate Smpl   ";
            DumpResource(SepSmpl);
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Separate Img    ";
            DumpResource(SepImg);
        }
    );
    VERIFY_EXPR(ResNum == GetTotalResources());
    
    return ss.str();
}



bool SPIRVShaderResources::IsCompatibleWith(const SPIRVShaderResources& Resources)const
{
    if( GetNumUBs()               != Resources.GetNumUBs()        ||
        GetNumSBs()               != Resources.GetNumSBs()        ||
        GetNumImgs()              != Resources.GetNumImgs()       ||
        GetNumSmpldImgs()         != Resources.GetNumSmpldImgs()  ||
        GetNumACs()               != Resources.GetNumACs()        ||
        GetNumSepImgs()           != Resources.GetNumSepImgs()    ||
        GetNumSepSmplrs()         != Resources.GetNumSepSmplrs()  ||
        GetNumImmutableSamplers() != Resources.GetNumImmutableSamplers())
        return false;
    VERIFY_EXPR(GetTotalResources() == Resources.GetTotalResources());

    bool IsCompatible = true;
    ProcessResources(nullptr, 0,
        [&](const SPIRVShaderResourceAttribs& Res, Uint32 n)
        {
            const auto& Res2 = Resources.GetResource(n);
            if (!Res.IsCompatibleWith(Res2))
                IsCompatible = false;
        });

    return IsCompatible;
}

}
