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

#include "pch.h"

#include "stl/utility.h"
#include "stl/algorithm.h"

#include <d3dcompiler.h>

#include "ShaderResourceLayoutD3D11.h"
#include "ShaderResourceCacheD3D11.h"
#include "BufferD3D11Impl.h"
#include "BufferViewD3D11Impl.h"
#include "TextureBaseD3D11.h"
#include "TextureViewD3D11.h"
#include "SamplerD3D11Impl.h"
#include "ShaderD3D11Impl.h"

namespace Diligent
{

ShaderResourceLayoutD3D11::ShaderResourceLayoutD3D11(IObject& Owner) : 
    m_Owner(Owner)
{
}

ShaderResourceLayoutD3D11::~ShaderResourceLayoutD3D11()
{
    HandleResources(
        [&](ConstBuffBindInfo& cb)
        {
            cb.~ConstBuffBindInfo();
        },

        [&](TexSRVBindInfo& ts)
        {
            ts.~TexSRVBindInfo();
        },

        [&](TexUAVBindInfo& uav)
        {
            uav.~TexUAVBindInfo();
        },

        [&](BuffSRVBindInfo& srv)
        {
            srv.~BuffSRVBindInfo();
        },

        [&](BuffUAVBindInfo& uav)
        {
            uav.~BuffUAVBindInfo();
        },

        [&](SamplerBindInfo& sam)
        {
            sam.~SamplerBindInfo();
        }
    );
}

size_t ShaderResourceLayoutD3D11::GetRequiredMemorySize(const ShaderResourcesD3D11& SrcResources, 
                                                        const SHADER_VARIABLE_TYPE* VarTypes, 
                                                        Uint32                      NumVarTypes)
{
    auto ResCounters = SrcResources.CountResources(VarTypes, NumVarTypes);
    auto MemSize = ResCounters.NumCBs      * sizeof(ConstBuffBindInfo) +
                   ResCounters.NumTexSRVs  * sizeof(TexSRVBindInfo)    +
                   ResCounters.NumTexUAVs  * sizeof(TexUAVBindInfo)    +
                   ResCounters.NumBufSRVs  * sizeof(BuffSRVBindInfo)   + 
                   ResCounters.NumBufUAVs  * sizeof(BuffUAVBindInfo)   +
                   ResCounters.NumSamplers * sizeof(SamplerBindInfo);
    return MemSize;
}

void ShaderResourceLayoutD3D11::Initialize(std::shared_ptr<const ShaderResourcesD3D11> pSrcResources,
                                           const SHADER_VARIABLE_TYPE*                 VarTypes, 
                                           Uint32                                      NumVarTypes, 
                                           ShaderResourceCacheD3D11&                   ResourceCache,
                                           IMemoryAllocator&                           ResCacheDataAllocator,
                                           IMemoryAllocator&                           ResLayoutDataAllocator)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-layout#Shader-Resource-Layout-Initialization

    m_pResources = move(pSrcResources);
    m_pResourceCache = &ResourceCache;

    auto AllowedTypeBits = GetAllowedTypeBits(VarTypes, NumVarTypes);

    // Count total number of resources of allowed types
    auto ResCounters = m_pResources->CountResources(VarTypes, NumVarTypes);

    // Initialize offsets
    size_t CurrentOffset = 0;
    auto AdvanceOffset = [&CurrentOffset](size_t NumResources)
    {
        constexpr size_t MaxOffset = std::numeric_limits<OffsetType>::max();
        VERIFY(CurrentOffset <= MaxOffset, "Current offser (", CurrentOffset, ") exceeds max allowed value (", MaxOffset, ")");
        auto Offset = static_cast<OffsetType>(CurrentOffset);
        CurrentOffset += NumResources;
        return Offset;
    };

    auto CBOffset    = AdvanceOffset(ResCounters.NumCBs      * sizeof(ConstBuffBindInfo));  CBOffset; // To suppress warning
    m_TexSRVsOffset  = AdvanceOffset(ResCounters.NumTexSRVs  * sizeof(TexSRVBindInfo)   );
    m_TexUAVsOffset  = AdvanceOffset(ResCounters.NumTexUAVs  * sizeof(TexUAVBindInfo)   );
    m_BuffSRVsOffset = AdvanceOffset(ResCounters.NumBufSRVs  * sizeof(BuffSRVBindInfo)  );
    m_BuffUAVsOffset = AdvanceOffset(ResCounters.NumBufUAVs  * sizeof(BuffUAVBindInfo)  );
    m_SamplerOffset  = AdvanceOffset(ResCounters.NumSamplers * sizeof(SamplerBindInfo)  );
    m_MemorySize     = AdvanceOffset(0);

    VERIFY_EXPR(m_MemorySize == GetRequiredMemorySize(*m_pResources, VarTypes, NumVarTypes));

    if (m_MemorySize)
    {
        auto *pRawMem = ALLOCATE(ResLayoutDataAllocator, "Raw memory buffer for shader resource layout resources", m_MemorySize);
        m_ResourceBuffer = stl::unique_ptr<void, STDDeleterRawMem<void> >(pRawMem, ResLayoutDataAllocator);
    }

    VERIFY_EXPR(ResCounters.NumCBs     == GetNumCBs()     );
    VERIFY_EXPR(ResCounters.NumTexSRVs == GetNumTexSRVs() );
    VERIFY_EXPR(ResCounters.NumTexUAVs == GetNumTexUAVs() );
    VERIFY_EXPR(ResCounters.NumBufSRVs == GetNumBufSRVs() );
    VERIFY_EXPR(ResCounters.NumBufUAVs == GetNumBufUAVs() );
    VERIFY_EXPR(ResCounters.NumSamplers== GetNumSamplers());

