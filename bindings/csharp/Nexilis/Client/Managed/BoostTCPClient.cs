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

using System;
using System.Runtime.InteropServices;

namespace Nexilis.Client
{
    /// <summary>
    /// Represents a Boost TCP client.
    /// </summary>
    public class BoostTCPClient : IDisposable
    {
        private IntPtr _boostTcpClientPtr;

        /// <summary>
        /// Initializes a new instance of the <see cref="BoostTCPClient"/> class.
        /// </summary>
        public BoostTCPClient(ProtocolManager protocolManager, ClientAPI clientApi)
        {
            if (protocolManager == null)
            {
                throw new ArgumentNullException(nameof(protocolManager));
            }

            if (clientApi == null)
            {
                throw new ArgumentNullException(nameof(clientApi));
            }

            _boostTcpClientPtr = BoostTCPClientNative.nexilis_boost_tcp_client_create(
                    protocolManager.ProtocolManagerPtr, clientApi.ClientApiPtr);
            if (_boostTcpClientPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException(
                    "Boost TCP client creation failed. Configure a TLS-PSK password or disable TLS on both peers.");
            }
        }

        /// <summary>
        /// Gets the native pointer to the Boost TCP client.
        /// </summary>
        public IntPtr BoostTcpClientPtr => _boostTcpClientPtr;

        /// <summary>
        /// Releases the resources used by the <see cref="BoostTCPClient"/>.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Releases the unmanaged resources used by the <see cref="BoostTCPClient"/> and optionally releases the managed resources.
        /// </summary>
        /// <param name="disposing">True to release both managed and unmanaged resources; false to release only unmanaged resources.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (_boostTcpClientPtr != IntPtr.Zero)
            {
                BoostTCPClientNative.nexilis_boost_tcp_client_destroy(_boostTcpClientPtr);
                _boostTcpClientPtr = IntPtr.Zero;
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="BoostTCPClient"/> class.
        /// </summary>
        ~BoostTCPClient()
        {
            Dispose(false);
        }

        /// <summary>
        /// Gets the protocol type of the Boost TCP client.
        /// </summary>
        /// <returns>The protocol type.</returns>
        public ProtocolType GetProtocolType()
        {
            return BoostTCPClientNative.nexilis_boost_tcp_client_get_type(_boostTcpClientPtr);
        }

        /// <summary>
        /// Starts the Boost TCP client.
        /// </summary>
        public void Start()
        {
            BoostTCPClientNative.nexilis_boost_tcp_client_start(_boostTcpClientPtr);
        }

        /// <summary>
        /// Runs the full startClient sequence: start, send clientId, wait for init, send rooms info.
        /// </summary>
        public bool StartClient(ClientAPI clientApi)
        {
            return BoostTCPClientNative.nexilis_start_client(clientApi.ClientApiPtr, _boostTcpClientPtr);
        }

        /// <summary>
        /// Stops the Boost TCP client.
        /// </summary>
        public void Stop()
        {
            BoostTCPClientNative.nexilis_boost_tcp_client_stop(_boostTcpClientPtr);
        }

        /// <summary>
        /// Sends a message using the Boost TCP client.
        /// </summary>
        /// <param name="message">The message to send.</param>
        /// <param name="length">The length of the message.</param>
        public void SendMessage(byte[] message, uint length)
        {
            if (message == null || message.Length < length)
            {
                throw new ArgumentException("Invalid message or length.");
            }

            BoostTCPClientNative.nexilis_boost_tcp_client_send_message(_boostTcpClientPtr, message, length);
        }

        /// <summary>
        /// Sends a message using the Boost TCP client with a callback.
        /// /// </summary>
        /// <param name="message">The message to send.</param>
        /// <param callref="callback">The callback to invoke when the message is sent.</param>
        public void SendMessageWithCallback(string message, IntPtr callback)
        {
            if (string.IsNullOrEmpty(message))
            {
                throw new ArgumentException("Invalid message.");
            }

            byte[] messageBytes = System.Text.Encoding.UTF8.GetBytes(message);
            BoostTCPClientNative.nexilis_boost_tcp_client_send_message_with_callback(
                    _boostTcpClientPtr, messageBytes, (uint)messageBytes.Length, callback);
        }
    }
}

