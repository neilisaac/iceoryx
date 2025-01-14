// Copyright (c) 2022 by Apex.AI Inc. All rights reserved.
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

#include "test_popo_server_port_common.hpp"

namespace iox_test_popo_server_port
{
constexpr iox::units::Duration ServerPort_test::DEADLOCK_TIMEOUT;

TEST_F(ServerPort_test, GetRequestQueueFullPolicyReturnsCorrectValues)
{
    ::testing::Test::RecordProperty("TEST_ID", "4b3dbe4c-6c3d-4129-a4f0-643a801a4803");

    auto& sutWithDiscardOldestData = serverPortWithOfferOnCreate;
    auto& sutWithBlockProducer = serverOptionsWithBlockProducerRequestQueueFullPolicy;

    EXPECT_THAT(sutWithDiscardOldestData.portRouDi.getRequestQueueFullPolicy(),
                Eq(QueueFullPolicy::DISCARD_OLDEST_DATA));
    EXPECT_THAT(sutWithBlockProducer.portRouDi.getRequestQueueFullPolicy(), Eq(QueueFullPolicy::BLOCK_PRODUCER));
}

TEST_F(ServerPort_test, GetClientTooSlowPolicyReturnsCorrectValues)
{
    ::testing::Test::RecordProperty("TEST_ID", "7090916c-57c5-4ef4-9876-87e58ab64058");

    auto& sutWithDiscardOldestData = serverPortWithOfferOnCreate;
    auto& sutWithWaitForConsumer = serverOptionsWithWaitForConsumerClientTooSlowPolicy;

    EXPECT_THAT(sutWithDiscardOldestData.portRouDi.getClientTooSlowPolicy(),
                Eq(ConsumerTooSlowPolicy::DISCARD_OLDEST_DATA));
    EXPECT_THAT(sutWithWaitForConsumer.portRouDi.getClientTooSlowPolicy(),
                Eq(ConsumerTooSlowPolicy::WAIT_FOR_CONSUMER));
}

TEST_F(ServerPort_test, ReleaseAllChunksWorks)
{
    ::testing::Test::RecordProperty("TEST_ID", "f29f8890-c106-470d-820f-32eeea694f17");
    auto& sut = serverPortWithOfferOnCreate;

    // produce chunks for the chunk receiver
    pushRequests(sut.requestQueuePusher, QUEUE_CAPACITY);

    // produce chunks for the chunk sender
    allocateResponseWithRequestHeaderAndThen(sut, [&](const auto, auto res) { sut.portUser.sendResponse(res); });

    EXPECT_THAT(getNumberOfUsedChunks(), Ne(0U));

    sut.portRouDi.releaseAllChunks();

    // this is not part of the server port but holds the chunk from `sendResponse`
    clientResponseQueue.clear();

    EXPECT_THAT(getNumberOfUsedChunks(), Eq(0U));
}

// BEGIN tryGetCaProMessage tests

TEST_F(ServerPort_test, TryGetCaProMessageOnOfferWhenPortIsNotOffering)
{
    ::testing::Test::RecordProperty("TEST_ID", "8944621b-4753-413b-bee0-a714fa4324c8");
    auto& sut = serverPortWithoutOfferOnCreate;

    sut.portUser.offer();

    sut.portRouDi.tryGetCaProMessage()
        .and_then([&](const auto& caproMessage) {
            EXPECT_THAT(caproMessage.m_type, Eq(CaproMessageType::OFFER));
            EXPECT_THAT(caproMessage.m_serviceType, Eq(CaproServiceType::SERVER));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });
}

TEST_F(ServerPort_test, TryGetCaProMessageOnOfferWhenPortAlreadyOffers)
{
    ::testing::Test::RecordProperty("TEST_ID", "15a399ee-b162-4b42-8aab-da13571fb478");
    auto& sut = serverPortWithOfferOnCreate;

    sut.portUser.offer();

    sut.portRouDi.tryGetCaProMessage()
        .and_then([&](const auto& caproMessage) {
            GTEST_FAIL() << "Expected no CaPro message but got: " << caproMessage.m_type;
        })
        .or_else([&]() { GTEST_SUCCEED(); });
}

TEST_F(ServerPort_test, TryGetCaProMessageOnStopOfferWhenPortIsOffering)
{
    ::testing::Test::RecordProperty("TEST_ID", "83467e90-734b-4e51-836c-2dbeaf44ce95");
    auto& sut = serverPortWithOfferOnCreate;

    sut.portUser.stopOffer();

    sut.portRouDi.tryGetCaProMessage()
        .and_then([&](const auto& caproMessage) {
            EXPECT_THAT(caproMessage.m_type, Eq(CaproMessageType::STOP_OFFER));
            EXPECT_THAT(caproMessage.m_serviceType, Eq(CaproServiceType::SERVER));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });
}

TEST_F(ServerPort_test, TryGetCaProMessageOnStopOfferWhenPortIsNotOffering)
{
    ::testing::Test::RecordProperty("TEST_ID", "a9a162d3-307a-4add-af23-63511da4b07e");
    auto& sut = serverPortWithoutOfferOnCreate;

    sut.portUser.stopOffer();

    sut.portRouDi.tryGetCaProMessage()
        .and_then([&](const auto& caproMessage) {
            GTEST_FAIL() << "Expected no CaPro message but got: " << caproMessage.m_type;
        })
        .or_else([&]() { GTEST_SUCCEED(); });
}

// END tryGetCaProMessage tests

// BEGIN test CaPro transitions

TEST_F(ServerPort_test, StateNotOfferedWithAllRelevantCaProMessageTypesButOfferReactsWithNack)
{
    ::testing::Test::RecordProperty("TEST_ID", "ceaef856-2a8d-46c0-9167-fe1ca6fad736");

    for (const auto caproMessageType :
         {CaproMessageType::CONNECT, CaproMessageType::DISCONNECT, CaproMessageType::STOP_OFFER})
    {
        SCOPED_TRACE(caproMessageType);

        auto& sut = serverPortWithoutOfferOnCreate;

        auto caproMessage = CaproMessage{caproMessageType, sut.portData.m_serviceDescription};

        sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
            .and_then([&](const auto& responseCaproMessage) {
                EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
                EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::NACK));
                EXPECT_THAT(responseCaproMessage.m_serviceType, Eq(CaproServiceType::NONE));
            })
            .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });
    }
}

