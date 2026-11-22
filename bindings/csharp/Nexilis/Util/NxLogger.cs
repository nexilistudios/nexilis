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
using System.Threading;

namespace Nexilis
{

public class NxLogger : IDisposable
{
    // Nexilis logger.
    readonly Logger.Logger _logger = new Logger.Logger();

    // Nexilis logger function handler.
    Logger.FunctionHandler _functionHandler;

    // Logging callback.
    Action<Logger.LogLevel, string>? _logCallback;

    readonly ReaderWriterLockSlim _lock = new ReaderWriterLockSlim();
    readonly object _handlerLock = new object();
    bool _disposed = false;
    string _logContext;

    public NxLogger(string logContext)
    {
        _logContext = logContext;
        _functionHandler = new Logger.FunctionHandler(Logger.FunctionHandler.EmptyFunction);
    }

    public void AddHandler(Action<Logger.LogLevel, string> logFunction)
    {
        if (logFunction == null) throw new ArgumentNullException(nameof(logFunction));
        lock (_handlerLock)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
            Action<Logger.LogLevel, string> logCb = (level, msg) =>
            {
                SafeInvokeLogCallback(level, msg);
            };
            var replacement = new Logger.FunctionHandler(logCb);
            try
            {
                _logger.AddFunctionHandler(replacement);
            }
            catch
            {
                replacement.Dispose();
                throw;
            }

            var previous = _functionHandler;
            _functionHandler = replacement;
            _logCallback = logFunction;
            if (!previous.isEmpty)
                _logger.RemoveHandler(previous.GetId());
            previous.Dispose();
        }
    }

    public NxLogger Setup(Action<Logger.LogLevel, string> logFunction)
    {
        AddHandler(logFunction);
        SetMinimumLevel(Logger.LogLevel.DEBUG);
        return this;
    }

    public void Debug(string message)
    {
        _logger.Debug(AddLogContext(message));
    }

    public void Info(string message)
    {
        _logger.Info(AddLogContext(message));
    }

    public void Warning(string message)
    {
        _logger.Warning(AddLogContext(message));
    }

    public void Error(string message)
    {
        _logger.Error(AddLogContext(message));
    }

    public void Critical(string message)
    {
        _logger.Critical(AddLogContext(message));
    }

    private string AddLogContext(string message)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        if (_functionHandler.isEmpty) throw new InvalidOperationException("Function handler is empty.");
        if (!string.IsNullOrEmpty(_logContext))
        {
            return $"{_logContext}: {message}";
        }
        return message;
    }

    public void ClearHandlers()
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        _logger?.ClearHandlers();
    }

    public void RemoveHandler(ulong handlerId)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        _logger?.RemoveHandler(handlerId);
    }

    public bool NoHandlers()
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        return _logger?.NoHandlers() ?? false;
    }

    public void SetMinimumLevel(Logger.LogLevel level)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        _logger.SetMinimumLevel((int)level);
    }

    public bool UnsetLevel(int level)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        return _logger?.UnsetLevel(level) ?? false;
    }

    public bool SetLevel(int level)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        return _logger?.SetLevel(level) ?? false;
    }

    public bool GetLevel(int level)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        return _logger?.GetLevel(level) ?? false;
    }

    public void SetLogLevel(byte level)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));
        _logger?.SetLogLevel(level);
    }

    public void Dispose()
    {
        lock (_handlerLock)
        {
            if (_disposed) return;
            if (!_functionHandler.isEmpty)
                _logger.RemoveHandler(_functionHandler.GetId());
            _functionHandler.Dispose();
            _logger.Dispose();
            _logCallback = null;
            _disposed = true;
        }
    }

    private void SafeInvokeLogCallback(Logger.LogLevel level, string message)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(NxLogger));

        _lock.EnterReadLock();
        try
        {
            _logCallback?.Invoke(level, message);
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }
}

}
