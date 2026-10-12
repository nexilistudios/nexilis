/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#include <nexilis/client/protocol/af_inet/tcp_client.hh>

#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/ports.hh>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace nexilis::client::af_inet
{

TCPClient::TCPClient(ClientAPI& clientApi)
    : NxClass("client::af_inet::TCPClient"),
      NXProtocol(),
      ClientProtocol(&clientApi),
      m_clientSocket(-1)
{
    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;

    uint16_t serverPort = clientApi.getInetTCPServerPortNumber();
    if (serverPort == 0)
    {
        // No explicit port configured, fall back to the default inet TCP port.
        serverPort = Ports::getInetTCPPort();
    }
    m_serverAddr.sin_port = htons(serverPort);
    m_serverAddr.sin_addr.s_addr = inet_addr(clientApi.getInetTCPServerAddress().c_str());
}

TCPClient::~TCPClient()
{
    stop();

    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }

    if (m_clientSocket != -1)
    {
        close(m_clientSocket);
        m_clientSocket = -1;
    }
}

TCPClient::TCPClient(TCPClient&& other)
    : NxClass(std::move(other)),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_clientSocket(std::move(other.m_clientSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread))
{
    other.m_clientSocket = -1;
}

TCPClient& TCPClient::operator=(TCPClient&& other)
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);
        m_clientSocket = std::move(other.m_clientSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);

        other.m_clientSocket = -1;
    }
    return *this;
}

void TCPClient::start()
{
    if (m_clientSocket == -1)
    {
        m_clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_clientSocket == -1)
        {
            Log::critical(header(), "TCPClient: Error creating socket");
            return;
        }
    }

    if (!connectToServer())
    {
        Log::error(header(), "TCPClient: Error connecting to server");
        return;
    }

    m_receiveThread = std::thread([this]()
                                  { receiveLoop(); });

    try
    {
        ClientProtocol::start(getType());
    }
    catch (const std::exception& e)
    {
        Log::error("Failed to start TCPClient: ", e.what());
        stop();
    }
}

void TCPClient::stop()
{
    if (m_clientSocket != -1)
    {
        // Shutdown before close so that a receive thread blocked in read()
        // is woken up with EOF instead of hanging forever.
        shutdown(m_clientSocket, SHUT_RDWR);
        close(m_clientSocket);
        m_clientSocket = -1;
    }
}

bool TCPClient::connectToServer()
{
    if (connect(m_clientSocket, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr)) == -1)
    {
        Log::error(header(), "TCPClient: Error connecting to server: ", std::strerror(errno));
        return false;
    }
    return true;
}

void TCPClient::sendMessage(const nx_data& message)
{
    sendFrame(reinterpret_cast<const char*>(message.data()), message.size());
}

void TCPClient::sendMessage(const nx_data& message, const std::function<void()>& callback)
{
    sendMessageWithCallback(message, callback);
}

std::future<void> TCPClient::sendMessageAsync(const nx_data& message)
{
    if (m_clientSocket == -1)
    {
        std::promise<void> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Socket is not open")));
        return promise.get_future();
    }

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    auto messageCopy = std::make_shared<nx_data>(message);

    std::thread([this, messageCopy, promise]()
                {
        try
        {
            if (m_clientSocket == -1)
            {
                throw std::runtime_error("Socket is not open");
            }

            if (!sendFrame(reinterpret_cast<const char*>(messageCopy->data()), messageCopy->size()))
            {
                throw std::runtime_error(std::string("Failed to send message: ") + std::strerror(errno));
            }
            promise->set_value();
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        } })
            .detach();

    return future;
}

bool TCPClient::sendFrame(const char* data, size_t dataSize)
{
    if (m_clientSocket == -1)
    {
        Log::error(header(), "TCPClient: Error sending message, socket is not open");
        return false;
    }

    // Write the frame size as a 4-byte big-endian value.
    uint32_t frameSize = static_cast<uint32_t>(dataSize);
    uint32_t networkSize = htonl(frameSize);

    ssize_t frameBytes = send(m_clientSocket, &networkSize, sizeof(networkSize), MSG_NOSIGNAL);
    if (frameBytes <= 0)
    {
        Log::error(header(), "TCPClient: Error sending frame size");
        return false;
    }

    size_t bytesSent = 0;
    while (bytesSent < dataSize)
    {
        ssize_t sent = send(m_clientSocket, data + bytesSent, dataSize - bytesSent, MSG_NOSIGNAL);
        if (sent <= 0)
        {
            Log::error(header(), "TCPClient: Error sending message");
            return false;
        }
        bytesSent += static_cast<size_t>(sent);
    }

    return true;
}

ssize_t TCPClient::receive(char* buffer, size_t bufferSize)
{
    uint32_t networkSize = 0;
    ssize_t frameBytes = read(m_clientSocket, &networkSize, sizeof(networkSize));
    if (frameBytes <= 0)
    {
        return -1;
    }

    if (frameBytes != static_cast<ssize_t>(sizeof(networkSize)))
    {
        Log::error(header(), "TCPClient: Error receiving frame size");
        return -1;
    }

    uint32_t frameSize = ntohl(networkSize);

    if (frameSize > bufferSize)
    {
        Log::error(header(), "TCPClient: Frame size exceeds buffer size");
        return -1;
    }

    size_t bytesReceived = 0;
    while (bytesReceived < frameSize)
    {
        ssize_t bytesRead = read(m_clientSocket, buffer + bytesReceived, frameSize - bytesReceived);
        if (bytesRead <= 0)
        {
            return -1;
        }
        bytesReceived += static_cast<size_t>(bytesRead);
    }

    return static_cast<ssize_t>(bytesReceived);
}

void TCPClient::receiveLoop()
{
    while (true)
    {
        char buffer[NEXILIS_BUFFER];
        ssize_t receivedBytes = receive(buffer, sizeof(buffer));
        if (receivedBytes <= 0)
        {
            break;
        }

        nx_data receivedData(buffer, buffer + receivedBytes);
        getClientAPI()->readMessage(receivedData);
    }
}

} // namespace nexilis::client::af_inet