    // Current resource index for every resource type
    Uint32 cb     = 0;
    Uint32 texSrv = 0;
    Uint32 texUav = 0;
    Uint32 bufSrv = 0;
    Uint32 bufUav = 0;
    Uint32 sam    = 0;

    Uint32 NumCBSlots = 0;
    Uint32 NumSRVSlots = 0;
    Uint32 NumSamplerSlots = 0;
    Uint32 NumUAVSlots = 0;
    m_pResources->ProcessResources(
        VarTypes, NumVarTypes,

        [&](const D3DShaderResourceAttribs& CB, Uint32)
        {
            VERIFY_EXPR( CB.IsAllowedType(AllowedTypeBits) );

            // Initialize current CB in place, increment CB counter
            new (&GetResource<ConstBuffBindInfo>(cb++)) ConstBuffBindInfo( CB, *this );
            NumCBSlots = stl::max(NumCBSlots, Uint32{CB.BindPoint} + Uint32{CB.BindCount});
        },

        [&](const D3DShaderResourceAttribs& Sampler, Uint32)
        {
            VERIFY_EXPR(Sampler.IsAllowedType(AllowedTypeBits));

            // Skip static samplers as they are initialized in the resource cache
            if (!Sampler.IsStaticSampler())
            {
                // Initialize current sampler in place, increment sampler counter
                new (&GetResource<SamplerBindInfo>(sam++)) SamplerBindInfo( Sampler, *this );
                NumSamplerSlots = stl::max(NumSamplerSlots, Uint32{Sampler.BindPoint} + Uint32{Sampler.BindCount});
            }
        },

        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            VERIFY_EXPR( TexSRV.IsAllowedType(AllowedTypeBits) );
            auto NumSamplers = GetNumSamplers();
            VERIFY(sam == NumSamplers, "All samplers must be initialized before texture SRVs");

            Uint32 AssignedSamplerIndex = TexSRVBindInfo::InvalidSamplerIndex;
            if (TexSRV.ValidSamplerAssigned())
            {
                const auto& AssignedSamplerAttribs = m_pResources->GetSampler(TexSRV.GetSamplerId());
                DEV_CHECK_ERR(AssignedSamplerAttribs.GetVariableType() == TexSRV.GetVariableType(),
                              "The type (", GetShaderVariableTypeLiteralName(TexSRV.GetVariableType()),") of texture SRV variable '", TexSRV.Name,
                              "' is not consistent with the type (", GetShaderVariableTypeLiteralName(AssignedSamplerAttribs.GetVariableType()),
                               ") of the sampler '", AssignedSamplerAttribs.Name, "' that is assigned to it");
                // Do not assign static sampler to texture SRV as it is initialized directly in the shader resource cache
                if (!AssignedSamplerAttribs.IsStaticSampler())
                {
                    for (AssignedSamplerIndex = 0; AssignedSamplerIndex < NumSamplers; ++AssignedSamplerIndex)
                    {
                        const auto& Sampler = GetResource<SamplerBindInfo>(AssignedSamplerIndex);
                        if (strcmp(Sampler.Attribs.Name, AssignedSamplerAttribs.Name) == 0)
                            break;
                    }
                    VERIFY(AssignedSamplerIndex < NumSamplers, "Unable to find assigned sampler");
                }
            }

            // Initialize tex SRV in place, increment counter of tex SRVs
            new (&GetResource<TexSRVBindInfo>(texSrv++)) TexSRVBindInfo( TexSRV, AssignedSamplerIndex, *this );
            NumSRVSlots = stl::max(NumSRVSlots, Uint32{TexSRV.BindPoint} + Uint32{TexSRV.BindCount});
        },

        [&](const D3DShaderResourceAttribs& TexUAV, Uint32)
        {
            VERIFY_EXPR( TexUAV.IsAllowedType(AllowedTypeBits) );
             
            // Initialize tex UAV in place, increment counter of tex UAVs
            new (&GetResource<TexUAVBindInfo>(texUav++)) TexUAVBindInfo( TexUAV, *this );
            NumUAVSlots = stl::max(NumUAVSlots, Uint32{TexUAV.BindPoint} + Uint32{TexUAV.BindCount});
        },

        [&](const D3DShaderResourceAttribs& BuffSRV, Uint32)
        {
            VERIFY_EXPR(BuffSRV.IsAllowedType(AllowedTypeBits));
            
            // Initialize buff SRV in place, increment counter of buff SRVs
            new (&GetResource<BuffSRVBindInfo>(bufSrv++)) BuffSRVBindInfo( BuffSRV, *this );
            NumSRVSlots = stl::max(NumSRVSlots, Uint32{BuffSRV.BindPoint} + Uint32{BuffSRV.BindCount});
        },

