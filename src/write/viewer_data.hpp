/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef VIEWER_DATA_HPP
#define VIEWER_DATA_HPP

#include "socket_utils.hpp"
#include "writer_base.hpp"

namespace lxstreamer {

struct uri_data_t;

struct mg_ssl_if_ctx {
    SSL*        ssl;
    SSL_CTX*    ssl_ctx;
    struct mbuf psk;
    size_t      identity_len;
};

struct viewer_data : public writer_base {
    uri_data_t              uri_data;
    mg_connection*          connection{nullptr};
    unique_ptr<AVIOContext> io{nullptr};
    std::string             address;

    std::atomic_bool header_sent = false;
    sock_t           write_sock  = InvalidSocket;
    mg_ssl_if_ctx*   ssl_ctx     = nullptr;

    viewer_data(const uri_data_t& ud, mg_connection* mc);

    ~viewer_data();

    std::error_code init_io();
    void            reset_io();
    bool            try_setup_output();
    bool            setup_output();
    void            finalize();
};

} // namespace lxstreamer

#endif // VIEWER_DATA_HPP