TEST_F(ServerPort_test, StateNotOfferedWithCaProMessageTypeOfferReactsWithOffer)
{
    ::testing::Test::RecordProperty("TEST_ID", "bd8667d3-9c09-4caa-865e-bb3c7c3c1283");
    auto& sut = serverPortWithoutOfferOnCreate;

    sut.portUser.offer();

    // this is what tryGetCaProMessage does before it calls dispatchCaProMessageAndGetPossibleResponse
    auto caproMessage = CaproMessage{CaproMessageType::OFFER, sut.portData.m_serviceDescription};
    caproMessage.m_serviceType = CaproServiceType::SERVER;

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            EXPECT_THAT(sut.portUser.isOffered(), Eq(true));
            EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
            EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::OFFER));
            EXPECT_THAT(responseCaproMessage.m_serviceType, Eq(CaproServiceType::SERVER));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });
}

TEST_F(ServerPort_test, StateOfferedWithCaProMessageTypeConnectReactsWithAckAndValidRequestQueue)
{
    ::testing::Test::RecordProperty("TEST_ID", "15ae7423-0945-45b6-b164-cc7ff5b979b1");
    auto& sut = serverPortWithOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::CONNECT, sut.portData.m_serviceDescription};
    caproMessage.m_chunkQueueData = &clientChunkQueueData;

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
            EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::ACK));
            EXPECT_THAT(responseCaproMessage.m_chunkQueueData, Eq(&sut.portData.m_chunkReceiverData));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });

    EXPECT_TRUE(sut.portUser.hasClients());
}

TEST_F(ServerPort_test, StateOfferedWithCaProMessageTypeConnectAndNoResponseQueueCallsErrorHandler)
{
    ::testing::Test::RecordProperty("TEST_ID", "616b7a3d-6463-43bd-b75e-a257f62a006b");
    auto& sut = serverPortWithOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::CONNECT, sut.portData.m_serviceDescription};
    caproMessage.m_chunkQueueData = nullptr;

    iox::cxx::optional<iox::Error> detectedError;
    auto errorHandlerGuard = iox::ErrorHandler::setTemporaryErrorHandler(
        [&](const iox::Error error, const std::function<void()>, const iox::ErrorLevel errorLevel) {
            EXPECT_THAT(error, Eq(iox::Error::kPOPO__SERVER_PORT_NO_CLIENT_RESPONSE_QUEUE_TO_CONNECT));
            EXPECT_THAT(errorLevel, Eq(iox::ErrorLevel::MODERATE));
            detectedError.emplace(error);
        });

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
            EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::NACK));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });

    EXPECT_TRUE(detectedError.has_value());
}

