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

#include "DynamicBitSet.hpp"
#include "gtest/gtest.h"

#include <iterator>
#include <utility>
#include <vector>

using namespace Diligent;

namespace
{

void CopyAssign(DynamicBitSet& Dst, const DynamicBitSet& Src)
{
    Dst = Src;
}

void MoveAssign(DynamicBitSet& Dst, DynamicBitSet&& Src)
{
    Dst = std::move(Src);
}

} // namespace

TEST(Common_DynamicBitSet, EmptySet)
{
    DynamicBitSet Bits{0};
    EXPECT_EQ(Bits.GetSize(), 0u);

    bool HandlerCalled = false;
    Bits.ForEachSetBit([&](size_t) {
        HandlerCalled = true;
    });
    EXPECT_FALSE(HandlerCalled);

    Bits.ResetAll();
}

TEST(Common_DynamicBitSet, SetsAndResetsBitsAcrossWords)
{
    static constexpr size_t BitCount        = 130;
    static constexpr size_t SetBitIndices[] = {0, 1, 63, 64, 65, 127, 128, 129};

    DynamicBitSet Bits{BitCount};
    EXPECT_EQ(Bits.GetSize(), BitCount);
    for (size_t BitIndex = 0; BitIndex < BitCount; ++BitIndex)
        EXPECT_FALSE(Bits.Test(BitIndex));

    for (const size_t BitIndex : SetBitIndices)
        Bits.Set(BitIndex);

    std::vector<size_t> VisitedBits;
    Bits.ForEachSetBit([&](size_t BitIndex) {
        VisitedBits.push_back(BitIndex);
    });
    EXPECT_EQ(VisitedBits, std::vector<size_t>(std::begin(SetBitIndices), std::end(SetBitIndices)));

    Bits.Reset(64);
    Bits.Set(128, false);
    EXPECT_FALSE(Bits.Test(64));
    EXPECT_FALSE(Bits.Test(128));
    EXPECT_TRUE(Bits.Test(63));
    EXPECT_TRUE(Bits.Test(65));
    EXPECT_TRUE(Bits.Test(129));

    Bits.ResetAll();
    for (size_t BitIndex = 0; BitIndex < BitCount; ++BitIndex)
        EXPECT_FALSE(Bits.Test(BitIndex));
}

TEST(Common_DynamicBitSet, ResizeGrowthPreservesExistingBitsAndResetsNewBits)
{
    DynamicBitSet Bits{63};
    Bits.Set(0);
    Bits.Set(62);

    Bits.Resize(65);
    EXPECT_EQ(Bits.GetSize(), 65u);
    EXPECT_TRUE(Bits.Test(0));
    EXPECT_TRUE(Bits.Test(62));
    EXPECT_FALSE(Bits.Test(63));
    EXPECT_FALSE(Bits.Test(64));

    Bits.Resize(130);
    EXPECT_TRUE(Bits.Test(0));
    EXPECT_TRUE(Bits.Test(62));
    for (size_t BitIndex = 63; BitIndex < Bits.GetSize(); ++BitIndex)
        EXPECT_FALSE(Bits.Test(BitIndex));
}

TEST(Common_DynamicBitSet, ResizeShrinkDiscardsRemovedBits)
{
    DynamicBitSet Bits{130};
    Bits.Set(0);
    Bits.Set(62);
    Bits.Set(64);
    Bits.Set(129);

    Bits.Resize(65);
    EXPECT_TRUE(Bits.Test(0));
    EXPECT_TRUE(Bits.Test(62));
    EXPECT_TRUE(Bits.Test(64));

    Bits.Resize(64);
    Bits.Resize(65);
    EXPECT_FALSE(Bits.Test(64));

    Bits.Resize(2);
    EXPECT_TRUE(Bits.Test(0));
    EXPECT_FALSE(Bits.Test(1));

    Bits.Resize(5);
    for (size_t BitIndex = 1; BitIndex < Bits.GetSize(); ++BitIndex)
        EXPECT_FALSE(Bits.Test(BitIndex));

    Bits.Resize(0);
    EXPECT_EQ(Bits.GetSize(), 0u);

    Bits.Resize(1);
    EXPECT_FALSE(Bits.Test(0));
}

TEST(Common_DynamicBitSet, ResizeShrinkClearsUnusedBitsInRetainedWord)
{
    DynamicBitSet Bits{64};
    Bits.Set(1);
    Bits.Set(63);

    Bits.Resize(2);
    EXPECT_TRUE(Bits.Test(1));

    Bits.Resize(64);
    EXPECT_TRUE(Bits.Test(1));
    EXPECT_FALSE(Bits.Test(63));
}

TEST(Common_DynamicBitSet, CopyOperationsCreateIndependentSets)
{
    DynamicBitSet Source{65};
    Source.Set(0);
    Source.Set(64);

    DynamicBitSet CopyConstructed{Source};
    EXPECT_EQ(CopyConstructed.GetSize(), 65u);
    EXPECT_TRUE(CopyConstructed.Test(0));
    EXPECT_TRUE(CopyConstructed.Test(64));

    CopyConstructed.Reset(0);
    EXPECT_TRUE(Source.Test(0));

    DynamicBitSet CopyAssigned{2};
    CopyAssigned.Set(1);
    CopyAssigned = Source;
    EXPECT_EQ(CopyAssigned.GetSize(), 65u);
    EXPECT_TRUE(CopyAssigned.Test(0));
    EXPECT_TRUE(CopyAssigned.Test(64));

    CopyAssigned.Reset(64);
    EXPECT_TRUE(Source.Test(64));

    CopyAssign(CopyAssigned, CopyAssigned);
    EXPECT_EQ(CopyAssigned.GetSize(), 65u);
    EXPECT_TRUE(CopyAssigned.Test(0));
    EXPECT_FALSE(CopyAssigned.Test(64));
}

TEST(Common_DynamicBitSet, MoveOperationsLeaveSourceEmptyAndReusable)
{
    DynamicBitSet Source{65};
    Source.Set(0);
    Source.Set(64);

    DynamicBitSet MoveConstructed{std::move(Source)};
    EXPECT_EQ(MoveConstructed.GetSize(), 65u);
    EXPECT_TRUE(MoveConstructed.Test(0));
    EXPECT_TRUE(MoveConstructed.Test(64));
    EXPECT_EQ(Source.GetSize(), 0u);

    Source.Resize(1);
    Source.Set(0);
    EXPECT_TRUE(Source.Test(0));

    DynamicBitSet MoveAssigned{2};
    MoveAssigned.Set(1);
    MoveAssigned = std::move(MoveConstructed);
    EXPECT_EQ(MoveAssigned.GetSize(), 65u);
    EXPECT_TRUE(MoveAssigned.Test(0));
    EXPECT_TRUE(MoveAssigned.Test(64));
    EXPECT_EQ(MoveConstructed.GetSize(), 0u);

    MoveConstructed.Resize(1);
    EXPECT_FALSE(MoveConstructed.Test(0));

    MoveAssign(MoveAssigned, std::move(MoveAssigned));
    EXPECT_EQ(MoveAssigned.GetSize(), 65u);
    EXPECT_TRUE(MoveAssigned.Test(0));
    EXPECT_TRUE(MoveAssigned.Test(64));
}
