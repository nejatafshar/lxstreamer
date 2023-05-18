/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "viewer_data.hpp"
#include "error_types.hpp"

namespace lxstreamer {

constexpr const char ResponseHeader[] = "HTTP/1.1 200 OK\r\n"
                                        "Server: lxstreamer/1.1\r\n"
                                        "Connection: Close\r\n"
                                        "Content-Type: video/mp4\r\n"
                                        "\r\n";

void
close_socket(sock_t& s) {
    if (s == InvalidSocket)
        return;
    while (!close(s))
        ;
    s = InvalidSocket;
}

int
write_sock_or_ssl(viewer_data& d, const char* data, size_t size) {
    if (d.sd->super.https)
        return write(d.ssl_ctx->ssl, data, size);
    else
        return write(d.write_sock, data, size);
}

int
send_data(viewer_data& d, const char* data, size_t size) {
    if (d.sd->super.https && (!d.ssl_ctx || !d.ssl_ctx->ssl))
        return ensure_negative(EPIPE);
    if (!d.header_sent.load(std::memory_order_relaxed)) {
        if (auto ret = write_sock_or_ssl(
                d, ResponseHeader, std::strlen(ResponseHeader));
            ret <= 0)
            return ret;
        d.header_sent = true;
    }
    return write_sock_or_ssl(d, data, size);
}

int
write_callback(void* opaque, uint8_t* buf, int size) {
    auto* d = reinterpret_cast<viewer_data*>(opaque);
    if (d == nullptr || size == 0 || buf == nullptr)
        return ensure_negative(EOF);

    auto ret = send_data(
        *d, reinterpret_cast<const char*>(buf), static_cast<size_t>(size));

    if (ret < 0)
        close_socket(d->write_sock);
    return ret;
}

std::string
to_string(const container_t& f) {
    if (f == container_t::matroska)
        return "matroska";
    else if (f == container_t::mpegts)
        return "mpegts";
    else if (f == container_t::flv)
        return "flv";
    return "";
}

viewer_data::viewer_data(const uri_data_t& ud, mg_connection* mc)
    : writer_base(writer_type::view), uri_data{ud}, connection{mc} {
    if (mc) {
        std::string buf(64, '\0');
        mg_sock_addr_to_str(
            &mc->sa,
            &buf.front(),
            buf.size(),
            MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
        buf.resize(std::strlen(buf.data()));
        address = buf;
    }
}

viewer_data::~viewer_data() {
    reset_io();
    if (io) {
        auto* _io = io.release();
        if (_io->opaque)
            _io->write_packet(_io->opaque, nullptr, 0);
        deleter{}(_io, true);
    }
    if (output)
        output->pb = nullptr;
}

std::error_code
viewer_data::init_io() {
    int  buf_size = 4096;
    auto buf      = reinterpret_cast<unsigned char*>(av_malloc(buf_size));
    auto ptr      = avio_alloc_context(
        buf, buf_size, 1, this, nullptr, write_callback, nullptr);
    if (ptr == nullptr) {
        logFatal(
            "viewer: failed to alloc avio context: src: %s", sd->iargs.name);
        return make_err(error_t::bad_state);
    }
    io.reset(ptr);

    write_sock = connection->sock;
    if (sd->super.https)
        ssl_ctx = reinterpret_cast<mg_ssl_if_ctx*>(connection->ssl_if_data);
    connection->sock = InvalidSocket;
    if (sd->super.https)
        connection->ssl_if_data = nullptr;
    connection->flags |= MG_F_CLOSE_IMMEDIATELY;

    set_blocking(write_sock);

    logTrace(
        "viewer client connected: src: %s addr: %s", sd->iargs.name, address);

    return std::error_code{};
}

void
viewer_data::reset_io() {
    if (io.get())
        io->opaque = nullptr;
    if (sd->super.https && ssl_ctx && ssl_ctx->ssl) {
        SSL_shutdown(ssl_ctx->ssl);
        close_socket(write_sock);
        if (ssl_ctx->ssl)
            SSL_free(ssl_ctx->ssl);
        mbuf_free(&ssl_ctx->psk);
        memset(ssl_ctx, 0, sizeof(*ssl_ctx));
        free(ssl_ctx);
        ssl_ctx = nullptr;
    } else if (write_sock != InvalidSocket)
        close_socket(write_sock);
}

bool
viewer_data::try_setup_output() {
    if (!sd)
        return false;
    AVFormatContext* octx{nullptr};
    auto             ret = avformat_alloc_output_context2(
        &octx, nullptr, to_string(sd->container).c_str(), nullptr);
    if (ret < 0 || !octx) {
        logFatal(
            "viewer: failed to alloc output context: src: %s err:%d, %s",
            sd->iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        return false;
    }

    octx->flags |=
        AVFMT_FLAG_GENPTS | AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_FLUSH_PACKETS;

    output.reset(octx);
    if (octx->pb)
        deleter{}(octx->pb);

    octx->pb = io.get();

    auto& conf       = sd->view_encoding;
    conf.audio.codec = codec_t::unknown;
    if (sd->container != container_t::matroska)
        if (auto codec = alternate_proper_audio_codec();
            codec != codec_t::unknown)
            conf.audio.codec = codec;

    if (is_valid(conf.video))
        if (sd->iencoder.initialize(conf.video, octx) != 0)
            return false;
    if (is_valid(conf.audio))
        if (sd->iencoder.initialize(conf.audio, octx) != 0)
            return false;

    if (!make_output_streams())
        return false;

    av_dict_set(&octx->metadata, "Streamer", "lxstreamer", 0);
    av_dict_set(
        &octx->metadata,
        "Copyright",
        "(C) 2022-present Nejat Afshar <nejatafshar@gmail.com>",
        0);
    av_dict_set(&octx->metadata, "Source", sd->iargs.name.c_str(), 0);

    ret = avformat_write_header(octx, nullptr);
    if (ret < 0) {
        logWarn(
            "viewer: failed to write header: src: %s container: %s err:%d, %s",
            sd->iargs.name,
            to_string(sd->container),
            ret,
            ffmpeg_make_error_string(ret));
        return false;
    }

    return true;
}

bool
viewer_data::setup_output() {
    if (!sd)
        return false;
    if (!sd->demux_data.demuxer_initialized)
        return false;

    std::list<container_t> formats{
        container_t::matroska, container_t::mpegts, container_t::flv};

    if (sd->iargs.container == container_t::unknown)
        sd->iargs.container = formats.front();

    // first try preferred format, if fails try others in order
    formats.erase(
        std::find(formats.begin(), formats.end(), sd->iargs.container));
    formats.emplace_front(sd->iargs.container);

    if (sd->container == container_t::unknown)
        sd->container = sd->iargs.container;

    if (try_setup_output())
        return true;
    else {
        // try next format
        auto it = std::find(formats.begin(), formats.end(), sd->container);
        if (it != formats.end())
            it++;
        if (it == formats.end())
            it = formats.begin();
        sd->container = *it;
        return false;
    }
    return false;
}

void
viewer_data::finalize() {
    if (sd && sd->container != container_t::flv)
        av_write_trailer(output.get());
}

} // namespace lxstreamer
