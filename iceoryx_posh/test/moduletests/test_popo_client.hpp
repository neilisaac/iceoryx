// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2021 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_posh/popo/client.hpp"
#include "iceoryx_posh/testing/mocks/chunk_mock.hpp"
#include "mocks/client_mock.hpp" // TODO

#include "test.hpp"

namespace
{
using namespace ::testing;
using ::testing::_;

struct DummyData
{
    DummyData() = default;
    DummyData(uint64_t initialVal)
        : val(initialVal)
    {
    }
    // constexpr variable cannot be used instead in tests (linker optimization - no address)
    static constexpr uint64_t defaultVal()
    {
        return 42U;
    };
    uint64_t val{defaultVal()};
};

using TestClient = iox::popo::ClientImpl<DummyData, iox::mepoo::NoUserHeader, MockBaseClient<DummyData>>;

class PublisherTest : public Test
{
  public:
    PublisherTest()
    {
    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }

  protected:
    ChunkMock<DummyData> chunkMock;
    TestClient sut{{"", "", ""}};
    MockClientPortUser& portMock{sut.mockPort()};
};
} // namespace
