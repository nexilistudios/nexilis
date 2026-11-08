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

#ifndef NEXILIS_UNIX_SOCKET_HH
#define NEXILIS_UNIX_SOCKET_HH

#ifdef __linux__
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace nexilis::unix_socket
{
/// Filesystem sockets only: reserve room for the terminator and reject NULs.
inline sockaddr_un address(const std::string& path)
{
    sockaddr_un result{};
    if (path.empty() || path.size() >= sizeof(result.sun_path) ||
        path.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid Unix socket path");
    result.sun_family = AF_UNIX;
    std::memcpy(result.sun_path, path.c_str(), path.size() + 1);
    return result;
}

/// Never unlink an existing path: a second server must not replace a live one
/// or delete an unrelated file. The caller closes fd on failure.
inline void bindPrivate(int fd, const std::string& path)
{
    const auto endpoint = address(path);
    if (bind(fd, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) == -1)
        throw std::system_error(errno, std::generic_category(), "Unix socket bind failed");
    if (chmod(path.c_str(), S_IRUSR | S_IWUSR) == -1)
    {
        const auto error = errno;
        unlink(path.c_str());
        throw std::system_error(error, std::generic_category(), "Unix socket chmod failed");
    }
}

inline bool sameUser(int fd)
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);
    return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0 &&
           length == sizeof(credentials) && credentials.uid == geteuid();
}
/// Datagram credentials are supplied by the kernel with SO_PASSCRED enabled.
inline bool sameUser(msghdr& message)
{
    if (message.msg_flags & (MSG_CTRUNC | MSG_TRUNC))
        return false;
    for (auto* header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header))
    {
        if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_CREDENTIALS &&
            header->cmsg_len >= CMSG_LEN(sizeof(ucred)))
        {
            ucred credentials{};
            std::memcpy(&credentials, CMSG_DATA(header), sizeof(credentials));
            return credentials.uid == geteuid();
        }
    }
    return false;
}
} // namespace nexilis::unix_socket
#endif
#endif