        [&](const D3DShaderResourceAttribs& BuffUAV, Uint32)
        {
            VERIFY_EXPR(BuffUAV.IsAllowedType(AllowedTypeBits));
            
            // Initialize buff UAV in place, increment counter of buff UAVs
            new (&GetResource<BuffUAVBindInfo>(bufUav++)) BuffUAVBindInfo( BuffUAV, *this );
            NumUAVSlots = stl::max(NumUAVSlots, Uint32{BuffUAV.BindPoint} + Uint32{BuffUAV.BindCount});
        }
    );

    VERIFY(cb     == GetNumCBs(),      "Not all CBs are initialized which will cause a crash when dtor is called");
    VERIFY(texSrv == GetNumTexSRVs(),  "Not all Tex SRVs are initialized which will cause a crash when dtor is called");
    VERIFY(texUav == GetNumTexUAVs(),  "Not all Tex UAVs are initialized which will cause a crash when dtor is called");
    VERIFY(bufSrv == GetNumBufSRVs(),  "Not all Buf SRVs are initialized which will cause a crash when dtor is called");
    VERIFY(bufUav == GetNumBufUAVs(),  "Not all Buf UAVs are initialized which will cause a crash when dtor is called");
    VERIFY(sam    == GetNumSamplers(), "Not all samplers are initialized which will cause a crash when dtor is called");

    // Shader resource cache in the SRB is initialized by the constructor of ShaderResourceBindingD3D11Impl to
    // hold all variable types. The corresponding layout in the SRB is initialized to keep mutable and dynamic 
    // variables only
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache#Shader-Resource-Cache-Initialization
    if (!m_pResourceCache->IsInitialized())
    {
        // NOTE that here we are using max bind points required to cache only the shader variables of allowed types!
        m_pResourceCache->Initialize(NumCBSlots, NumSRVSlots, NumSamplerSlots, NumUAVSlots, ResCacheDataAllocator);
    }
}

void ShaderResourceLayoutD3D11::CopyResources(ShaderResourceCacheD3D11& DstCache)
{
    VERIFY(m_pResourceCache, "Resource cache must not be null");

    VERIFY( DstCache.GetCBCount()      >= m_pResourceCache->GetCBCount(),      "Dst cache is not large enough to contain all CBs" );
    VERIFY( DstCache.GetSRVCount()     >= m_pResourceCache->GetSRVCount(),     "Dst cache is not large enough to contain all SRVs" );
    VERIFY( DstCache.GetSamplerCount() >= m_pResourceCache->GetSamplerCount(), "Dst cache is not large enough to contain all samplers" );
    VERIFY( DstCache.GetUAVCount()     >= m_pResourceCache->GetUAVCount(),     "Dst cache is not large enough to contain all UAVs" );

    ShaderResourceCacheD3D11::CachedCB*       CachedCBs          = nullptr;
    ID3D11Buffer**                            d3d11CBs           = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedSRVResources = nullptr;
    ID3D11ShaderResourceView**                d3d11SRVs          = nullptr;
    ShaderResourceCacheD3D11::CachedSampler* CachedSamplers      = nullptr;
    ID3D11SamplerState**                     d3d11Samplers       = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedUAVResources = nullptr;
    ID3D11UnorderedAccessView**               d3d11UAVs          = nullptr;
    m_pResourceCache->GetCBArrays     (CachedCBs,          d3d11CBs);
    m_pResourceCache->GetSRVArrays    (CachedSRVResources, d3d11SRVs);
    m_pResourceCache->GetSamplerArrays(CachedSamplers,     d3d11Samplers);
    m_pResourceCache->GetUAVArrays    (CachedUAVResources, d3d11UAVs);


    ShaderResourceCacheD3D11::CachedCB*       DstCBs           = nullptr;
    ID3D11Buffer**                            DstD3D11CBs      = nullptr;
    ShaderResourceCacheD3D11::CachedResource* DstSRVResources  = nullptr;
    ID3D11ShaderResourceView**                DstD3D11SRVs     = nullptr;
    ShaderResourceCacheD3D11::CachedSampler*  DstSamplers      = nullptr;
    ID3D11SamplerState**                      DstD3D11Samplers = nullptr;
    ShaderResourceCacheD3D11::CachedResource* DstUAVResources  = nullptr;
    ID3D11UnorderedAccessView**               DstD3D11UAVs     = nullptr;
    DstCache.GetCBArrays     (DstCBs,          DstD3D11CBs);
    DstCache.GetSRVArrays    (DstSRVResources, DstD3D11SRVs);
    DstCache.GetSamplerArrays(DstSamplers,     DstD3D11Samplers);
    DstCache.GetUAVArrays    (DstUAVResources, DstD3D11UAVs);

    HandleResources(
        [&](const ConstBuffBindInfo& cb)
        {
            for(auto CBSlot = cb.Attribs.BindPoint; CBSlot < cb.Attribs.BindPoint+cb.Attribs.BindCount; ++CBSlot)
            {
                VERIFY_EXPR(CBSlot < m_pResourceCache->GetCBCount() && CBSlot < DstCache.GetCBCount());
                DstCBs     [CBSlot] = CachedCBs[CBSlot];
                DstD3D11CBs[CBSlot] = d3d11CBs [CBSlot];
            }
        },

        [&](const TexSRVBindInfo& ts)
        {
            for(auto SRVSlot = ts.Attribs.BindPoint; SRVSlot < ts.Attribs.BindPoint + ts.Attribs.BindCount; ++SRVSlot)
            {
                VERIFY_EXPR(SRVSlot < m_pResourceCache->GetSRVCount() && SRVSlot < DstCache.GetSRVCount());
                DstSRVResources[SRVSlot] = CachedSRVResources[SRVSlot];
                DstD3D11SRVs   [SRVSlot] = d3d11SRVs         [SRVSlot];
            }
        },

        [&](const TexUAVBindInfo& uav)
        {
            for(auto UAVSlot = uav.Attribs.BindPoint; UAVSlot < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++UAVSlot)
            {
                VERIFY_EXPR(UAVSlot < m_pResourceCache->GetUAVCount() && UAVSlot < DstCache.GetUAVCount());
                DstUAVResources[UAVSlot] = CachedUAVResources[UAVSlot];
                DstD3D11UAVs   [UAVSlot] = d3d11UAVs         [UAVSlot];
            }
        },

        [&](const BuffSRVBindInfo& srv)
        {
            for(auto SRVSlot = srv.Attribs.BindPoint; SRVSlot < srv.Attribs.BindPoint + srv.Attribs.BindCount; ++SRVSlot)
            {
                VERIFY_EXPR(SRVSlot < m_pResourceCache->GetSRVCount() && SRVSlot < DstCache.GetSRVCount());
                DstSRVResources[SRVSlot] = CachedSRVResources[SRVSlot];
                DstD3D11SRVs   [SRVSlot] = d3d11SRVs         [SRVSlot];
            }
        },

        [&](const BuffUAVBindInfo& uav)
        {
            for(auto UAVSlot = uav.Attribs.BindPoint; UAVSlot < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++UAVSlot)
            {
                VERIFY_EXPR(UAVSlot < m_pResourceCache->GetUAVCount() && UAVSlot < DstCache.GetUAVCount());
                DstUAVResources[UAVSlot] = CachedUAVResources[UAVSlot];
                DstD3D11UAVs   [UAVSlot] = d3d11UAVs         [UAVSlot];
            }
        },

        [&](const SamplerBindInfo& sam)
        {
            VERIFY(!sam.Attribs.IsStaticSampler(), "Variables are not created for static samplers");
            for(auto SamSlot = sam.Attribs.BindPoint; SamSlot < sam.Attribs.BindPoint + sam.Attribs.BindCount; ++SamSlot)
            {
                VERIFY_EXPR(SamSlot < m_pResourceCache->GetSamplerCount() && SamSlot < DstCache.GetSamplerCount());
                DstSamplers     [SamSlot] = CachedSamplers[SamSlot];
                DstD3D11Samplers[SamSlot] = d3d11Samplers [SamSlot];
            }
        }
    );
}

