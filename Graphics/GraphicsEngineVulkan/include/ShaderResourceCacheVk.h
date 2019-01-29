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
/// Declaration of Diligent::ShaderResourceCacheVk class

// Shader resource cache stores Vk resources in a continuous chunk of memory:
//   
//                                 |Vulkan Descriptor Set|
//                                           A    ___________________________________________________________
//  m_pMemory                                |   |              m_pResources, m_NumResources == m            |
//  |               m_DescriptorSetAllocation|   |                                                           |
//  V                                        |   |                                                           V
//  |  DescriptorSet[0]  |   ....    |  DescriptorSet[Ns-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
//         |    |                                                A \
//         |    |                                                |  \
//         |    |________________________________________________|   \RefCntAutoPtr
//         |             m_pResources, m_NumResources == n            \_________     
//         |                                                          |  Object |
//         | m_DescriptorSetAllocation                                 --------- 
//         V
//  |Vulkan Descriptor Set|
//                                                                    
//  Ns = m_NumSets
//
//
// Descriptor set for static and mutable resources is assigned during cache initialization
// Descriptor set for dynamic resources is assigned at every draw call

#include "stl/utility.h"
#include "stl/vector.h"

#include "DescriptorPoolManager.h"
#include "SPIRVShaderResources.h"

namespace Diligent
{

class DeviceContextVkImpl;

// sizeof(ShaderResourceCacheVk) == 24 (x64, msvc, Release)
class ShaderResourceCacheVk
{
public:
    // This enum is used for debug purposes only
    enum DbgCacheContentType
    {
        StaticShaderResources,
        SRBResources
    };

    ShaderResourceCacheVk(DbgCacheContentType dbgContentType)
#ifdef _DEBUG
        : m_DbgContentType(dbgContentType)
#endif
    {
    }

    ShaderResourceCacheVk             (const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk             (ShaderResourceCacheVk&&)      = delete;
    ShaderResourceCacheVk& operator = (const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk& operator = (ShaderResourceCacheVk&&)      = delete;

    ~ShaderResourceCacheVk();

    static size_t GetRequiredMemorySize(Uint32 NumSets, Uint32 SetSizes[]);
    void InitializeSets(IMemoryAllocator &MemAllocator, Uint32 NumSets, Uint32 SetSizes[]);
    void InitializeResources(Uint32 Set, Uint32 Offset, Uint32 ArraySize, SPIRVShaderResourceAttribs::ResourceType Type);

    // sizeof(Resource) == 16 (x64, msvc, Release)
    struct Resource
    {
        Resource(SPIRVShaderResourceAttribs::ResourceType _Type) :
            Type(_Type)
        {}

        Resource(const Resource&)              = delete;
        Resource(Resource&&)                   = delete;
        Resource& operator = (const Resource&) = delete;
        Resource& operator = (Resource&&)      = delete;

        const SPIRVShaderResourceAttribs::ResourceType  Type;
        RefCntAutoPtr<IDeviceObject>                    pObject;

        VkDescriptorBufferInfo GetUniformBufferDescriptorWriteInfo ()                const;
        VkDescriptorBufferInfo GetStorageBufferDescriptorWriteInfo ()                const;
        VkDescriptorImageInfo  GetImageDescriptorWriteInfo  (bool IsImmutableSampler)const;
        VkBufferView           GetBufferViewWriteInfo       ()                       const;
        VkDescriptorImageInfo  GetSamplerDescriptorWriteInfo()                       const;
    };

    // sizeof(DescriptorSet) == 40 (x64, msvc, Release)
    class DescriptorSet
    {
    public:
        DescriptorSet(Uint32 NumResources, Resource *pResources) :
            m_NumResources  (NumResources),
            m_pResources    (pResources)
        {}

        DescriptorSet(const DescriptorSet&)              = delete;
        DescriptorSet(DescriptorSet&&)                   = delete;
        DescriptorSet& operator = (const DescriptorSet&) = delete;
        DescriptorSet& operator = (DescriptorSet&&)      = delete;

        inline Resource& GetResource(Uint32 CacheOffset)
        {
            VERIFY(CacheOffset < m_NumResources, "Offset ", CacheOffset, " is out of range" );
            return m_pResources[CacheOffset];
        }
        inline const Resource& GetResource(Uint32 CacheOffset)const
        {
            VERIFY(CacheOffset < m_NumResources, "Offset ", CacheOffset, " is out of range" );
            return m_pResources[CacheOffset];
        }

        inline Uint32 GetSize()const{return m_NumResources; }

        VkDescriptorSet GetVkDescriptorSet()const
        {
            return m_DescriptorSetAllocation.GetVkDescriptorSet();
        }

        void AssignDescriptorSetAllocation(DescriptorSetAllocation&& Allocation)
        {
            VERIFY(m_NumResources > 0, "Descriptor set is empty");
            m_DescriptorSetAllocation = move(Allocation);
        }

        const Uint32 m_NumResources = 0;

    private:
        Resource* const m_pResources = nullptr;
        DescriptorSetAllocation m_DescriptorSetAllocation;
    };

    inline DescriptorSet& GetDescriptorSet(Uint32 Index)
    {
        VERIFY_EXPR(Index < m_NumSets);
        return reinterpret_cast<DescriptorSet*>(m_pMemory)[Index];
    }
    inline const DescriptorSet& GetDescriptorSet(Uint32 Index)const
    {
        VERIFY_EXPR(Index < m_NumSets);
        return reinterpret_cast<const DescriptorSet*>(m_pMemory)[Index];
    }

    inline Uint32 GetNumDescriptorSets()const{return m_NumSets; }

#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    DbgCacheContentType DbgGetContentType()const{return m_DbgContentType;}
#endif

    template<bool VerifyOnly>
    void TransitionResources(DeviceContextVkImpl *pCtxVkImpl);

    Uint32 GetDynamicBufferOffsets(DeviceContextVkImpl *pCtxVkImpl, vector<uint32_t>& Offsets)const;

private:

    Resource* GetFirstResourcePtr()
    {
        return reinterpret_cast<Resource*>(reinterpret_cast<DescriptorSet*>(m_pMemory) + m_NumSets);
    }

    IMemoryAllocator*   m_pAllocator     = nullptr; 
    void*               m_pMemory        = nullptr;
    Uint32              m_NumSets        = 0;
    Uint32              m_TotalResources = 0;

#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    const DbgCacheContentType m_DbgContentType;
#endif
};

}
