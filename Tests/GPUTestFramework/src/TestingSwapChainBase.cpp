/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <memory>
#include <sstream>

#include "TestingSwapChainBase.hpp"
#include "GraphicsAccessories.hpp"
#include "FileSystem.hpp"

#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#include "../../../ThirdParty/stb/stb_image.h"
#ifdef __clang__
#    pragma clang diagnostic pop
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../ThirdParty/stb/stb_image_write.h"

namespace Diligent
{

namespace Testing
{

bool LoadTestImage(const char*         FilePath,
                   std::vector<Uint8>& Pixels,
                   Uint32&             Width,
                   Uint32&             Height)
{
    Pixels.clear();
    Width  = 0;
    Height = 0;

    if (FilePath == nullptr || FilePath[0] == '\0')
    {
        LOG_ERROR_MESSAGE("Reference image file path must not be null or empty");
        return false;
    }

    int ImageWidth      = 0;
    int ImageHeight     = 0;
    int ImageComponents = 0;

    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pImage{
        stbi_load(FilePath, &ImageWidth, &ImageHeight, &ImageComponents, STBI_rgb_alpha),
        &stbi_image_free};
    if (!pImage)
    {
        LOG_ERROR_MESSAGE("Failed to load reference image '", FilePath, "': ", stbi_failure_reason());
        return false;
    }

    if (ImageWidth <= 0 || ImageHeight <= 0)
    {
        LOG_ERROR_MESSAGE("Reference image '", FilePath, "' has invalid dimensions ", ImageWidth, 'x', ImageHeight);
        return false;
    }

    const size_t ImageWidthSize  = static_cast<size_t>(ImageWidth);
    const size_t ImageHeightSize = static_cast<size_t>(ImageHeight);
    if (ImageWidthSize > (std::numeric_limits<size_t>::max)() / 4 / ImageHeightSize)
    {
        LOG_ERROR_MESSAGE("Reference image '", FilePath, "' is too large");
        return false;
    }

    const size_t DataSize = ImageWidthSize * ImageHeightSize * 4;
    Pixels.assign(pImage.get(), pImage.get() + DataSize);
    Width  = static_cast<Uint32>(ImageWidth);
    Height = static_cast<Uint32>(ImageHeight);
    return true;
}

std::string GetTestImageComparisonFailureFileName(Uint32 FailureIndex)
{
    const auto* const            TestInfo     = ::testing::UnitTest::GetInstance()->current_test_info();
    GPUTestingEnvironment* const pEnvironment = GPUTestingEnvironment::GetInstance();
    VERIFY_EXPR(TestInfo != nullptr);
    VERIFY_EXPR(pEnvironment != nullptr && pEnvironment->GetDevice() != nullptr);

    const auto ValidateName = [](const std::string& src) {
        std::string dst = src;
        for (char& c : dst)
        {
            if (c == '.' || c == '\\' || c == '/')
                c = '_';
        }
        return dst;
    };

    std::string FileName{ValidateName(TestInfo->test_suite_name())};
    FileName += '.';
    FileName += ValidateName(TestInfo->name());
    FileName += '_';
    FileName += GetRenderDeviceTypeShortString(pEnvironment->GetDevice()->GetDeviceInfo().Type);
    FileName += "_FAIL";
    if (FailureIndex > 0)
        FileName += std::to_string(FailureIndex);
    FileName += "_.png";
    return FileName;
}

void CompareTestImages(const Uint8*                          pReferencePixels,
                       Uint64                                RefPixelsStride,
                       const Uint8*                          pPixels,
                       Uint64                                PixelsStride,
                       Uint32                                Width,
                       Uint32                                Height,
                       TEXTURE_FORMAT                        Format,
                       std::unordered_map<std::string, int>& FailureCounters,
                       const TestImageComparisonAttribs&     ComparisonAttribs,
                       bool                                  CompareAlpha)
{
    VERIFY_EXPR(pReferencePixels != nullptr);
    VERIFY_EXPR(pPixels != nullptr);
    VERIFY_EXPR(Width != 0);
    VERIFY_EXPR(Height != 0);
    VERIFY_EXPR(PixelsStride != 0);
    VERIFY_EXPR(RefPixelsStride != 0);
    VERIFY(Format == TEX_FORMAT_RGBA8_UNORM, GetTextureFormatAttribs(Format).Name, " is not supported");
    VERIFY_EXPR(ComparisonAttribs.MaxBadPixelRatio >= 0 && ComparisonAttribs.MaxBadPixelRatio <= 1);

    bool bIsIdentical = true;

    const Uint32 ComponentCount = CompareAlpha ? 4u : 3u;
    if (CompareAlpha)
    {
        for (Uint32 Row = 0; Row < Height; ++Row)
        {
            if (memcmp(pReferencePixels + Row * RefPixelsStride,
                       pPixels + Row * PixelsStride,
                       Width * 4) != 0)
            {
                bIsIdentical = false;
                break;
            }
        }
    }
    else
    {
        for (Uint32 Row = 0; Row < Height && bIsIdentical; ++Row)
        {
            for (Uint32 Col = 0; Col < Width; ++Col)
            {
                if (memcmp(pReferencePixels + Row * RefPixelsStride + Col * 4,
                           pPixels + Row * PixelsStride + Col * 4,
                           ComponentCount) != 0)
                {
                    bIsIdentical = false;
                    break;
                }
            }
        }
    }

    if (bIsIdentical)
        return;

    Uint64 ToleratedPixelCount      = 0;
    Uint64 BadPixelCount            = 0;
    Uint32 MaxToleratedChannelError = 0;
    Uint32 MaxBadChannelError       = 0;
    for (Uint32 Row = 0; Row < Height; ++Row)
    {
        for (Uint32 Col = 0; Col < Width; ++Col)
        {
            Uint32 PixelMaxChannelError = 0;
            for (Uint32 Component = 0; Component < ComponentCount; ++Component)
            {
                const Uint32 RefValue = pReferencePixels[Row * RefPixelsStride + Col * 4 + Component];
                const Uint32 Value    = pPixels[Row * PixelsStride + Col * 4 + Component];
                const Uint32 Error    = RefValue > Value ? RefValue - Value : Value - RefValue;
                PixelMaxChannelError  = std::max(PixelMaxChannelError, Error);
            }

            if (PixelMaxChannelError > ComparisonAttribs.MaxChannelError)
            {
                ++BadPixelCount;
                MaxBadChannelError = std::max(MaxBadChannelError, PixelMaxChannelError);
            }
            else if (PixelMaxChannelError > 0)
            {
                ++ToleratedPixelCount;
                MaxToleratedChannelError = std::max(MaxToleratedChannelError, PixelMaxChannelError);
            }
        }
    }

    if (ToleratedPixelCount == 0 && BadPixelCount == 0)
        return;

    const Uint64 PixelCount = Uint64{Width} * Height;

    std::ostringstream Statistics;
    if (ToleratedPixelCount > 0)
    {
        Statistics << ToleratedPixelCount << " of " << PixelCount
                   << " pixels differ but remain within the per-channel error threshold "
                   << Uint32{ComparisonAttribs.MaxChannelError} << "; maximum channel error is "
                   << MaxToleratedChannelError;
    }
    if (BadPixelCount > 0)
    {
        if (ToleratedPixelCount > 0)
            Statistics << '\n';

        const double BadPixelPercentage = static_cast<double>(BadPixelCount) / static_cast<double>(PixelCount) * 100.0;
        Statistics << BadPixelCount << " of " << PixelCount << " pixels (" << BadPixelPercentage
                   << "%) exceed the threshold; maximum channel error is " << MaxBadChannelError;
    }

    if (static_cast<double>(BadPixelCount) <=
        static_cast<double>(PixelCount) * ComparisonAttribs.MaxBadPixelRatio)
    {
        LOG_WARNING_MESSAGE("Image rendered by the test differs from the reference image, but is within the configured tolerance:\n",
                            Statistics.str());
        return;
    }

    {
        auto ReportImageStride = (Width * 2) * 3;

        std::vector<Uint8> ReportImage(ReportImageStride * (Height * 2));
        for (Uint32 row = 0; row < Height; ++row)
        {
            for (Uint32 col = 0; col < Width; ++col)
            {
                for (Uint32 c = 0; c < 3; ++c)
                {
                    auto RefVal = pReferencePixels[row * RefPixelsStride + col * 4 + c];
                    auto Val    = pPixels[row * PixelsStride + col * 4 + c];
                    auto diff   = static_cast<Uint8>(std::min(std::abs(int{RefVal} - int{Val}), 255));

                    // clang-format off
                    ReportImage[row            * ReportImageStride +  col * 3          + c] = RefVal;
                    ReportImage[row            * ReportImageStride + (Width + col) * 3 + c] = Val;
                    ReportImage[(row + Height) * ReportImageStride +  col * 3          + c] = diff;
                    ReportImage[(row + Height) * ReportImageStride + (Width + col) * 3 + c] = static_cast<Uint8>(std::min(diff*16, 255));
                    // clang-format on
                }
            }
        }
        const std::string CounterKey     = GetTestImageComparisonFailureFileName();
        auto&             FailureCounter = FailureCounters[CounterKey];
        const std::string FileName       = GetTestImageComparisonFailureFileName(static_cast<Uint32>(FailureCounter));
        if (stbi_write_png(FileName.c_str(), Width * 2, Height * 2, 3, ReportImage.data(), (Width * 2) * 3) == 0)
        {
            LOG_ERROR_MESSAGE("Failed to write ", FileName);
        }
        ADD_FAILURE() << "Image rendered by the test differs from the reference image:\n"
                      << Statistics.str();
        ++FailureCounter;
    }
}

void DumpTestImage(const Uint8*   pPixels,
                   Uint64         PixelsStride,
                   Uint32         Width,
                   Uint32         Height,
                   TEXTURE_FORMAT Format,
                   const char*    DumpName,
                   bool           bIsOpenGL,
                   bool           KeepAlpha)
{

    VERIFY_EXPR(pPixels != nullptr);
    VERIFY_EXPR(Width != 0);
    VERIFY_EXPR(Height != 0);
    VERIFY_EXPR(PixelsStride != 0);
    VERIFY(Format == TEX_FORMAT_RGBA8_UNORM, GetTextureFormatAttribs(Format).Name, " is not supported");

    const Uint32       ComponentCount  = KeepAlpha ? 4u : 3u;
    const auto         DumpImageStride = Width * ComponentCount;
    std::vector<Uint8> DumpImage(DumpImageStride * Height);
    for (Uint32 y = 0; y < Height; ++y)
    {
        for (Uint32 x = 0; x < Width; ++x)
        {
            for (Uint32 c = 0; c < ComponentCount; ++c)
            {
                const Uint32 FlipCoord                                  = bIsOpenGL ? (Height - 1 - y) : y;
                DumpImage[y * DumpImageStride + x * ComponentCount + c] = pPixels[FlipCoord * PixelsStride + x * 4 + c];
            }
        }
    }

    const String FileName = String{DumpName} + ".png";
    if (stbi_write_png(FileName.c_str(), Width, Height, static_cast<int>(ComponentCount), DumpImage.data(), static_cast<int>(DumpImageStride)) == 0)
    {
        LOG_ERROR_MESSAGE("Failed to write ", FileName);
    }
}

} // namespace Testing

} // namespace Diligent
