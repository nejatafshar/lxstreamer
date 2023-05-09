/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef FFMPEG_TYPES_HPP
#define FFMPEG_TYPES_HPP

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

#include "utils.hpp"

#include <memory>
#include <string>
#include <system_error>

namespace lxstreamer {

// ffmpeg error
//-----------------------------------------------------------------------------

struct ffmpeg_error_category : public std::error_category {
    ~ffmpeg_error_category() override = default;

    const char* name() const noexcept override {
        return "ffmpeg";
    }

    std::string message(int err) const override {
        char buff[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(err, &buff[0], AV_ERROR_MAX_STRING_SIZE);
        return std::string{buff};
    }

    static auto instance() -> const error_category& {
        static ffmpeg_error_category c;
        return c;
    }
};

//-----------------------------------------------------------------------------

inline std::string
ffmpeg_make_error_string(int err) {
    return ffmpeg_error_category::instance().message(err);
}

inline std::error_code
ffmpeg_make_err(int err) {
    return std::error_code{err, ffmpeg_error_category::instance()};
}

// base types
//-----------------------------------------------------------------------------

template <class T, class Impl> struct guard_value {
    guard_value() noexcept = default;
    explicit guard_value(T* p) noexcept : ptr{p} {}
    explicit guard_value(T& r) noexcept : ptr{&r} {}

    ~guard_value() {
        reset();
    }

    operator bool() const noexcept {
        return ptr != nullptr;
    }

    const T* get() const noexcept {
        return ptr;
    }
    T* get() noexcept {
        return ptr;
    }

    const T* operator->() const noexcept {
        return *ptr;
    }
    T* operator->() noexcept {
        return *ptr;
    }

    void reset() noexcept {
        if (ptr) {
            static_cast<Impl&>(*this).unref();
            ptr = nullptr;
        }
    }

    guard_value(const guard_value&) = delete;
    guard_value(guard_value&&)      = delete;
    guard_value& operator=(const guard_value&) = delete;
    guard_value& operator=(guard_value&&) = delete;

protected:
    T* ptr = nullptr;
};

template <class T, class Impl> struct ref_value {
    ~ref_value() {
        static_cast<Impl&>(*this).unref();
    }

    const T* get() const noexcept {
        return &value;
    }

    T* get() noexcept {
        return &value;
    }

    const T* operator->() const noexcept {
        return &value;
    }

    T* operator->() noexcept {
        return &value;
    }

    const T& operator*() const noexcept {
        return value;
    }

    T& operator*() noexcept {
        return value;
    }

    ref_value(const ref_value&) = delete;
    ref_value(ref_value&&)      = delete;
    ref_value& operator=(const ref_value&) = delete;
    ref_value& operator=(ref_value&&) = delete;

protected:
    ref_value() = default;
    T value{};
};

struct deleter {
    void operator()(AVFormatContext* ctx) noexcept {
        ctx->interrupt_callback.callback = nullptr;
        ctx->interrupt_callback.opaque   = nullptr;
        if (ctx->iformat != nullptr) {
            // also calls avformat_free_context() internally
            avformat_close_input(&ctx);
            return;
        }
        if (ctx->oformat != nullptr) {
            if (ctx->pb && !(ctx->oformat->flags & AVFMT_NOFILE))
                avio_closep(&ctx->pb);
        }
        avformat_free_context(ctx);
    }

    void operator()(AVCodecContext* ctx) noexcept {
        avcodec_free_context(&ctx);
    }

    void operator()(AVFrame* fr, bool unref) noexcept {
        if (fr == nullptr)
            return;
        if (unref)
            av_frame_unref(fr);
        else
            av_frame_free(&fr);
        fr = nullptr;
    }

    void operator()(AVFrame* pkt) noexcept {
        operator()(pkt, true);
    }

    void operator()(SwsContext* ctx) noexcept {
        sws_freeContext(ctx);
    }

    void operator()(AVDictionary* dic) noexcept {
        av_dict_free(&dic);
    }

    void operator()(AVPacket* pkt, bool unref) noexcept {
        if (pkt == nullptr)
            return;
        if (unref)
            av_packet_unref(pkt);
        else
            av_packet_free(&pkt);
        pkt = nullptr;
    }

    void operator()(AVPacket* pkt) noexcept {
        operator()(pkt, true);
    }

    void operator()(AVIOContext* ctx, bool custom = false) noexcept {
        if (!custom) {
            // the internal buffer of AVIOContext will be freed by avio_close
            avio_close(ctx);
        } else {
            if (ctx->buffer)
                av_freep(&ctx->buffer);
            avio_context_free(&ctx);
        }
    }

    void operator()(AVFilterInOut* f) noexcept {
        avfilter_inout_free(&f);
    }

    void operator()(AVFilterGraph* fg) noexcept {
        avfilter_graph_free(&fg);
    }
};

// packet types
//-----------------------------------------------------------------------------

struct packet : guard_value<AVPacket, packet> {
    packet() noexcept {
        ptr = av_packet_alloc();
    }

    void unref() noexcept {
        deleter{}(ptr, false);
    }
};

struct packet_ref : ref_value<AVPacket, packet_ref> {
    explicit packet_ref(const AVPacket* src) noexcept {
        if (av_packet_ref(&value, src) != 0)
            unref();
    }

    void unref() noexcept {
        deleter{}(&value, true);
    }
};

// frame types
//-----------------------------------------------------------------------------

struct frame : guard_value<AVFrame, frame> {
    frame() noexcept {
        ptr = av_frame_alloc();
    }

