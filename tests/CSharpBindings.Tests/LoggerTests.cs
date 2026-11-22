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
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Xunit;
using Nexilis;

namespace Nexilis.Tests
{
    public class LoggerTests : IDisposable
    {
        private readonly Logger.Logger _logger;

        public LoggerTests()
        {
            _logger = new Logger.Logger();
        }

        public void Dispose()
        {
            _logger.Dispose();
        }

        private string GenerateRandomFileName()
        {
            return $"test_file_{new Random().Next()}.log";
        }

        /*
        [Fact]
        public void DefaultValues()
        {
            Assert.False(_logger.GetLevel((int)LogLevel.Debug));
            Assert.False(_logger.GetLevel((int)LogLevel.Info));
            Assert.False(_logger.GetLevel((int)LogLevel.Warning));
            Assert.False(_logger.GetLevel((int)LogLevel.Error));
            Assert.False(_logger.GetLevel((int)LogLevel.Critical));
        }
        */

        [Fact]
        public void FileHandler()
        {
            string fileName = GenerateRandomFileName();
            var fileHandler = new Logger.FileHandler(fileName);

            _logger.AddFileHandler(fileHandler);
            _logger.SetLevel((int)Logger.LogLevel.INFO);
            _logger.Info("Test message");

            string line = File.ReadAllText(fileName);
            Assert.Equal("INFO: Test message", line.Trim());

            File.Delete(fileName);
            _logger.RemoveHandler(fileHandler.GetId());
        }

        [Fact]
        public void FunctionHandler()
        {
            string message = "Function handler test message";
            string expectedMessage = "INFO: Function handler test message";

            // Use a list to capture the log level and message
            var capturedLogs = new List<(Logger.LogLevel Level, string Message)>();

            // Create a function handler that captures the log level and message
            var functionHandler = new Logger.FunctionHandler((level, msg) =>
            {
                capturedLogs.Add((level, msg));
            });

            // Act
            _logger.AddFunctionHandler(functionHandler);
            _logger.SetLevel((int)Logger.LogLevel.INFO);
            _logger.Info(message);

            // Assert
            // Check that exactly one log entry was captured
            Assert.Single(capturedLogs);

            // Check the log level and message
            var (actualLevel, actualMessage) = capturedLogs[0];
            Assert.Equal(expectedMessage, actualMessage);

            // Clean up
            _logger.RemoveHandler(functionHandler.GetId());
        }

        [Fact]
        public void FunctionHandlerRetainsIdAfterLoggerTakesOwnership()
        {
            using var handler = new Logger.FunctionHandler((_, _) => { });
            ulong id = handler.GetId();

            _logger.AddFunctionHandler(handler);

            Assert.Equal(id, handler.GetId());
            _logger.RemoveHandler(id);
        }

        [Fact]
        public void NxLoggerReplacesCallbackWithoutLeavingNativeHandler()
        {
            using var logger = new NxLogger("replacement");
            int firstCalls = 0;
            int secondCalls = 0;
            logger.Setup((_, _) => firstCalls++);
            logger.Setup((_, _) => secondCalls++);

            GC.Collect();
            GC.WaitForPendingFinalizers();
            logger.Debug("message");

            Assert.Equal(0, firstCalls);
            Assert.Equal(1, secondCalls);
        }

        [Fact]
        public void MultipleHandlersWithThreads()
        {
            string fileName = GenerateRandomFileName();
            var consoleHandler = new Logger.ConsoleHandler();
            var fileHandler = new Logger.FileHandler(fileName);

            _logger.AddConsoleHandler(consoleHandler);
            _logger.AddFileHandler(fileHandler);
            _logger.SetLevel((int)Logger.LogLevel.WARNING);

            var t1 = new Thread(() => _logger.Warning("Test message from thread 1"));
            var t2 = new Thread(() => _logger.Warning("Test message from thread 2"));

            t1.Start();
            t2.Start();
            t1.Join();
            t2.Join();

            string[] lines = File.ReadAllLines(fileName);
            Assert.True((lines[0] == "WARNING: Test message from thread 1" && lines[1] == "WARNING: Test message from thread 2") ||
                        (lines[0] == "WARNING: Test message from thread 2" && lines[1] == "WARNING: Test message from thread 1"));

            File.Delete(fileName);
            _logger.RemoveHandler(consoleHandler.GetId());
            _logger.RemoveHandler(fileHandler.GetId());
        }

        /*
        [Theory]
        [InlineData((int)LogLevel.Debug)]
        [InlineData((int)LogLevel.Info)]
        [InlineData((int)LogLevel.Warning)]
        [InlineData((int)LogLevel.Error)]
        [InlineData((int)LogLevel.Critical)]
        public void SetLevel(int level)
        {
            Assert.False(_logger.GetLevel(level));
            _logger.SetLevel(level);
            Assert.True(_logger.GetLevel(level));
            _logger.UnsetLevel(level);
            Assert.False(_logger.GetLevel(level));
        }
        */
    }
}
