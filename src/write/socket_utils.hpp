/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef SOCKET_UTILS_HPP
#define SOCKET_UTILS_HPP

#include "utils.hpp"

#include "openssl/err.h"
#include "openssl/ssl.h"

namespace lxstreamer {

inline constexpr int
ensure_negative(int err) noexcept {
    return err < 0 ? err : -err;
}

#if defined(__unix__) || defined(__MACH__)

constexpr const sock_t InvalidSocket{static_cast<sock_t>(-1)};

inline bool
close(sock_t s) {
    return s == InvalidSocket || ::close(static_cast<int>(s)) == 0;
}

inline auto
send(sock_t s, const void* data, size_t len) {
    constexpr const int flags =
#if defined(MSG_NOSIGNAL)
        MSG_NOSIGNAL; // prevent SIGPIPE on linux
#else
        0;
#endif
    return ::send(s, data, len, flags);
}

inline bool
set_blocking(sock_t& sock) {
    const auto flags = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, flags ^ O_NONBLOCK) != -1;
}

#elif defined(_WIN32)

constexpr const sock_t InvalidSocket{static_cast<sock_t>(INVALID_SOCKET)};

inline bool
close(sock_t s) {
    return s == InvalidSocket || ::closesocket(s) == 0;
}

inline auto
send(sock_t s, const void* data, size_t len) {
    return ::send(
        s, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
}

inline bool
set_blocking(sock_t& s) {
    unsigned long flag = 0;
    return ioctlsocket(s, FIONBIO, &flag) == 0;
}

#endif

inline int
write(sock_t to, const char* data, size_t length) noexcept {
    const char* p = data;
    for (auto left = length; left > 0;) {
        auto ret = send(to, p, left);
#if defined(__unix__) || defined(__MACH__)
        if (ret == ssize_t(-1))
            return ensure_negative(errno);
#elif defined(_WIN32)
        if (ret == SOCKET_ERROR)
            return ensure_negative(EPIPE);
#endif
        p += ret;
        left -= ret;
    }
    return static_cast<int>(length);
}

inline int
write(SSL* to, const char* data, size_t length) noexcept {
    const char* p = data;
    for (auto left = length; left > 0;) {
        auto ret = SSL_write(to, data, static_cast<int>(length));
        if (ret <= 0) {
            SSL_get_error(to, ret);
            ERR_clear_error();
            return ensure_negative(EPIPE);
        }
        p += ret;
        left -= ret;
    }
    return static_cast<int>(length);
}

} // namespace lxstreamer

#endif // SOCKET_UTILS_HPP
