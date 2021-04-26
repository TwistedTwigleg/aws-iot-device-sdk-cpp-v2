/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include <aws/crt/Api.h>

#include <aws/eventstreamrpc/EventStreamClient.h>

#include <aws/testing/aws_test_harness.h>

#include <sstream>
#include <iostream>

using namespace Aws::Eventstreamrpc;

static int s_TestEventStreamConnect(struct aws_allocator *allocator, void *ctx)
{
    (void)ctx;
    {
        Aws::Crt::ApiHandle apiHandle(allocator);
        Aws::Crt::Io::TlsContextOptions tlsCtxOptions = Aws::Crt::Io::TlsContextOptions::InitDefaultClient();
        Aws::Crt::Io::TlsContext tlsContext(tlsCtxOptions, Aws::Crt::Io::TlsMode::CLIENT, allocator);
        ASSERT_TRUE(tlsContext);

        Aws::Crt::Io::TlsConnectionOptions tlsConnectionOptions = tlsContext.NewConnectionOptions();
        Aws::Crt::Io::SocketOptions socketOptions;
        socketOptions.SetConnectTimeoutMs(1000);

        Aws::Crt::Io::EventLoopGroup eventLoopGroup(0, allocator);
        ASSERT_TRUE(eventLoopGroup);

        Aws::Crt::Io::DefaultHostResolver defaultHostResolver(eventLoopGroup, 8, 30, allocator);
        ASSERT_TRUE(defaultHostResolver);

        Aws::Crt::Io::ClientBootstrap clientBootstrap(eventLoopGroup, defaultHostResolver, allocator);
        ASSERT_TRUE(clientBootstrap);
        clientBootstrap.EnableBlockingShutdown();
        Aws::Crt::List<EventStreamHeader> authHeaders;
        authHeaders.push_back(EventStreamHeader(Aws::Crt::String("client-name"), Aws::Crt::String("accepted.testy_mc_testerson"), allocator));
        MessageAmendment connectionAmendment(authHeaders);
        auto messageAmender = [&](void) -> MessageAmendment& {
            return connectionAmendment;
        };
        std::shared_ptr<EventstreamRpcConnection> connection(nullptr);
        bool errorOccured = true;
        bool connectionShutdown = false;

        std::condition_variable semaphore;
        std::mutex semaphoreLock;

        auto onConnect = [&](const std::shared_ptr<EventstreamRpcConnection> &newConnection) {
            std::lock_guard<std::mutex> lockGuard(semaphoreLock);

            std::cout << "Connected" << std::endl;

            connection = newConnection;

            semaphore.notify_one();
        };

        auto onDisconnect = [&](const std::shared_ptr<EventstreamRpcConnection> &newConnection,
                                int errorCode) 
        {
            std::lock_guard<std::mutex> lockGuard(semaphoreLock);

            std::cout << "Disconnected" << std::endl;

            if(errorCode) errorOccured = true; else connectionShutdown = true;

            connection = newConnection;

            semaphore.notify_one();
        };

        Aws::Crt::String hostName = "127.0.0.1";
        EventstreamRpcConnectionOptions options;
        options.Bootstrap = &clientBootstrap;
        options.SocketOptions = socketOptions;
        options.HostName = hostName;
        options.Port = 8033;
        options.ConnectMessageAmenderCallback = messageAmender;
        options.OnConnectCallback = onConnect;
        options.OnDisconnectCallback = onDisconnect;
        options.OnErrorCallback = nullptr;
        options.OnPingCallback = nullptr;

        {
            std::unique_lock<std::mutex> semaphoreULock(semaphoreLock);
            ASSERT_TRUE(EventstreamRpcConnection::CreateConnection(options, allocator));
            semaphore.wait(semaphoreULock, [&]() { return connection; });
            ASSERT_TRUE(connection);
            connection->Close();
        }
    }

    return AWS_OP_SUCCESS;
}

AWS_TEST_CASE(EventStreamConnect, s_TestEventStreamConnect)
