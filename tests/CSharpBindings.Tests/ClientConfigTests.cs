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

using Xunit;
using Nexilis;
using Nexilis.Client;

namespace Nexilis.Tests
{
    public class ClientConfigTests
    {
        [Fact]
        public void CreateAndDispose()
        {
            using var config = new ClientConfig();
            Assert.NotEqual(System.IntPtr.Zero, config.ConfigPtr);
        }

        [Fact]
        public void PasswordSetGet()
        {
            using var config = new ClientConfig();
            config.SetPassword("my_password");

            Assert.Equal("my_password", config.GetPassword());
        }

        [Fact]
        public void Mode()
        {
            using var config = new ClientConfig();
            // 2 = password_protected.
            config.SetMode(2);
        }

        [Fact]
        public void BoostTCPAddress()
        {
            using var config = new ClientConfig();
            config.SetBoostTCPAddress("192.168.1.100");

            Assert.Equal("192.168.1.100", config.GetBoostTCPServerAddress());
        }

        [Fact]
        public void BoostUDPAddress()
        {
            using var config = new ClientConfig();
            config.SetBoostUDP("10.0.0.1");

            Assert.Equal("10.0.0.1", config.GetBoostUdpServerAddress());
        }

        [Fact]
        public void MessageEncryptionToggle()
        {
            using var config = new ClientConfig();

            // Optional feature: off by default.
            Assert.False(config.IsMessageEncryptionEnabled());

            config.SetMessageEncryption(true);
            Assert.True(config.IsMessageEncryptionEnabled());

            config.SetMessageEncryption(false);
            Assert.False(config.IsMessageEncryptionEnabled());
        }

        [Fact]
        public void TlsToggle()
        {
            using var config = new ClientConfig();

            Assert.True(config.IsTlsEnabled());

            config.SetTls(true);
            Assert.True(config.IsTlsEnabled());

            config.SetTls(false);
            Assert.False(config.IsTlsEnabled());
        }

        [Fact]
        public void BoostTcpClientRequiresPasswordByDefault()
        {
            using var config = new ClientConfig();
            using var api = new ClientAPI(config);
            using var manager = new ProtocolManager();

            Assert.Throws<System.InvalidOperationException>(() => new BoostTCPClient(manager, api));
        }

        [Fact]
        public void BoostTcpClientCanExplicitlyDisableTls()
        {
            using var config = new ClientConfig();
            config.SetTls(false);
            using var api = new ClientAPI(config);
            using var manager = new ProtocolManager();
            using var client = new BoostTCPClient(manager, api);

            Assert.NotEqual(System.IntPtr.Zero, client.BoostTcpClientPtr);
        }

        [Fact]
        public void FluentSettersReturnSameInstance()
        {
            using var config = new ClientConfig();

            var returned = config.Password("pw").BoostTCP("1.2.3.4").BoostUDP("5.6.7.8");

            Assert.Same(config, returned);
            Assert.Equal("pw", config.GetPassword());
            Assert.Equal("1.2.3.4", config.GetBoostTCPServerAddress());
            Assert.Equal("5.6.7.8", config.GetBoostUdpServerAddress());
        }
    }
}
