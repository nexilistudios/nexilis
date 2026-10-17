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

#ifdef __linux__

#include <nexilis/client/protocol/af_unix/stream_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace nexilis::client::af_unix
{

StreamClient::StreamClient(ClientAPI& clientApi)
    : NxClass("client::af_unix::StreamClient"),
      ClientProtocol(&clientApi),
      m_serverSocketPath(clientApi.getUnixStreamPath()),
      m_mutex(std::make_unique<std::mutex>()),
      m_running(std::make_unique<std::atomic<bool>>(false))
{
    createSocket();
    connectToServer();
}

StreamClient::~StreamClient()
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

StreamClient::StreamClient(StreamClient&& other)
    : NxClass(std::move(other)),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_serverSocketPath(std::move(other.m_serverSocketPath)),
      m_clientSocket(std::move(other.m_clientSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_mutex(std::move(other.m_mutex)),
      m_running(std::move(other.m_running))
{
}

StreamClient& StreamClient::operator=(StreamClient&& other)
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);

        m_serverSocketPath = std::move(other.m_serverSocketPath);
        m_clientSocket = std::move(other.m_clientSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);
        m_mutex = std::move(other.m_mutex);
        m_running = std::move(other.m_running);
    }
    return *this;
}

void StreamClient::createSocket()
{
    m_clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_clientSocket == -1)
    {
        perror("socket");
        close(m_clientSocket);
    }
}

void StreamClient::connectToServer()
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_serverAddr.sun_family = AF_UNIX;
    strcpy(m_serverAddr.sun_path, m_serverSocketPath.c_str());

    auto address = reinterpret_cast<sockaddr*>(&m_serverAddr);
    if (connect(m_clientSocket, address, sizeof(m_serverAddr)) == -1)
    {
        Log::error("Failed to connect to: ", m_serverSocketPath, " - ", strerror(errno));
        close(m_clientSocket);
        m_clientSocket = -1;
        updateProtocolStatus(ProtocolStatus::error);
    }
}

void StreamClient::sendMsg(const nx_data& message)
{
    Log::debug("Sending message to: ", m_serverAddr.sun_path);
    auto framed = ClientProtocol::frame(message);
    std::lock_guard<std::mutex> lock(*m_mutex);
    size_t offset = 0;
    while (offset < framed.size())
    {
        ssize_t sentBytes = send(m_clientSocket, framed.data() + offset,
                                 framed.size() - offset, MSG_NOSIGNAL);
        if (sentBytes <= 0)
        {
            Log::error("Error sending message");
            return;
        }
        offset += static_cast<size_t>(sentBytes);
    }
}

void StreamClient::sendMessage(const nx_data& message)
{
    auto request = authenticate(message);
    if (!request.empty())
        sendMsg(request);
}

void StreamClient::sendMessage(const nx_data& message, const std::function<void()>& callback)
{
    sendMessageWithCallback(message, callback);
}

std::future<void> StreamClient::sendMessageAsync(const nx_data& message)
{
    std::promise<void> promise;
    promise.set_value();
    auto future = promise.get_future();

    sendMessage(message);

    return future;
}

void StreamClient::start()
{
    m_running->store(true);

    if (m_clientSocket == -1)
    {
        Log::error(header(), "Cannot start: no active connection to: ", m_serverSocketPath);
        updateProtocolStatus(ProtocolStatus::error);
        m_running->store(false);
        return;
    }

    m_receiveThread = std::thread([this]()
                                  {
        while (m_running->load())
        {
            auto data = receiveMessage();
            if (!m_running->load() || data.empty())
            {
                break;
            }
            ClientProtocol::getClientAPI()->readMessage(data);
        } });
    ClientProtocol::start(getType());
}

void StreamClient::stop()
{
    if (m_running)
    {
        m_running->store(false);
    }

    // Unblock a blocking recv so the receive thread can exit.
    if (m_clientSocket != -1)
    {
        shutdown(m_clientSocket, SHUT_RDWR);
    }
}

nx_data StreamClient::receiveMessage()
{
    int socket;
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        if (!m_running->load() || m_clientSocket == -1)
            return {};
        socket = m_clientSocket;
    }

    // Do not hold the send mutex while waiting for a server response: the
    // initial authentication request must be able to use the same socket.
    nx_data receivedData(NEXILIS_BUFFER);
    ssize_t bytesRead = recv(socket, receivedData.data(), receivedData.size(), 0);

    if (bytesRead <= 0)
    {
        if (bytesRead == -1)
        {
            perror("recv");
        }

        std::lock_guard<std::mutex> lock(*m_mutex);
        if (m_clientSocket == socket)
        {
            close(socket);
            m_clientSocket = -1;
        }

        receivedData.clear();
        return receivedData;
    }

    receivedData.resize(bytesRead);
    return receivedData;
}

} // namespace nexilis::client::af_unix

#endif
