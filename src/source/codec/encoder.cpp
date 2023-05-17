/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "encoder.hpp"
#include "../source_data.hpp"
#include "utils.hpp"

extern "C" {
#include <libavutil/opt.h>
}

namespace lxstreamer {

int
calc_width(int src_w, int src_h, int dest_h) {
    auto dest_w =
        int((static_cast<double>(src_w) / static_cast<double>(src_h)) * dest_h);
    if (dest_w % 2 == 1) // check even
        --dest_w;
    return dest_w;
}

int
height_for_bitrate(size_t bitrate) {
    if (bitrate >= 16000)
        return 2160;
    else if (bitrate >= 4000)
        return 1080;
    else if (bitrate >= 2000)
        return 720;
    else if (bitrate >= 1000)
        return 480;
    else if (bitrate >= 500)
        return 360;
    else if (bitrate >= 250)
        return 240;
    else if (bitrate >= 120)
        return 144;
    else
        return 90;
}

void
init_resolution(encoding_t& enc, int input_width, int input_height) {
    input_width  = input_width > 0 ? input_width : 15360;
    input_height = input_height > 0 ? input_height : 8640;

    auto out_height = std::min(
        enc.height > 0 ? enc.height : height_for_bitrate(enc.max_bandwidth),
        input_height);
    if (out_height % 2 == 1) // check even
        --out_height;
    auto out_width = calc_width(input_width, input_height, out_height);
    enc.width      = out_width;
    enc.height     = out_height;
}

const AVCodec*
get_encoder(codec_t codec) {
    const AVCodec* enc = nullptr;
    if (codec == codec_t::h264) {
#if defined(_WIN32)
        enc = avcodec_find_encoder_by_name("h264_mf");
#endif
        if (!enc)
            enc = avcodec_find_encoder_by_name("libx264");
    } else if (codec == codec_t::hevc) {
#if defined(_WIN32)
        // enc = avcodec_find_encoder_by_name("hevc_mf");
#endif
        if (!enc)
            enc = avcodec_find_encoder_by_name("libx265");
    } else if (codec == codec_t::av1) {
        enc = avcodec_find_encoder_by_name("libsvtav1");
        if (!enc)
            enc = avcodec_find_encoder_by_name("librav1e");
        if (!enc)
            enc = avcodec_find_encoder_by_name("libaom-av1");
    } else if (codec == codec_t::vp9)
        enc = avcodec_find_encoder_by_name("libvpx-vp9");
    else if (codec == codec_t::ac3) {
        enc = avcodec_find_encoder_by_name("ac3");
        if (!enc)
            enc = avcodec_find_encoder_by_name("ac3_fixed");
    } else if (codec == codec_t::mp2) {
        enc = avcodec_find_encoder_by_name("mp2");
        if (!enc)
            enc = avcodec_find_encoder_by_name("mp2fixed");
        if (!enc)
            enc = avcodec_find_encoder_by_name("libtwolame");
    } else if (codec == codec_t::mp3) {
        enc = avcodec_find_encoder_by_name("libshine");
        if (!enc)
            enc = avcodec_find_encoder_by_name("libmp3lame");
    } else if (codec == codec_t::aac) {
        enc = avcodec_find_encoder_by_name("aac");
    }
    return enc;
}

/* check that a given sample format is supported by the encoder */
int
check_sample_fmt(const AVCodec* codec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat* p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

/* select preferred samplerate if supported otherwise select some other one */
int
select_sample_rate(const AVCodec* codec, int preferred_sample_rate) {
    if (preferred_sample_rate <= 0)
        preferred_sample_rate = 44100;
    if (!codec->supported_samplerates)
        return 44100;
    const int* p;
    int        best_samplerate = 0;
    p                          = codec->supported_samplerates;
    while (*p) {
        if (*p == preferred_sample_rate)
            return preferred_sample_rate;
        else if (*p > preferred_sample_rate && *p == 44100)
            return *p;
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

/* select preferred layout if supported  otherwise select select the one with
 * the highest channel count */
int
select_channel_layout(
    const AVCodec*   codec,
    AVChannelLayout* dst,
    AVChannelLayout* preferred_layout) {
    const AVChannelLayout *p, *best_ch_layout;
    if (!codec->ch_layouts) {
        AVChannelLayout l{AV_CHANNEL_ORDER_NATIVE, 2, {AV_CH_LAYOUT_STEREO}};
        return av_channel_layout_copy(dst, &l);
    }
    int best_nb_channels = 0;
    p                    = codec->ch_layouts;
    while (p->nb_channels) {
        if (p->order == preferred_layout->order &&
            p->nb_channels == preferred_layout->nb_channels)
            return av_channel_layout_copy(dst, p);
        if (p->nb_channels > best_nb_channels) {
            best_ch_layout   = p;
            best_nb_channels = p->nb_channels;
        }
        p++;
    }
    return av_channel_layout_copy(dst, best_ch_layout);
}

namespace {

struct encoder_struct {
    using codec_context_t = unique_ptr<AVCodecContext>;
    const AVCodec*  encoder{nullptr};
    codec_context_t enc_ctx{nullptr};
    elapsed_timer   tt;
};

} // namespace

struct encoder::impl {

    const source_data&                             super;
    std::unordered_map<encoding_t, encoder_struct> encoders;
    std::mutex                                     mutex;

    explicit impl(const source_data& sup) : super{sup} {}

    ~impl() {}

    void set_encoder_video_settings(
        const encoding_t& config, AVCodecContext* codec_ctx);
    void set_encoder_audio_settings(
        const encoding_t& config, AVCodecContext* codec_ctx, AVCodec* codec);

    int encode_packets(
        AVCodecContext*        enc_ctx,
        const AVFrame*         frm,
        std::list<packet_ref>& packets);
};

void
encoder::impl::set_encoder_video_settings(
    const encoding_t& config, AVCodecContext* codec_ctx) {
    uint64_t maxBitrate = config.max_bandwidth * 1000;
    uint64_t bufSize    = maxBitrate * 2;
    auto     ret        = av_opt_set_int(
        codec_ctx,
        "b",
        maxBitrate / (super.is_webcam ? 4 : 2),
        AV_OPT_SEARCH_CHILDREN);
    if (ret != 0)
        logError(
            "failed setting encoder parameter <b> err:%d, %s",
            ret,
            ffmpeg_make_error_string(ret));

    ret = av_opt_set_int(
        codec_ctx, "maxrate", maxBitrate, AV_OPT_SEARCH_CHILDREN);
    if (ret != 0)
        logError(
            "failed setting encoder parameter <maxrate> err:%d, %s",
            ret,
            ffmpeg_make_error_string(ret));

    av_opt_set_int(codec_ctx, "minrate", 1000, AV_OPT_SEARCH_CHILDREN);
    if (ret != 0)
        logError(
            "failed setting encoder parameter <minrate> err:%d, %s",
            ret,
            ffmpeg_make_error_string(ret));

    ret = av_opt_set_int(codec_ctx, "bufsize", bufSize, AV_OPT_SEARCH_CHILDREN);
    if (ret != 0)
        logError(
            "failed setting encoder parameter <bufsize> err:%d, %s",
            ret,
            ffmpeg_make_error_string(ret));

    auto dec_ctx = super.idecoder.video_context();
    if (!dec_ctx)
        const_cast<source_data&>(super).idecoder.initialize(
            super.demux_data.video_stream.stream);
    dec_ctx = super.idecoder.video_context();
    if (!dec_ctx)
        return;

    codec_ctx->width  = config.width;
    codec_ctx->height = config.height;
    codec_ctx->sample_aspect_ratio =
        dec_ctx ? dec_ctx->sample_aspect_ratio : AVRational{0, 1};

    codec_ctx->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUV420P;
    /* video time_base can be set to whatever is handy and supported by
     * encoder
     */
    codec_ctx->time_base =
        dec_ctx
            ? av_inv_q(dec_ctx->framerate)
            : AVRational{AV_TIME_BASE / (config.frame_rate / 2), AV_TIME_BASE};
}

void
encoder::impl::set_encoder_audio_settings(
    const encoding_t& config, AVCodecContext* codec_ctx, AVCodec* codec) {
    auto dec_ctx = super.idecoder.audio_context();
    if (!dec_ctx)
        const_cast<source_data&>(super).idecoder.initialize(
            super.demux_data.audio_stream.stream);
    dec_ctx = super.idecoder.audio_context();
    if (!dec_ctx)
        return;

    auto prefered_sample_rate = config.sample_rate;
    if (prefered_sample_rate <= 0)
        prefered_sample_rate = dec_ctx->sample_rate;
    codec_ctx->sample_rate = select_sample_rate(codec, prefered_sample_rate);


    codec_ctx->ch_layout.nb_channels = 0;
    if (!config.channel_layout.empty()) {
        AVChannelLayout layout;
        av_channel_layout_channel_from_string(
            &layout, config.channel_layout.c_str());
        auto ret = select_channel_layout(codec, &codec_ctx->ch_layout, &layout);
        // if the prefered one in config was not compatible
        if (ret < 0 || layout.order != codec_ctx->ch_layout.order ||
            layout.nb_channels != codec_ctx->ch_layout.nb_channels)
            codec_ctx->ch_layout.nb_channels = 0;
    }
    if (codec_ctx->ch_layout.nb_channels == 0)
        if (auto ret = select_channel_layout(
                codec, &codec_ctx->ch_layout, &dec_ctx->ch_layout);
            ret < 0) {
            logError(
                "failed to select encoder audio channel layout err:%d, %s",
                ret,
                ffmpeg_make_error_string(ret));
        }

    codec_ctx->sample_fmt = AV_SAMPLE_FMT_NONE;
    if (auto sf = av_get_sample_fmt(config.sample_fmt.c_str());
        sf != AV_SAMPLE_FMT_NONE)
        if (check_sample_fmt(codec, sf))
            codec_ctx->sample_fmt = sf;
    if (codec_ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
        if (check_sample_fmt(codec, dec_ctx->sample_fmt))
            codec_ctx->sample_fmt = dec_ctx->sample_fmt;
    if (codec_ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
        codec_ctx->sample_fmt = codec->sample_fmts[0];

    codec_ctx->time_base = AVRational{1, codec_ctx->sample_rate};
}

int
encoder::impl::encode_packets(
    AVCodecContext*        enc_ctx,
    const AVFrame*         frm,
    std::list<packet_ref>& packets) {
    auto ret = avcodec_send_frame(enc_ctx, frm);
    if (ret < 0) {
        logError(
            "encoding failed: src: %s err: %d, %s",
            super.iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        return ret;
        return ret;
    }

    while (ret >= 0) {
        packet pkt;
        ret = avcodec_receive_packet(enc_ctx, pkt.get());
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            break;

        auto is_audio = enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO;

        auto dec_ctx = !is_audio ? super.idecoder.video_context()
                                 : super.idecoder.audio_context();
        if (!dec_ctx || is_audio) {
            pkt.get()->pts       = frm->pts;
            pkt.get()->dts       = pkt.get()->pts;
            pkt.get()->duration  = is_audio ? frm->duration : AV_NOPTS_VALUE;
            pkt.get()->time_base = frm->time_base;
        }

        if (is_audio)
            pkt.get()->stream_index = super.demux_data.audio_stream.stream_idx;

        packets.emplace_back(pkt.get());
    }
    return 0;
}

encoder::encoder(const source_data& s) : pimpl{std::make_unique<impl>(s)} {}

encoder::~encoder() {}

AVCodecContext*
encoder::context(const encoding_t& config) const {
    if (auto it = pimpl->encoders.find(config); it != pimpl->encoders.cend())
        return pimpl->encoders[config].enc_ctx.get();
    return nullptr;
}

int
encoder::initialize(const encoding_t& config, const AVFormatContext* octx) {
    std::scoped_lock lock{pimpl->mutex};
    if (pimpl->encoders.find(config) != pimpl->encoders.cend())
        return 0;
    encoder_struct enc;
    enc.encoder = get_encoder(config.codec);
    if (!enc.encoder) {
        logError("encoder not found: src: %s", pimpl->super.iargs.name);
        return AVERROR_INVALIDDATA;
    }
    auto* codec_ctx = avcodec_alloc_context3(enc.encoder);
    if (!codec_ctx) {
        logError(
            "failed to allocate encoder context: src: %s",
            pimpl->super.iargs.name);
        return AVERROR(ENOMEM);
    }

    if (is_video(config))
        pimpl->set_encoder_video_settings(config, codec_ctx);
    else
        pimpl->set_encoder_audio_settings(
            config, codec_ctx, const_cast<AVCodec*>(enc.encoder));

    if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    /* Third parameter can be used to pass settings to encoder */
    auto ret = avcodec_open2(codec_ctx, enc.encoder, nullptr);
    if (ret < 0) {
        logError(
            "failed opening encoder: src: %s err: %d, %s",
            pimpl->super.iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        return ret;
    }

    enc.enc_ctx.reset(codec_ctx);
    pimpl->encoders[config] = std::move(enc);

    return 0;
}

int
encoder::encode_packets(
    const encoding_t&      config,
    const AVFrame*         frm,
    std::list<packet_ref>& packets) {
    std::scoped_lock lock{pimpl->mutex};
    if (pimpl->encoders.find(config) == pimpl->encoders.cend())
        return AVERROR_ENCODER_NOT_FOUND;
    pimpl->encoders[config].tt.start();
    return pimpl->encode_packets(
        pimpl->encoders[config].enc_ctx.get(), frm, packets);
}

void
encoder::prune() {
    std::scoped_lock lock{pimpl->mutex};
    for (auto it = pimpl->encoders.begin(); it != pimpl->encoders.end();)
        if (it->second.tt.seconds() > 10)
            it = pimpl->encoders.erase(it);
        else
            ++it;
}

} // namespace lxstreamer