#define LOG_RESOURCE_BINDING_ERROR(ResType, pResource, Attribs, ArrayInd, ShaderName, ...)\
do{                                                                                       \
    const auto* ResName = pResource->GetDesc().Name;                                      \
    if(Attribs.BindCount>1)                                                               \
        LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " '", ResName, "' to variable '", Attribs.Name,\
                           "[", ArrayInd, "]' in shader '", ShaderName, "'. ", __VA_ARGS__ );         \
    else                                                                                                 \
        LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " '", ResName, "' to variable '", Attribs.Name,\
                           "' in shader '", ShaderName, "'. ", __VA_ARGS__ );                         \
}while(false)

void ShaderResourceLayoutD3D11::ConstBuffBindInfo::BindResource(IDeviceObject* pBuffer,
                                                                Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache != nullptr, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferD3D11Impl> pBuffD3D11Impl(pBuffer, IID_BufferD3D11);
#ifdef DEVELOPMENT
    if (pBuffer && !pBuffD3D11Impl)
        LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorrect resource type: buffer is expected.");

    if (pBuffD3D11Impl && (pBuffD3D11Impl->GetDesc().BindFlags & BIND_UNIFORM_BUFFER) == 0)
    {
        LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNIFORM_BUFFER flag.");
        pBuffD3D11Impl.Release();
    }
       
    if (Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedCB = ResourceCache.GetCB(Attribs.BindPoint + ArrayIndex);
        if (CachedCB.pBuff != nullptr && CachedCB.pBuff != pBuffD3D11Impl)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null constant buffer is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif

    ResourceCache.SetCB(Attribs.BindPoint + ArrayIndex, stl::move(pBuffD3D11Impl) );
}



bool ShaderResourceLayoutD3D11::ConstBuffBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsCBBound(Attribs.BindPoint + ArrayIndex);
}



#ifdef DEVELOPMENT
template<typename TResourceViewType, ///< Type of the view (ITextureViewD3D11 or IBufferViewD3D11)
         typename TViewTypeEnum>     ///< Type of the expected view enum
bool dbgVerifyViewType( const char*                     ViewTypeName,
                        TResourceViewType               pViewD3D11,
                        const D3DShaderResourceAttribs& Attribs, 
                        Uint32                          ArrayIndex,
                        TViewTypeEnum                   dbgExpectedViewType,
                        const Char*                     ShaderName )
{
    const auto& ViewDesc = pViewD3D11->GetDesc();
    auto ViewType = ViewDesc.ViewType;
    if (ViewType == dbgExpectedViewType)
    {
        return true;
    }
    else
    {
        const auto *ExpectedViewTypeName = GetViewTypeLiteralName( dbgExpectedViewType );
        const auto *ActualViewTypeName = GetViewTypeLiteralName( ViewType );
        LOG_RESOURCE_BINDING_ERROR(ViewTypeName, pViewD3D11, Attribs, ArrayIndex, ShaderName, 
                                   "Incorrect view type: ", ExpectedViewTypeName, " is expected, ", ActualViewTypeName, " is provided." );
        return false;
    }
}
#endif

