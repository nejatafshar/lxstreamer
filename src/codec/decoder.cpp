/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "decoder.hpp"
#include "../source_data.hpp"
#include "../utils.hpp"

namespace lxstreamer {

struct decoder::impl {

    const source_data&         super;
    std::mutex                 mutex;
    unique_ptr<AVCodecContext> ivid_decoder{nullptr};
    unique_ptr<AVCodecContext> iaud_decoder{nullptr};
    int64_t                    audio_rescale_last{AV_NOPTS_VALUE};
    elapsed_timer              elapsed;

    explicit impl(const source_data& sup) : super{sup} {}

    ~impl() {}
};

decoder::decoder(const source_data& s) : pimpl{std::make_unique<impl>(s)} {}

decoder::~decoder() {}

AVCodecContext*
decoder::video_context() const {
    return pimpl->ivid_decoder.get();
}

AVCodecContext*
decoder::audio_context() const {
    return pimpl->iaud_decoder.get();
}

int
decoder::initialize(const AVStream* stream) {
    std::scoped_lock lock{pimpl->mutex};
    auto             vid_stream = pimpl->super.demux_data.video_stream.stream;
    auto             aud_stream = pimpl->super.demux_data.audio_stream.stream;
    if ((stream == vid_stream && pimpl->ivid_decoder.get()) ||
        (stream == aud_stream && pimpl->iaud_decoder.get()))
        return 0;
    auto dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!dec) {
        // Failed to find decoder
        return AVERROR_DECODER_NOT_FOUND;
    }
    auto* codec_ctx = avcodec_alloc_context3(dec);
    if (!codec_ctx) {
        // Failed to allocate the decoder context
        return AVERROR(ENOMEM);
    }
    auto ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (ret < 0) {
        // Failed to copy decoder parameters to input decoder context
        return ret;
    }
    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
        codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            codec_ctx->framerate = av_guess_frame_rate(
                pimpl->super.input_ctx.get(),
                const_cast<AVStream*>(stream),
                nullptr);
        /* Open decoder */
        ret = avcodec_open2(codec_ctx, dec, nullptr);
        if (ret < 0) {
            // Failed to open decoder
            return ret;
        }
    }

    if (stream == vid_stream)
        pimpl->ivid_decoder.reset(codec_ctx);
    else
        pimpl->iaud_decoder.reset(codec_ctx);

    return 0;
}

int
decoder::decode_frames(const AVPacket* pkt, std::list<frame_ref>& frames) {
    if (!pkt)
        return AVERROR(EAGAIN);
    auto is_video =
        pkt->stream_index == pimpl->super.demux_data.video_stream.stream_idx;
    auto& dec    = is_video ? pimpl->ivid_decoder : pimpl->iaud_decoder;
    auto* stream = pimpl->super.input_ctx->streams[pkt->stream_index];
    if (!dec.get()) {
        if (auto ret = initialize(stream); ret != 0)
            return ret;
    }
    auto ret = avcodec_send_packet(dec.get(), pkt);
    if (ret < 0) {
        // Decoding failed
        return ret;
    }

    while (ret >= 0) {
        frame frm;
        ret = avcodec_receive_frame(dec.get(), frm.get());
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            break;
        else if (ret < 0)
            return ret;

        auto* f = frm.get();
        if (is_video) {
            f->pts       = f->best_effort_timestamp;
            f->time_base = stream->time_base;
        } else {
            if (pimpl->elapsed.elapsed() > std::chrono::seconds{5})
                pimpl->audio_rescale_last = AV_NOPTS_VALUE;
            pimpl->elapsed.start();
            AVRational decoded_frame_tb;
            if (f->pts != AV_NOPTS_VALUE) {
                decoded_frame_tb = stream->time_base;
            } else if (pkt->pts != AV_NOPTS_VALUE) {
                f->pts           = pkt->pts;
                decoded_frame_tb = stream->time_base;
            } else {
                f->pts           = pkt->dts;
                decoded_frame_tb = {1, AV_TIME_BASE};
            }
            if (f->pts != AV_NOPTS_VALUE)
                f->pts = av_rescale_delta(
                    decoded_frame_tb,
                    f->pts,
                    {1, f->sample_rate},
                    frm.get()->nb_samples,
                    &pimpl->audio_rescale_last,
                    {1, f->sample_rate});
            f->time_base = {1, f->sample_rate};
        }
        frames.emplace_back(frm.get());
    }
    return 0;
}

} // namespace lxstreamer
