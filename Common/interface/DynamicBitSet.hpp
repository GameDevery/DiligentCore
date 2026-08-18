/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "BasicMath.hpp"
#include "PlatformMisc.hpp"

namespace Diligent
{

/// Runtime-sized set of bits.
///
/// The initial number of bits is specified at construction and may subsequently
/// be changed with Resize(). All bits are initially reset, as are bits added by
/// Resize(). The class is not thread-safe; concurrent access requires external
/// synchronization if any thread may modify the set. Copies are independent;
/// moving leaves the source as a valid empty set. Self-move assignment is a no-op.
class DynamicBitSet
{
public:
    /// Creates a set containing BitCount reset bits.
    ///
    /// \param [in] BitCount - Number of addressable bits in the set. May be zero.
    explicit DynamicBitSet(size_t BitCount) :
        m_BitCount{BitCount},
        m_Words(GetWordCount(BitCount), 0)
    {}

    /// Creates an independent copy of Other.
    DynamicBitSet(const DynamicBitSet& Other) = default;

    /// Moves the contents of Other and leaves it as an empty set.
    DynamicBitSet(DynamicBitSet&& Other) noexcept :
        m_BitCount{std::exchange(Other.m_BitCount, 0)},
        m_Words{std::move(Other.m_Words)}
    {
        Other.m_Words.clear();
    }

    /// Replaces this set with an independent copy of Other.
    DynamicBitSet& operator=(const DynamicBitSet& Other)
    {
        if (this != &Other)
        {
            DynamicBitSet Copy{Other};
            Swap(Copy);
        }
        return *this;
    }

    /// Replaces this set with the contents of Other and leaves it empty.
    /// Self-move assignment is a no-op.
    DynamicBitSet& operator=(DynamicBitSet&& Other) noexcept
    {
        if (this != &Other)
        {
            m_BitCount = std::exchange(Other.m_BitCount, 0);
            m_Words    = std::move(Other.m_Words);
            Other.m_Words.clear();
        }
        return *this;
    }

    /// Returns the number of addressable bits in the set.
    size_t GetSize() const noexcept
    {
        return m_BitCount;
    }

    /// Changes the number of addressable bits in the set.
    ///
    /// Existing bits in the retained range preserve their values. Newly added
    /// bits are reset, and bits removed by shrinking are discarded.
    ///
    /// \param [in] BitCount - New number of addressable bits. May be zero.
    void Resize(size_t BitCount)
    {
        if (BitCount == m_BitCount)
            return;

        m_Words.resize(GetWordCount(BitCount), Uint64{0});
        m_BitCount = BitCount;
        ClearUnusedBits();
    }

    /// Tests whether the specified bit is set.
    ///
    /// \param [in] BitIndex - Bit index in the range [0, GetSize()).
    /// \return                  True if the bit is set and false otherwise.
    bool Test(size_t BitIndex) const noexcept
    {
        VERIFY_EXPR(BitIndex < m_BitCount);
        return (m_Words[GetWordIndex(BitIndex)] & GetBit(BitIndex)) != 0;
    }

    /// Assigns a value to the specified bit.
    ///
    /// \param [in] BitIndex - Bit index in the range [0, GetSize()).
    /// \param [in] Value    - New bit value. The default value is true.
    void Set(size_t BitIndex, bool Value = true) noexcept
    {
        VERIFY_EXPR(BitIndex < m_BitCount);

        Uint64&      Word = m_Words[GetWordIndex(BitIndex)];
        const Uint64 Bit  = GetBit(BitIndex);
        if (Value)
            Word |= Bit;
        else
            Word &= ~Bit;
    }

    /// Resets the specified bit.
    ///
    /// \param [in] BitIndex - Bit index in the range [0, GetSize()).
    void Reset(size_t BitIndex) noexcept
    {
        Set(BitIndex, false);
    }

    /// Resets every bit in the set.
    void ResetAll() noexcept
    {
        std::fill(m_Words.begin(), m_Words.end(), Uint64{0});
    }

    /// Invokes Handler for every set bit in ascending index order.
    ///
    /// Handler must be callable with one size_t argument containing the bit index.
    /// Modifying this bit set from Handler is not supported.
    ///
    /// \param [in] Handler - Function object invoked once for every set bit.
    template <typename HandlerType>
    void ForEachSetBit(HandlerType&& Handler) const
    {
        for (size_t WordIndex = 0; WordIndex < m_Words.size(); ++WordIndex)
        {
            Uint64 Bits = m_Words[WordIndex];
            while (Bits != 0)
            {
                const Uint64 Bit      = ExtractLSB(Bits);
                const size_t BitIndex = WordIndex * WordSize + PlatformMisc::GetLSB(Bit);
                VERIFY_EXPR(BitIndex < m_BitCount);
                Handler(BitIndex);
            }
        }
    }

private:
    static constexpr size_t WordSize = sizeof(Uint64) * 8;

    static size_t GetWordCount(size_t BitCount) noexcept
    {
        return BitCount == 0 ? 0 : (BitCount - 1) / WordSize + 1;
    }

    static size_t GetWordIndex(size_t BitIndex) noexcept
    {
        return BitIndex / WordSize;
    }

    static Uint64 GetBit(size_t BitIndex) noexcept
    {
        return Uint64{1} << (BitIndex % WordSize);
    }

    void ClearUnusedBits() noexcept
    {
        const size_t UsedBitCount = m_BitCount % WordSize;
        if (UsedBitCount != 0)
            m_Words.back() &= (Uint64{1} << UsedBitCount) - 1;
    }

    void Swap(DynamicBitSet& Other) noexcept
    {
        std::swap(m_BitCount, Other.m_BitCount);
        m_Words.swap(Other.m_Words);
    }

private:
    size_t              m_BitCount = 0;
    std::vector<Uint64> m_Words;
};

} // namespace Diligent
