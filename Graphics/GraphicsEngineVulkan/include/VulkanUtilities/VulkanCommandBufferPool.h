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

#include <memory>
#include <mutex>
#include <atomic>
#include "stl/deque.h"
#include "vulkan.h"
#include "VulkanLogicalDevice.h"
#include "VulkanObjectWrappers.h"

namespace VulkanUtilities
{
    class VulkanCommandBufferPool
    {
    public:
        VulkanCommandBufferPool(std::shared_ptr<const VulkanLogicalDevice> LogicalDevice, 
                                uint32_t                                   queueFamilyIndex, 
                                VkCommandPoolCreateFlags                   flags);

        VulkanCommandBufferPool             (const VulkanCommandBufferPool&)  = delete;
        VulkanCommandBufferPool             (      VulkanCommandBufferPool&&) = delete;
        VulkanCommandBufferPool& operator = (const VulkanCommandBufferPool&)  = delete;
        VulkanCommandBufferPool& operator = (      VulkanCommandBufferPool&&) = delete;

        ~VulkanCommandBufferPool();

        VkCommandBuffer GetCommandBuffer(const char* DebugName = "");
        // The GPU must have finished with the command buffer being returned to the pool
        void FreeCommandBuffer(VkCommandBuffer&& CmdBuffer);

        CommandPoolWrapper&& Release();

#ifdef DEVELOPMENT
        int32_t DvpGetBufferCounter()const{return m_BuffCounter;}
#endif

    private:
        // Shared point to logical device must be defined before the command pool
        std::shared_ptr<const VulkanLogicalDevice> m_LogicalDevice;
        CommandPoolWrapper m_CmdPool;

        std::mutex m_Mutex;
        Diligent::deque< VkCommandBuffer > m_CmdBuffers;
#ifdef DEVELOPMENT
        std::atomic_int32_t m_BuffCounter;
#endif
    };
}