void ShaderResourceLayoutD3D11::TexSRVBindInfo::BindResource(IDeviceObject* pView,
                                                             Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TextureViewD3D11Impl> pViewD3D11(pView, IID_TextureViewD3D11);
#ifdef DEVELOPMENT
    if (pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorect resource type: texture view is expected.");
    if (pViewD3D11 && !dbgVerifyViewType("texture view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, TEXTURE_VIEW_SHADER_RESOURCE, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if( Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedSRV = ResourceCache.GetSRV(Attribs.BindPoint + ArrayIndex);
        if (CachedSRV.pView != nullptr && CachedSRV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null texture SRV is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif
    
    if (ValidSamplerAssigned())
    {
        auto& Sampler = m_ParentResLayout.GetResource<SamplerBindInfo>(SamplerIndex);
        VERIFY(!Sampler.Attribs.IsStaticSampler(), "Static samplers are not assigned to texture SRVs as they are initialized directly in the shader resource cache");
        VERIFY_EXPR(Sampler.Attribs.BindCount == Attribs.BindCount || Sampler.Attribs.BindCount == 1);
        auto SamplerBindPoint = Sampler.Attribs.BindPoint + (Sampler.Attribs.BindCount != 1 ? ArrayIndex : 0);

        SamplerD3D11Impl* pSamplerD3D11Impl = nullptr;
        if (pViewD3D11)
        {
            pSamplerD3D11Impl = ValidatedCast<SamplerD3D11Impl>(pViewD3D11->GetSampler());
#ifdef DEVELOPMENT
            if (pSamplerD3D11Impl == nullptr)
            {
                if(Sampler.Attribs.BindCount > 1)
                    LOG_ERROR_MESSAGE( "Failed to bind sampler to variable '", Sampler.Attribs.Name, "[", ArrayIndex,"]'. Sampler is not set in the texture view '", pViewD3D11->GetDesc().Name, "'" );
                else
                    LOG_ERROR_MESSAGE( "Failed to bind sampler to variable '", Sampler.Attribs.Name, "'. Sampler is not set in the texture view '", pViewD3D11->GetDesc().Name, "'" );
            }
#endif
        }
#ifdef DEVELOPMENT
        if (Sampler.Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            auto& CachedSampler = ResourceCache.GetSampler(SamplerBindPoint);
            if (CachedSampler.pSampler != nullptr && CachedSampler.pSampler != pSamplerD3D11Impl)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
                LOG_ERROR_MESSAGE( "Non-null sampler is already bound to ", VarTypeStr, " shader variable '", Sampler.Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another sampler or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
            }
        }
#endif
        ResourceCache.SetSampler(SamplerBindPoint, pSamplerD3D11Impl);
    }          

    ResourceCache.SetTexSRV(Attribs.BindPoint + ArrayIndex, stl::move(pViewD3D11));
}

void ShaderResourceLayoutD3D11::SamplerBindInfo::BindResource(IDeviceObject* pSampler,
                                                              Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache != nullptr, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;
    VERIFY(!Attribs.IsStaticSampler(), "Cannot bind sampler to a static sampler");

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<SamplerD3D11Impl> pSamplerD3D11(pSampler, IID_SamplerD3D11);

#ifdef DEVELOPMENT
    if (pSampler && !pSamplerD3D11)
        LOG_RESOURCE_BINDING_ERROR("sampler", pSampler, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorect resource type: sampler is expected.");

    if (Attribs.ValidTexSRVAssigned())
    {
        auto* TexSRVName = m_ParentResLayout.m_pResources->GetTexSRV(Attribs.GetTexSRVId()).Name;
        LOG_WARNING_MESSAGE("Texture sampler sampler '", Attribs.Name, "' is assigned to texture SRV '", TexSRVName, "' and should not be accessed directly. The sampler is initialized when texture SRV is set to '", TexSRVName, "' variable.");
    }

    if (Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedSampler = ResourceCache.GetSampler(Attribs.BindPoint + ArrayIndex);
        if( CachedSampler.pSampler != nullptr && CachedSampler.pSampler != pSamplerD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null sampler is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another sampler or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif

    ResourceCache.SetSampler(Attribs.BindPoint + ArrayIndex, stl::move(pSamplerD3D11));
}

void ShaderResourceLayoutD3D11::BuffSRVBindInfo::BindResource(IDeviceObject* pView,
                                                              Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache != nullptr, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferViewD3D11Impl> pViewD3D11(pView, IID_BufferViewD3D11);
#ifdef DEVELOPMENT
    if (pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorect resource type: buffer view is expected.");
    if (pViewD3D11 && !dbgVerifyViewType("buffer view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, BUFFER_VIEW_SHADER_RESOURCE, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if (Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedSRV = ResourceCache.GetSRV(Attribs.BindPoint + ArrayIndex);
        if (CachedSRV.pView != nullptr && CachedSRV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null buffer SRV is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif

    ResourceCache.SetBufSRV(Attribs.BindPoint + ArrayIndex, stl::move(pViewD3D11));
}


void ShaderResourceLayoutD3D11::TexUAVBindInfo::BindResource(IDeviceObject* pView,
                                                             Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache != nullptr, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TextureViewD3D11Impl> pViewD3D11(pView, IID_TextureViewD3D11);
#ifdef DEVELOPMENT
    if (pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorect resource type: texture view is expected.");
    if (pViewD3D11 && !dbgVerifyViewType("texture view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, TEXTURE_VIEW_UNORDERED_ACCESS, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if (Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedUAV = ResourceCache.GetUAV(Attribs.BindPoint + ArrayIndex);
        if (CachedUAV.pView != nullptr && CachedUAV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null texture UAV is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif

    ResourceCache.SetTexUAV(Attribs.BindPoint + ArrayIndex, stl::move(pViewD3D11));
}


void ShaderResourceLayoutD3D11::BuffUAVBindInfo::BindResource(IDeviceObject* pView,
                                                              Uint32         ArrayIndex)
{
    VERIFY(m_ParentResLayout.m_pResourceCache != nullptr, "Resource cache is null");
    DEV_CHECK_ERR(ArrayIndex < Attribs.BindCount, "Array index (", ArrayIndex, ") is out of range for variable '", Attribs.Name, "'. Max allowed index: ", Attribs.BindCount);
    auto& ResourceCache = *m_ParentResLayout.m_pResourceCache;

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferViewD3D11Impl> pViewD3D11(pView, IID_BufferViewD3D11);
#ifdef DEVELOPMENT
    if (pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorect resource type: buffer view is expected.");
    if (pViewD3D11 && !dbgVerifyViewType("buffer view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, BUFFER_VIEW_UNORDERED_ACCESS, m_ParentResLayout.GetShaderName()) )
        pViewD3D11.Release();

    if (Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto& CachedUAV = ResourceCache.GetUAV(Attribs.BindPoint + ArrayIndex);
        if (CachedUAV.pView != nullptr && CachedUAV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
            LOG_ERROR_MESSAGE( "Non-null buffer UAV is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex), "' in shader '", m_ParentResLayout.GetShaderName(), "'. Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic." );
        }
    }
#endif

    ResourceCache.SetBufUAV(Attribs.BindPoint + ArrayIndex, stl::move(pViewD3D11));
}


bool ShaderResourceLayoutD3D11::TexSRVBindInfo::IsBound(Uint32 ArrayIndex)const
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache != nullptr, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsSRVBound(Attribs.BindPoint + ArrayIndex, true);
}


bool ShaderResourceLayoutD3D11::BuffSRVBindInfo::IsBound(Uint32 ArrayIndex)const
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsSRVBound(Attribs.BindPoint + ArrayIndex, false);
}

bool ShaderResourceLayoutD3D11::TexUAVBindInfo::IsBound(Uint32 ArrayIndex)const
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsUAVBound(Attribs.BindPoint + ArrayIndex, true);
}

bool ShaderResourceLayoutD3D11::BuffUAVBindInfo::IsBound(Uint32 ArrayIndex)const
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsUAVBound(Attribs.BindPoint + ArrayIndex, false);
}

bool ShaderResourceLayoutD3D11::SamplerBindInfo::IsBound(Uint32 ArrayIndex)const
{
    auto* pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsSamplerBound(Attribs.BindPoint + ArrayIndex);
}



// Helper template class that facilitates binding CBs, SRVs, and UAVs
class BindResourceHelper
{
public:
    BindResourceHelper(IResourceMapping& RM, Uint32 Fl) :
        ResourceMapping(RM),
        Flags(Fl)
    {
    }

    template<typename ResourceType>
    void Bind( ResourceType &Res)
    {
        if ( (Flags & (1 << Res.Attribs.GetVariableType())) == 0 )
            return;

        for (Uint16 elem=0; elem < Res.Attribs.BindCount; ++elem)
        {
            if ( (Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) && Res.IsBound(elem) )
                continue;

            const auto* VarName = Res.Attribs.Name;
            RefCntAutoPtr<IDeviceObject> pRes;
            ResourceMapping.GetResource( VarName, &pRes, elem );
            if (pRes)
            {
                //  Call non-virtual function
                Res.BindResource(pRes, elem);
            }
            else
            {
                if ( (Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !Res.IsBound(elem) )
                    LOG_ERROR_MESSAGE( "Unable to bind resource to shader variable '", VarName, "': resource is not found in the resource mapping" );
            }
        }
    }

private:
    IResourceMapping& ResourceMapping;
    const Uint32      Flags;
};

void ShaderResourceLayoutD3D11::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheD3D11& dbgResourceCache )
{
    VERIFY(&dbgResourceCache == m_pResourceCache, "Resource cache does not match the cache provided at initialization");

    if (pResourceMapping == nullptr)
    {
        LOG_ERROR_MESSAGE( "Failed to bind resources in shader '", GetShaderName(), "': resource mapping is null" );
        return;
    }
    
    if ( (Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0 )
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    BindResourceHelper BindResHelper(*pResourceMapping, Flags);

    HandleResources(
        [&](ConstBuffBindInfo& cb)
        {
            BindResHelper.Bind(cb);
        },

        [&](TexSRVBindInfo& ts)
        {
            BindResHelper.Bind(ts);
        },

        [&](TexUAVBindInfo& uav)
        {
            BindResHelper.Bind(uav);
        },

        [&](BuffSRVBindInfo& srv)
        {
            BindResHelper.Bind(srv);
        },

        [&](BuffUAVBindInfo& uav)
        {
            BindResHelper.Bind(uav);
        },

        [&](SamplerBindInfo& sam)
        {
            if (!m_pResources->IsUsingCombinedTextureSamplers())
                BindResHelper.Bind(sam);
        }
    );
}

template<typename ResourceType>
IShaderVariable* ShaderResourceLayoutD3D11::GetResourceByName( const Char* Name )
{
    auto NumResources = GetNumResources<ResourceType>();
    for (Uint32 res = 0; res < NumResources; ++res)
    {
        auto& Resource = GetResource<ResourceType>(res);
        if (strcmp(Resource.Attribs.Name, Name) == 0)
            return &Resource;
    }

    return nullptr;
}

IShaderVariable* ShaderResourceLayoutD3D11::GetShaderVariable(const Char* Name)
{
    if(auto* pCB = GetResourceByName<ConstBuffBindInfo>(Name))
        return pCB;

    if(auto* pTexSRV = GetResourceByName<TexSRVBindInfo>(Name))
        return pTexSRV;

    if(auto* pTexUAV = GetResourceByName<TexUAVBindInfo>(Name))
        return pTexUAV;

    if(auto* pBuffSRV = GetResourceByName<BuffSRVBindInfo>(Name))
        return pBuffSRV;

    if(auto* pBuffUAV = GetResourceByName<BuffUAVBindInfo>(Name))
        return pBuffUAV;

    if (!m_pResources->IsUsingCombinedTextureSamplers())
    {
        if(auto* pSampler = GetResourceByName<SamplerBindInfo>(Name))
            return pSampler;
    }

    return nullptr;
}

class ShaderVariableIndexLocator
{
public:
    ShaderVariableIndexLocator(const ShaderResourceLayoutD3D11& _Layout, const ShaderResourceLayoutD3D11::ShaderVariableD3D11Base& Variable) : 
        Layout   (_Layout),
        VarOffset(reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<const Uint8*>(_Layout.m_ResourceBuffer.get()))
    {}

    template<typename ResourceType>
    bool TryResource(ShaderResourceLayoutD3D11::OffsetType NextResourceTypeOffset)
    {
#ifdef _DEBUG
        VERIFY(Layout.GetResourceOffset<ResourceType>() >= dbgPreviousResourceOffset, "Resource types are processed out of order!");
        dbgPreviousResourceOffset = Layout.GetResourceOffset<ResourceType>();
        VERIFY_EXPR(NextResourceTypeOffset >= Layout.GetResourceOffset<ResourceType>());
#endif
        if (VarOffset < NextResourceTypeOffset)
        {
            auto RelativeOffset = VarOffset - Layout.GetResourceOffset<ResourceType>();
            DEV_CHECK_ERR( RelativeOffset % sizeof(ResourceType) == 0, "Offset is not multiple of resource type (", sizeof(ResourceType), ")");
            Index += static_cast<Uint32>(RelativeOffset / sizeof(ResourceType));
            return true;
        }
        else
        {
            Index += Layout.GetNumResources<ResourceType>();
            return false;
        }
    }

    Uint32 GetIndex() const {return Index;}

private:
    const ShaderResourceLayoutD3D11& Layout;
    const size_t VarOffset;
    Uint32 Index = 0;
#ifdef _DEBUG
    Uint32 dbgPreviousResourceOffset = 0;
#endif
};

Uint32 ShaderResourceLayoutD3D11::GetVariableIndex(const ShaderVariableD3D11Base& Variable)const
{
    if (!m_ResourceBuffer)
    {
        LOG_ERROR("This shader resource layout does not have resources");
        return static_cast<Uint32>(-1);
    }
   
    ShaderVariableIndexLocator IdxLocator(*this, Variable);
    if (IdxLocator.TryResource<ConstBuffBindInfo>(m_TexSRVsOffset))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<TexSRVBindInfo>(m_TexUAVsOffset))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<TexUAVBindInfo>(m_BuffSRVsOffset))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<BuffSRVBindInfo>(m_BuffUAVsOffset))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<BuffUAVBindInfo>(m_SamplerOffset))
        return IdxLocator.GetIndex();

    if (!m_pResources->IsUsingCombinedTextureSamplers())
    {
        if (IdxLocator.TryResource<SamplerBindInfo>(m_MemorySize))
            return IdxLocator.GetIndex();
    }

    LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader resource layout");
    return static_cast<Uint32>(-1);
}

class ShaderVariableLocator
{
public:
    ShaderVariableLocator(ShaderResourceLayoutD3D11& _Layout, Uint32 _Index) : 
        Layout(_Layout),
        Index (_Index)
    {
    }

    template<typename ResourceType>
    IShaderVariable* TryResource()
    {
#ifdef _DEBUG
        VERIFY(Layout.GetResourceOffset<ResourceType>() >= dbgPreviousResourceOffset, "Resource types are processed out of order!");
        dbgPreviousResourceOffset = Layout.GetResourceOffset<ResourceType>();
#endif
        auto NumResources = Layout.GetNumResources<ResourceType>();
        if (Index < NumResources)
            return &Layout.GetResource<ResourceType>(Index);
        else
        {
            Index -= NumResources;
            return nullptr;
        }
    }

private:
    ShaderResourceLayoutD3D11& Layout;
    Uint32 Index;
#ifdef _DEBUG
    Uint32 dbgPreviousResourceOffset = 0;
#endif
};

IShaderVariable* ShaderResourceLayoutD3D11::GetShaderVariable( Uint32 Index )
{
    ShaderVariableLocator VarLocator(*this, Index);

    if(auto* pCB = VarLocator.TryResource<ConstBuffBindInfo>())
        return pCB;

    if(auto* pTexSRV = VarLocator.TryResource<TexSRVBindInfo>())
        return pTexSRV;

    if(auto* pTexUAV = VarLocator.TryResource<TexUAVBindInfo>())
        return pTexUAV;

    if(auto* pBuffSRV = VarLocator.TryResource<BuffSRVBindInfo>())
        return pBuffSRV;

    if(auto* pBuffUAV = VarLocator.TryResource<BuffUAVBindInfo>())
        return pBuffUAV;

    if (!m_pResources->IsUsingCombinedTextureSamplers())
    {
        if(auto* pSampler = VarLocator.TryResource<SamplerBindInfo>())
            return pSampler;
    }
    
    auto TotalResCount = GetTotalResourceCount();
    LOG_ERROR(Index, " is not a valid variable index. Total resource count: ", TotalResCount);
    return nullptr;
}


#ifdef DEVELOPMENT
bool ShaderResourceLayoutD3D11::dvpVerifyBindings()const
{

#define LOG_MISSING_BINDING(VarType, Attrs, BindPt)\
do{                                                \
    if (Attrs.BindCount == 1)                      \
        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable '", Attrs.Name, "' in shader '", GetShaderName(), "'" );   \
    else                                                                                                                                  \
        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable '", Attrs.Name, "[", BindPt-Attrs.BindPoint, "]' in shader '", GetShaderName(), "'" );\
}while(false)

    m_pResourceCache->dbgVerifyCacheConsistency();
    
    bool BindingsOK = true;
    // Use const_cast to avoid duplication of the HandleResources() function
    // The function actually changes nothing
    const_cast<ShaderResourceLayoutD3D11*>(this)->HandleResources(
        [&](const ConstBuffBindInfo& cb)
        {
            for (Uint32 BindPoint = cb.Attribs.BindPoint; BindPoint < Uint32{cb.Attribs.BindPoint} + cb.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsCBBound(BindPoint))
                {
                    LOG_MISSING_BINDING("constant buffer", cb.Attribs, BindPoint);
                    BindingsOK  = false;
                }
            }
        },

        [&](const TexSRVBindInfo& ts)
        {
            for (Uint32 BindPoint = ts.Attribs.BindPoint; BindPoint < Uint32{ts.Attribs.BindPoint} + ts.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsSRVBound(BindPoint, true))
                {
                    LOG_MISSING_BINDING("texture", ts.Attribs, BindPoint);
                    BindingsOK  = false;
                }

                if (ts.ValidSamplerAssigned())
                {
                    const auto& Sampler = GetConstResource<SamplerBindInfo>(ts.SamplerIndex);
                    VERIFY_EXPR(Sampler.Attribs.BindCount == ts.Attribs.BindCount || Sampler.Attribs.BindCount == 1);

                    // Verify that if single sampler is used for all texture array elements, all samplers set in the resource views are consistent
                    if (ts.Attribs.BindCount > 1 && Sampler.Attribs.BindCount == 1)
                    {
                        ShaderResourceCacheD3D11::CachedSampler* pCachedSamplers       = nullptr;
                        ID3D11SamplerState**                     ppCachedD3D11Samplers = nullptr;
                        m_pResourceCache->GetSamplerArrays(pCachedSamplers, ppCachedD3D11Samplers);
                        VERIFY_EXPR(Sampler.Attribs.BindPoint < m_pResourceCache->GetSamplerCount());
                        const auto& CachedSampler = pCachedSamplers[Sampler.Attribs.BindPoint];

                        ShaderResourceCacheD3D11::CachedResource* pCachedResources       = nullptr;
                        ID3D11ShaderResourceView**                ppCachedD3D11Resources = nullptr;
                        m_pResourceCache->GetSRVArrays(pCachedResources, ppCachedD3D11Resources);
                        VERIFY_EXPR(BindPoint < m_pResourceCache->GetSRVCount());
                        auto& CachedResource = pCachedResources[BindPoint];
                        if (CachedResource.pView)
                        {
                            auto* pTexView = CachedResource.pView.RawPtr<ITextureView>();
                            auto* pSampler = pTexView->GetSampler();
                            if (pSampler != nullptr && pSampler != CachedSampler.pSampler.RawPtr())
                            {
                                LOG_ERROR_MESSAGE( "All elements of texture array '", ts.Attribs.Name, "' in shader '", GetShaderName(), "' share the same sampler. However, the sampler set in view for element ", BindPoint - ts.Attribs.BindPoint, " does not match bound sampler. This may cause incorrect behavior on GL platform."  );
                            }
                        }
                    }
                }
            }
        },

        [&](const TexUAVBindInfo& uav)
        {
            for (Uint32 BindPoint = uav.Attribs.BindPoint; BindPoint < Uint32{uav.Attribs.BindPoint} + uav.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsUAVBound(BindPoint, true))
                {
                    LOG_MISSING_BINDING("texture UAV", uav.Attribs, BindPoint);
                    BindingsOK  = false;
                }
            }
        },

        [&](const BuffSRVBindInfo& buf)
        {
            for (Uint32 BindPoint = buf.Attribs.BindPoint; BindPoint < Uint32{buf.Attribs.BindPoint} + buf.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsSRVBound(BindPoint, false))
                {
                    LOG_MISSING_BINDING("buffer", buf.Attribs, BindPoint);
                    BindingsOK  = false;
                }
            }
        },

        [&](const BuffUAVBindInfo& uav)
        {
            for (Uint32 BindPoint = uav.Attribs.BindPoint; BindPoint < Uint32{uav.Attribs.BindPoint} + uav.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsUAVBound(BindPoint, false))
                {
                    LOG_MISSING_BINDING("buffer UAV", uav.Attribs, BindPoint);
                    BindingsOK  = false;
                }
            }
        },

        [&](const SamplerBindInfo& sam)
        {
            for (Uint32 BindPoint = sam.Attribs.BindPoint; BindPoint < Uint32{sam.Attribs.BindPoint} + sam.Attribs.BindCount; ++BindPoint)
            {
                if (!m_pResourceCache->IsSamplerBound(BindPoint))
                {
                    LOG_MISSING_BINDING("sampler", sam.Attribs, BindPoint);
                    BindingsOK  = false;
                }
            }
        }
    );
#undef LOG_MISSING_BINDING

    return BindingsOK;
}

#endif
}
