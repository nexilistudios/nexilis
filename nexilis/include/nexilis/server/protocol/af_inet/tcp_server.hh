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

#ifndef NEXILIS_AF_INET_TCP_SERVER_HH
#define NEXILIS_AF_INET_TCP_SERVER_HH

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>

#include <netinet/in.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#ifdef __linux__

namespace nexilis::server::af_inet
{

/// AF_INET TCP server protocol using raw sockets.
class TCPServer : public NXProtocol,
                  public ServerProtocol
{
public:
    /// A connected client.
    class Client
    {
    public:
        /// Constructor.
        Client(std::string address, uint16_t port, int socket)
            : m_address(std::move(address)),
              m_port(port),
              m_socket(socket)
        {
        }

        /// Default constructor.
        Client() = default;

        /// Get the client address.
        const std::string& getAddress() const
        {
            return m_address;
        }

        /// Get the client port.
        uint16_t getPort() const
        {
            return m_port;
        }

        /// Get the client socket.
        int getSocket() const
        {
            return m_socket;
        }

    private:
        std::string m_address;
        uint16_t m_port = 0;
        int m_socket = -1;
    };

public:
    /// Constructor.
    TCPServer(const ServerConfig& settings, uint16_t port);

    /// Destructor.
    ~TCPServer();

    /// Move constructor.
    TCPServer(TCPServer&& other);

    /// Move assignment operator.
    TCPServer& operator=(TCPServer&& other);

    /// Deleted copy constructor.
    TCPServer(const TCPServer& other) = delete;

    /// Deleted copy assignment operator.
    TCPServer& operator=(const TCPServer& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_INET_TCP_SERVER;
    }

    /// Set the port number.
    void setPort(uint16_t port)
    {
        m_port = port;
    }

    /// Get the port number.
    uint16_t getPort() const
    {
        return m_port;
    }

private:
    /// Create the socket.
    void createSocket();

    /// Bind the socket.
    void bindSocket();

    /// Listen for connections.
    void listenSocket();

    /// Accept client connections.
    void acceptClients();

    /// Handle a connected client.
    void handleClient(const std::shared_ptr<Client>& client);

    /// Send a message to a connected client.
    bool sendMessageToClient(const std::string& message, int clientSocket);

    /// Receive a framed message from a client.
    bool receiveMessage(int clientSocket, nx_data& receivedData);

    /// Find a connected client by socket.
    std::shared_ptr<Client> findClient(int clientSocket);

private:
    uint16_t m_port;
    int m_serverSocket = -1;
    sockaddr_in m_serverAddr;
    std::thread m_acceptThread;
    std::vector<std::thread> m_clientThreads;
    std::vector<std::shared_ptr<Client>> m_clients;
    std::unique_ptr<std::atomic<bool>> m_stopped = std::make_unique<std::atomic<bool>>(false);
    std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
};

} // namespace nexilis::server::af_inet

#endif

#endif // NEXILIS_AF_INET_TCP_SERVER_HH
