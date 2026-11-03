//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
//                                                                         //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "SocketWinsock.h"
#if !defined(JOURNEY_USE_ASIO) && defined(MS_PLATFORM_SDL)

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace jrc
{
    namespace
    {
        constexpr int INVALID_FD = -1;

        int fd_of(uint64_t sock)
        {
            return static_cast<int>(static_cast<int64_t>(sock));
        }

        // Blocks until exactly 'length' bytes are read. Used for the handshake only.
        bool read_exact(int fd, int8_t* bytes, size_t length)
        {
            size_t done = 0;

            while (done < length)
            {
                ssize_t result = ::recv(fd, bytes + done, length - done, 0);

                if (result > 0)
                {
                    done += static_cast<size_t>(result);
                }
                else if (result < 0 && errno == EINTR)
                {
                    continue;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        // Size of the packet that starts at 'bytes', header included. The
        // header is two 16-bit words whose XOR is the body length, the same
        // rule Cryptography::check_length applies.
        size_t packet_size(const int8_t* bytes)
        {
#ifdef JOURNEY_USE_CRYPTO
            uint16_t low = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8);
            uint16_t high = static_cast<uint8_t>(bytes[2]) | (static_cast<uint8_t>(bytes[3]) << 8);
            size_t body = static_cast<uint16_t>(low ^ high);
#else
            size_t body = 0;

            for (size_t i = 0; i < HEADER_LENGTH; ++i)
            {
                body += static_cast<size_t>(static_cast<uint8_t>(bytes[i])) << (8 * i);
            }
#endif
            return HEADER_LENGTH + body;
        }
    }

    bool SocketWinsock::open(const char* iaddr, const char* port)
    {
        sock = static_cast<uint64_t>(static_cast<int64_t>(INVALID_FD));
        pending_length = 0;

        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* addr_info = nullptr;

        if (::getaddrinfo(iaddr, port, &hints, &addr_info) != 0)
        {
            return false;
        }

        int fd = INVALID_FD;

        for (addrinfo* ptr = addr_info; ptr != nullptr; ptr = ptr->ai_next)
        {
            fd = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

            if (fd == INVALID_FD)
            {
                continue;
            }

            if (::connect(fd, ptr->ai_addr, ptr->ai_addrlen) == 0)
            {
                break;
            }

            ::close(fd);
            fd = INVALID_FD;
        }

        ::freeaddrinfo(addr_info);

        if (fd == INVALID_FD)
        {
            return false;
        }

        int nodelay = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        // The handshake is a 16-bit length followed by that many bytes.
        bool handshake_ok = read_exact(fd, buffer, 2);
        size_t handshake_body = 0;

        if (handshake_ok)
        {
            handshake_body = static_cast<uint8_t>(buffer[0]) | (static_cast<uint8_t>(buffer[1]) << 8);
            handshake_ok = 2 + handshake_body == HANDSHAKE_LEN &&
                read_exact(fd, buffer + 2, handshake_body);
        }

        if (!handshake_ok)
        {
            ::close(fd);
            return false;
        }

        // Game packets are polled every frame, so reads must not block.
        int flags = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        sock = static_cast<uint64_t>(static_cast<int64_t>(fd));
        return true;
    }

    bool SocketWinsock::close()
    {
        int fd = fd_of(sock);
        sock = static_cast<uint64_t>(static_cast<int64_t>(INVALID_FD));
        pending_length = 0;

        return fd != INVALID_FD && ::close(fd) == 0;
    }

    bool SocketWinsock::dispatch(const int8_t* bytes, size_t length) const
    {
        int fd = fd_of(sock);
        size_t done = 0;

        while (done < length)
        {
            ssize_t result = ::send(fd, bytes + done, length - done, MSG_NOSIGNAL);

            if (result > 0)
            {
                done += static_cast<size_t>(result);
            }
            else if (result < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            {
                // The socket is non-blocking. Outgoing packets are small, so
                // spinning briefly on a full send buffer is acceptable.
                continue;
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    size_t SocketWinsock::receive(bool* success)
    {
        int fd = fd_of(sock);

        if (pending_length < MAX_PACKET_LENGTH)
        {
            ssize_t result = ::recv(fd, pending + pending_length, MAX_PACKET_LENGTH - pending_length, 0);

            if (result > 0)
            {
                pending_length += static_cast<size_t>(result);
            }
            else if (result == 0)
            {
                // The server closed the connection.
                *success = false;
                return 0;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                *success = false;
                return 0;
            }
        }

        // Session expects whole packets, so hand over only complete ones and
        // keep a partial packet for the next call. TCP may split anywhere.
        size_t complete = 0;

        while (pending_length - complete >= HEADER_LENGTH)
        {
            size_t size = packet_size(pending + complete);

            if (size > MAX_PACKET_LENGTH)
            {
                *success = false;
                return 0;
            }

            if (pending_length - complete < size)
            {
                break;
            }

            complete += size;
        }

        if (complete == 0)
        {
            return 0;
        }

        std::memcpy(buffer, pending, complete);
        std::memmove(pending, pending + complete, pending_length - complete);
        pending_length -= complete;

        return complete;
    }

    const int8_t* SocketWinsock::get_buffer() const
    {
        return buffer;
    }
}
#endif
