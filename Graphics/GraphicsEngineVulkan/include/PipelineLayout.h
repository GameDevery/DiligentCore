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
/// Declaration of Diligent::PipelineLayout class
#include "stl/array.h"
#include "stl/vector.h"

#include "ShaderBase.h"
#include "ShaderResourceLayoutVk.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"

namespace Diligent
{

class RenderDeviceVkImpl;
class DeviceContextVkImpl;
class ShaderResourceCacheVk;

/// Implementation of the Diligent::PipelineLayout class
class PipelineLayout
{
public:
    static VkDescriptorType GetVkDescriptorType(const SPIRVShaderResourceAttribs& Res);

    PipelineLayout();
    void Release(RenderDeviceVkImpl* pDeviceVkImpl, Uint64 CommandQueueMask);
    void Finalize(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice);

    VkPipelineLayout GetVkPipelineLayout()const{return m_LayoutMgr.GetVkPipelineLayout();}
    stl::array<Uint32, 2> GetDescriptorSetSizes(Uint32& NumSets)const;
    void InitResourceCache(RenderDeviceVkImpl*    pDeviceVkImpl,
                           ShaderResourceCacheVk& ResourceCache,
                           IMemoryAllocator&      CacheMemAllocator,
                           const char*            DbgPipelineName)const;

    void AllocateResourceSlot(const SPIRVShaderResourceAttribs& ResAttribs, 
                              VkSampler                         vkStaticSampler,
                              SHADER_TYPE                       ShaderType, 
                              Uint32&                           DescriptorSet, 
                              Uint32&                           Binding,
                              Uint32&                           OffsetInCache,
                              stl::vector<uint32_t>&            SPIRV);

    Uint32 GetTotalDescriptors(SHADER_VARIABLE_TYPE VarType)const
    {
        VERIFY_EXPR(VarType >= 0 && VarType < SHADER_VARIABLE_TYPE_NUM_TYPES);
        return m_LayoutMgr.GetDescriptorSet(VarType).TotalDescriptors;
    }

    bool IsSameAs(const PipelineLayout& RS)const
    {
        return m_LayoutMgr == RS.m_LayoutMgr;
    }
    size_t GetHash()const
    {
        return m_LayoutMgr.GetHash();
    }

    VkDescriptorSetLayout GetDynamicDescriptorSetVkLayout()const
    {
        return m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_DYNAMIC).VkLayout;
    }

    struct DescriptorSetBindInfo
    {
        stl::vector<VkDescriptorSet> vkSets;
        stl::vector<uint32_t>        DynamicOffsets;
        const ShaderResourceCacheVk* pResourceCache     = nullptr;
        VkPipelineBindPoint          BindPoint          = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        Uint32                       SetCout            = 0;
        Uint32                       DynamicOffsetCount = 0;
#ifdef _DEBUG
        const PipelineLayout*        pDbgPipelineLayout = nullptr;
#endif
        DescriptorSetBindInfo() : 
            vkSets(2),
            DynamicOffsets(64)
        {
        }

        void Reset()
        {
            SetCout            = 0;
            DynamicOffsetCount = 0;
#ifdef _DEBUG
            // In release mode, do not clear vectors as this causes unnecessary work
            vkSets.clear();
            DynamicOffsets.clear();
#endif
            pResourceCache = nullptr;
            BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
#ifdef _DEBUG
            pDbgPipelineLayout = nullptr;
#endif
        }
    };

    // Prepares Vulkan descriptor sets for binding. Actual binding 
    // may not be possible until draw command time because dynamic offsets are 
    // set by the same Vulkan command. If there are no dynamic descriptors, this
    // function also binds descriptor sets rightaway.
    void PrepareDescriptorSets(DeviceContextVkImpl*          pCtxVkImpl,
                               bool                          IsCompute,
                               const ShaderResourceCacheVk&  ResourceCache,
                               DescriptorSetBindInfo&        BindInfo,
                               VkDescriptorSet               VkDynamicDescrSet)const;

    // Computes dynamic offsets and binds descriptor sets
    void BindDescriptorSetsWithDynamicOffsets(DeviceContextVkImpl*    pCtxVkImpl,
                                              DescriptorSetBindInfo&  BindInfo)const;

private:

    class DescriptorSetLayoutManager
    {
    public:
        struct DescriptorSetLayout
        {
            DescriptorSetLayout() noexcept {}
            DescriptorSetLayout             (DescriptorSetLayout&&)      = default;
            DescriptorSetLayout             (const DescriptorSetLayout&) = delete;
            DescriptorSetLayout& operator = (const DescriptorSetLayout&) = delete;
            DescriptorSetLayout& operator = (DescriptorSetLayout&&)      = delete;
            
            uint32_t                                    TotalDescriptors      = 0;
            int8_t                                      SetIndex              = -1;
            uint8_t                                     NumDynamicDescriptors = 0; // Total number of uniform and storage buffers, counting all array elements
            uint16_t                                    NumLayoutBindings     = 0;
            VkDescriptorSetLayoutBinding*               pBindings             = nullptr;
            VulkanUtilities::DescriptorSetLayoutWrapper VkLayout;
            
            ~DescriptorSetLayout();
            void AddBinding(const VkDescriptorSetLayoutBinding& Binding, IMemoryAllocator& MemAllocator);
            void Finalize(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice, IMemoryAllocator& MemAllocator, VkDescriptorSetLayoutBinding* pNewBindings);
            void Release(RenderDeviceVkImpl* pRenderDeviceVk, IMemoryAllocator& MemAllocator, Uint64 CommandQueueMask);

            bool operator == (const DescriptorSetLayout& rhs)const;
            bool operator != (const DescriptorSetLayout& rhs)const{return !(*this == rhs);}
            size_t GetHash()const;

        private:
            void ReserveMemory(Uint32 NumBindings, IMemoryAllocator &MemAllocator);
            static size_t GetMemorySize(Uint32 NumBindings);
        };

        DescriptorSetLayoutManager(IMemoryAllocator &MemAllocator);
        ~DescriptorSetLayoutManager();

        DescriptorSetLayoutManager            (const DescriptorSetLayoutManager&) = delete;
        DescriptorSetLayoutManager& operator= (const DescriptorSetLayoutManager&) = delete;
        DescriptorSetLayoutManager            (DescriptorSetLayoutManager&&)      = delete;
        DescriptorSetLayoutManager& operator= (DescriptorSetLayoutManager&&)      = delete;
        
        void Finalize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice);
        void Release(RenderDeviceVkImpl* pRenderDeviceVk, Uint64 CommandQueueMask);

              DescriptorSetLayout& GetDescriptorSet(SHADER_VARIABLE_TYPE VarType)      { return m_DescriptorSetLayouts[VarType == SHADER_VARIABLE_TYPE_DYNAMIC ? 1 : 0]; }
        const DescriptorSetLayout& GetDescriptorSet(SHADER_VARIABLE_TYPE VarType)const { return m_DescriptorSetLayouts[VarType == SHADER_VARIABLE_TYPE_DYNAMIC ? 1 : 0]; }

        bool operator == (const DescriptorSetLayoutManager& rhs)const;
        bool operator != (const DescriptorSetLayoutManager& rhs)const {return !(*this == rhs);}
        size_t GetHash()const;
        VkPipelineLayout GetVkPipelineLayout()const{return m_VkPipelineLayout;}

        void AllocateResourceSlot(const SPIRVShaderResourceAttribs& ResAttribs,
                                  VkSampler                         vkStaticSampler,
                                  SHADER_TYPE                       ShaderType,
                                  Uint32&                           DescriptorSet,
                                  Uint32&                           Binding, 
                                  Uint32&                           OffsetInCache);

    private:
        IMemoryAllocator &m_MemAllocator;
        VulkanUtilities::PipelineLayoutWrapper      m_VkPipelineLayout;
        stl::array<DescriptorSetLayout, 2>          m_DescriptorSetLayouts;
        stl::vector<VkDescriptorSetLayoutBinding, STDAllocatorRawMem<VkDescriptorSetLayoutBinding>> m_LayoutBindings;
        uint8_t m_ActiveSets = 0;
    };

    IMemoryAllocator&          m_MemAllocator;
    DescriptorSetLayoutManager m_LayoutMgr;
};

}