TEST_F(ServerPort_test, StateOfferedWithCaProMessageTypeDisconnectReactsWithNackWhenResponseQueueNotPresent)
{
    ::testing::Test::RecordProperty("TEST_ID", "8e1a2bff-b58f-4545-8ff4-044f168276f1");
    auto& sut = serverPortWithOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::DISCONNECT, sut.portData.m_serviceDescription};
    caproMessage.m_chunkQueueData = &clientChunkQueueData;

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
            EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::NACK));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });
}

TEST_F(ServerPort_test, StateOfferedWithCaProMessageTypeDisconnectReactsWithAckWhenResponseQueueWasPresent)
{
    ::testing::Test::RecordProperty("TEST_ID", "7255fd86-a00c-4539-b06d-ea6f96f589cb");
    auto& sut = serverPortWithOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::CONNECT, sut.portData.m_serviceDescription};
    caproMessage.m_chunkQueueData = &clientChunkQueueData;
    IOX_DISCARD_RESULT(sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage));

    caproMessage.m_type = CaproMessageType::DISCONNECT;

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            EXPECT_THAT(responseCaproMessage.m_serviceDescription, Eq(sut.portData.m_serviceDescription));
            EXPECT_THAT(responseCaproMessage.m_type, Eq(iox::capro::CaproMessageType::ACK));
        })
        .or_else([&]() { GTEST_FAIL() << "Expected CaPro message but got none"; });

    EXPECT_FALSE(sut.portUser.hasClients());
}

TEST_F(ServerPort_test, StateNotOfferedWithInvalidCaProMessageTypeCallsErrorHandler)
{
    ::testing::Test::RecordProperty("TEST_ID", "3c645c89-e846-44b3-8e52-31642af593b5");
    auto& sut = serverPortWithoutOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::PUB, sut.portData.m_serviceDescription};

    iox::cxx::optional<iox::Error> detectedError;
    auto errorHandlerGuard = iox::ErrorHandler::setTemporaryErrorHandler(
        [&](const iox::Error error, const std::function<void()>, const iox::ErrorLevel errorLevel) {
            EXPECT_THAT(error, Eq(iox::Error::kPOPO__CAPRO_PROTOCOL_ERROR));
            EXPECT_THAT(errorLevel, Eq(iox::ErrorLevel::SEVERE));
            detectedError.emplace(error);
        });

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            GTEST_FAIL() << "Expected no CaPro message but got: " << responseCaproMessage.m_type;
        })
        .or_else([&]() { GTEST_SUCCEED(); });
}

TEST_F(ServerPort_test, StateOfferedWithInvalidCaProMessageTypeCallsErrorHandler)
{
    ::testing::Test::RecordProperty("TEST_ID", "30613e47-be74-4c74-a743-1bffd8468040");
    auto& sut = serverPortWithOfferOnCreate;

    auto caproMessage = CaproMessage{CaproMessageType::SUB, sut.portData.m_serviceDescription};

    iox::cxx::optional<iox::Error> detectedError;
    auto errorHandlerGuard = iox::ErrorHandler::setTemporaryErrorHandler(
        [&](const iox::Error error, const std::function<void()>, const iox::ErrorLevel errorLevel) {
            EXPECT_THAT(error, Eq(iox::Error::kPOPO__CAPRO_PROTOCOL_ERROR));
            EXPECT_THAT(errorLevel, Eq(iox::ErrorLevel::SEVERE));
            detectedError.emplace(error);
        });

    sut.portRouDi.dispatchCaProMessageAndGetPossibleResponse(caproMessage)
        .and_then([&](const auto& responseCaproMessage) {
            GTEST_FAIL() << "Expected no CaPro message but got: " << responseCaproMessage.m_type;
        })
        .or_else([&]() { GTEST_SUCCEED(); });
}

// END test CaPro transitions

} // namespace iox_test_popo_server_port
