/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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
/// Defines Diligent::DefaultRawMemoryAllocator class

#include "../../Primitives/interface/MemoryAllocator.h"

namespace Diligent
{

/// Default raw memory allocator
class DefaultRawMemoryAllocator : public IMemoryAllocator
{
public:
    DefaultRawMemoryAllocator();

    /// Allocates block of memory
    virtual void* Allocate(size_t Size, const Char* dbgDescription, const char* dbgFileName, const Int32 dbgLineNumber) override;

    /// Releases memory
    virtual void Free(void* Ptr) override;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment, const Char* dbgDescription, const char* dbgFileName, const Int32 dbgLineNumber) override;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) override;

    static DefaultRawMemoryAllocator& GetAllocator();

private:
    DefaultRawMemoryAllocator(const DefaultRawMemoryAllocator&) = delete;
    DefaultRawMemoryAllocator(DefaultRawMemoryAllocator&&)      = delete;
    DefaultRawMemoryAllocator& operator=(const DefaultRawMemoryAllocator&) = delete;
    DefaultRawMemoryAllocator& operator=(DefaultRawMemoryAllocator&&) = delete;
};

} // namespace Diligent