    frame(frame&& f) noexcept {
        ptr = av_frame_alloc();
        av_frame_unref(ptr);
        av_frame_move_ref(ptr, f.get());
    }

    void realloc() noexcept {
        unref();
        ptr = av_frame_alloc();
    }

    void unref() noexcept {
        deleter{}(ptr, false);
    }
};

struct frame_ref : ref_value<AVFrame, frame_ref> {
    explicit frame_ref(const AVFrame* src) noexcept {
        if (!src)
            return;
        if (av_frame_ref(&value, src) != 0)
            unref();
    }

    frame_ref(const frame_ref& f) noexcept {
        *this = f;
    }

    frame_ref& operator=(const AVFrame* f) noexcept {
        unref();
        if (av_frame_ref(&value, f) != 0)
            unref();
        return *this;
    }

    frame_ref& operator=(const frame_ref& f) noexcept {
        *this = f.get();
        return *this;
    }

    frame_ref& operator=(frame_ref&& f) noexcept {
        *this = f;
        f.unref();
        return *this;
    }

    void unref() noexcept {
        deleter{}(&value, true);
    }
};

// filter graph types
//-----------------------------------------------------------------------------

struct filter_graph : guard_value<AVFilterGraph, filter_graph> {
    filter_graph() noexcept {
        ptr = avfilter_graph_alloc();
    }

    void realloc() noexcept {
        unref();
        ptr = avfilter_graph_alloc();
    }

    void unref() noexcept {
        deleter{}(ptr);
    }
};

//  parse a filtergraph description returns a corresponding textual
//  representation in the dot language
inline std::string
get_digraph(AVFilterGraph* graph) {
    unsigned int i, j;

    std::string str;

    str += "digraph G {\n";
    str += "node [shape=box]\n";
    str += "rankdir=LR\n";

    for (i = 0; i < graph->nb_filters; i++) {
        char                   filter_ctx_label[128];
        const AVFilterContext* filter_ctx = graph->filters[i];

        snprintf(
            filter_ctx_label,
            sizeof(filter_ctx_label),
            "%s\\n(%s)",
            filter_ctx->name,
            filter_ctx->filter->name);

        for (j = 0; j < filter_ctx->nb_outputs; j++) {
            AVFilterLink* link = filter_ctx->outputs[j];
            if (link) {
                char                   dst_filter_ctx_label[128];
                const AVFilterContext* dst_filter_ctx = link->dst;

                snprintf(
                    dst_filter_ctx_label,
                    sizeof(dst_filter_ctx_label),
                    "%s\\n(%s)",
                    dst_filter_ctx->name,
                    dst_filter_ctx->filter->name);

                str += format_string(
                    "\"%s\" -> \"%s\" [ label= \"inpad:%s -> outpad:%s\\n",
                    filter_ctx_label,
                    dst_filter_ctx_label,
                    avfilter_pad_get_name(link->srcpad, 0),
                    avfilter_pad_get_name(link->dstpad, 0));

                if (link->type == AVMEDIA_TYPE_VIDEO) {
                    const AVPixFmtDescriptor* desc =
                        av_pix_fmt_desc_get(AVPixelFormat(link->format));
                    str += format_string(
                        "fmt:%s w:%d h:%d tb:%d/%d",
                        desc->name,
                        link->w,
                        link->h,
                        link->time_base.num,
                        link->time_base.den);
                } else if (link->type == AVMEDIA_TYPE_AUDIO) {
                    char buf[255];
                    av_channel_layout_describe(
                        &link->ch_layout, buf, sizeof(buf));
                    str += format_string(
                        "fmt:%s sr:%d cl:%s tb:%d/%d",
                        av_get_sample_fmt_name(AVSampleFormat(link->format)),
                        link->sample_rate,
                        buf,
                        link->time_base.num,
                        link->time_base.den);
                }
                str += "\" ];\n";
            }
        }
    }
    str += "}\n";
    return str;
}

// memory
//-----------------------------------------------------------------------------

class dictionary
{
    AVDictionary* ptr{nullptr};

public:
    ~dictionary() {
        reset();
    }

    void reset() {
        if (ptr) {
            deleter{}(ptr);
            ptr = nullptr;
        }
    }

    const AVDictionary* get() const noexcept {
        return ptr;
    }

    AVDictionary* get() noexcept {
        return ptr;
    }

    AVDictionary** ref() noexcept {
        return &ptr;
    }

    int set(const char* key, const char* value, int flags = 0) {
        return av_dict_set(&ptr, key, value, flags);
    }

    template <typename T>
    std::enable_if_t<std::is_integral<T>::value, int>
    set(const char* key, T value, int flags = 0) {
        return av_dict_set_int(&ptr, key, static_cast<int64_t>(value), flags);
    }
};

//-----------------------------------------------------------------------------

inline std::string
to_string(const AVDictionary* dict) {
    char*       buffer = nullptr;
    int         ret    = av_dict_get_string(dict, &buffer, ':', ',');
    std::string str;
    if (ret >= 0 && buffer) {
        str = buffer;
        av_freep(&buffer);
    }
    return str;
}

inline std::string
to_string(const dictionary& dict) {
    return to_string(dict.get());
}


// memory
//-----------------------------------------------------------------------------
template <class T> using unique_ptr = std::unique_ptr<T, deleter>;

} // namespace lxstreamer

#endif // FFMPEG_TYPES_HPP
