/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
/// Defines Diligent::IQuery interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

// {70F2A88A-F8BE-4901-8F05-2F72FA695BA0}
static constexpr INTERFACE_ID IID_Query =
    {0x70f2a88a, 0xf8be, 0x4901, {0x8f, 0x5, 0x2f, 0x72, 0xfa, 0x69, 0x5b, 0xa0}};

/// Query type.
enum QUERY_TYPE
{
    /// Query type is undefined.
    QUERY_TYPE_UNDEFINED = 0,

    /// Gets the number of samples that passed the depth and stencil tests in between IDeviceContext::BeginQuery
    /// and IDeviceContext::EndQuery. IQuery::GetData fills a Diligent::QueryDataOcclusion struct.
    QUERY_TYPE_OCCLUSION,

    /// Acts like QUERY_TYPE_OCCLUSION except that it returns simply a binary true/false result: false indicates that no samples
    /// passed depth and stencil testing, true indicates that at least one sample passed depth and stencil testing.
    /// IQuery::GetData fills a Diligent::QueryDataBinaryOcclusion struct.
    QUERY_TYPE_BINARY_OCCLUSION,

    /// Gets the GPU timestamp corresponding to IDeviceContext::EndQuery call. Fot this query
    /// type IDeviceContext::BeginQuery is disabled. IQuery::GetData fills a Diligent::QueryDataTimestamp struct.
    QUERY_TYPE_TIMESTAMP,

    /// Gets pipeline statistics, such as the number of pixel shader invocations in between IDeviceContext::BeginQuery
    /// and IDeviceContext::EndQuery. IQuery::GetData will fills a Diligent::QueryDataPipelineStatistics struct.
    QUERY_TYPE_PIPELINE_STATISTICS,

    /// The number of query types in the enum
    QUERY_TYPE_NUM_TYPES
};

/// Occlusion query data.
/// This structure is filled by IQuery::GetData() for Diligent::QUERY_TYPE_OCCLUSION query type.
struct QueryDataOcclusion
{
    /// Query type - must be Diligent::QUERY_TYPE_OCCLUSION
    const QUERY_TYPE Type = QUERY_TYPE_OCCLUSION;

    /// The number of samples that passed the depth and stencil tests in between
    /// IDeviceContext::BeginQuery and IDeviceContext::EndQuery.
    Uint64 NumSamples = 0;
};

/// Binary occlusion query data.
/// This structure is filled by IQuery::GetData() for Diligent::QUERY_TYPE_BINARY_OCCLUSION query type.
struct QueryDataBinaryOcclusion
{
    /// Query type - must be Diligent::QUERY_TYPE_BINARY_OCCLUSION
    const QUERY_TYPE Type = QUERY_TYPE_BINARY_OCCLUSION;

    /// Indicates if at least one sample passed depth and stencil testing in between
    /// IDeviceContext::BeginQuery and IDeviceContext::EndQuery.
    bool AnySamplePassed = 0;
};

/// Timestamp query data.
/// This structure is filled by IQuery::GetData() for Diligent::QUERY_TYPE_TIMESTAMP query type.
struct QueryDataTimestamp
{
    /// Query type - must be Diligent::QUERY_TYPE_TIMESTAMP
    const QUERY_TYPE Type = QUERY_TYPE_TIMESTAMP;

    /// The value of a high-frequency counter.
    Uint64 Counter = 0;

    /// The counter frequency, in Hz (ticks/second). If there was an error
    /// while getting the timestamp, this value will be 0.
    Uint64 Frequency = 0;
};

/// Pipeline statistics query data.
/// This structure is filled by IQuery::GetData() for Diligent::QUERY_TYPE_PIPELINE_STATISTICS query type.
///
/// \warning  In OpenGL backend the only field that will be populated is ClippingInvocations.
struct QueryDataPipelineStatistics
{
    /// Query type - must be Diligent::QUERY_TYPE_PIPELINE_STATISTICS
    const QUERY_TYPE Type = QUERY_TYPE_PIPELINE_STATISTICS;

    /// Number of vertices processed by the input assembler stage.
    Uint64 InputVertices = 0;

    /// Number of primitives processed by the input assembler stage.
    Uint64 InputPrimitives = 0;

    /// Number of primitives output by a geometry shader.
    Uint64 GSPrimitives = 0;

    /// Number of primitives that were sent to the clipping stage.
    Uint64 ClippingInvocations = 0;

    /// Number of primitives that were output by the clipping stage and were rendered.
    /// This may be larger or smaller than ClippingInvocations because after a primitive is
    /// clipped sometimes it is either broken up into more than one primitive or completely culled.
    Uint64 ClippingPrimitives = 0;

    /// Number of times a vertex shader was invoked.
    Uint64 VSInvocations = 0;

    /// Number of times a geometry shader was invoked.
    Uint64 GSInvocations = 0;

    /// Number of times a pixel shader shader was invoked.
    Uint64 PSInvocations = 0;

    /// Number of times a hull shader shader was invoked.
    Uint64 HSInvocations = 0;

    /// Number of times a domain shader shader was invoked.
    Uint64 DSInvocations = 0;

    /// Number of times a compute shader was invoked.
    Uint64 CSInvocations = 0;
};

/// Query description.
struct QueryDesc : DeviceObjectAttribs
{
    /// Query type, see Diligent::QUERY_TYPE.
    QUERY_TYPE Type = QUERY_TYPE_UNDEFINED;
};


/// Query interface.

/// Defines the methods to manipulate a Query object
class IQuery : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details.
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override = 0;

    /// Returns the Query description used to create the object.
    virtual const QueryDesc& GetDesc() const override = 0;

    /// Gets the query data.

    /// \param [in] pData    - pointer to the query data structure. Depending on the type of the query,
    ///                        this must be the pointer to Diligent::QueryDataOcclusion, Diligent::QueryDataBinaryOcclusion,
    ///                        Diligent::QueryDataTimestamp, or Diligent::QueryDataPipelineStatistics
    ///                        structure.
    /// \param [in] DataSize - Size of the data structure.
    /// \return     true if the query data is available and false otherwise.
    ///
    /// \note       In Direct3D11 backend timestamp queries will only be available after FinishFrame is called
    ///             for the frame in which they were collected.
    virtual bool GetData(void* pData, Uint32 DataSize) = 0;
};

} // namespace Diligent
