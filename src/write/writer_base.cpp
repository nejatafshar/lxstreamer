/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "writer_base.hpp"

namespace lxstreamer {

void
rescale_remux(AVPacket* pkt, const AVRational& in, const AVRational& out) {
    if (pkt->pts != AV_NOPTS_VALUE)
        pkt->pts = av_rescale_q_rnd(
            pkt->pts,
            in,
            out,
            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    if (pkt->dts != AV_NOPTS_VALUE)
        pkt->dts = av_rescale_q_rnd(
            pkt->dts,
            in,
            out,
            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    if (pkt->duration > 0)
        pkt->duration = av_rescale_q(pkt->duration, in, out);
}

int
writer_base::write_packet(const AVPacket* p) {
    if (last_write_time.seconds() > 15)
        return AVERROR(ETIMEDOUT);

    auto in_idx  = p->stream_index;
    auto out_idx = out_stream_map[in_idx];
    if (out_idx == -1)
        return 0;

    AVRational in_time_base;
    auto       out_stream = output->streams[out_idx];
    if (sd) {
        auto in_stream = sd->input_ctx->streams[in_idx];
        in_time_base   = in_stream->time_base;
        if (out_stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            is_valid(sd->view_encoding.audio))
            in_time_base = p->time_base;
    } else
        in_time_base = p->time_base;

    packet_ref pkt{p};
    pkt->stream_index = out_idx;

    // set timestamps
    if (type == writer_type::view) {
        av_packet_rescale_ts(pkt.get(), in_time_base, out_stream->time_base);
    } else if (type == writer_type::record) {
        auto flag =
            pkt->pts >= 0 && pkt->dts == AV_NOPTS_VALUE && pkt->duration == 0;
        if (!flag) {
            rescale_remux(pkt.get(), in_time_base, out_stream->time_base);
            if (first_ptses[in_idx] == -1)
                first_ptses[in_idx] = pkt->pts;
            pkt->pts -= first_ptses[in_idx];
            pkt->dts = AV_NOPTS_VALUE;
            if (pkt->pts < 0)
                pkt->pts = 0;
        }
    }

    if (pkt->dts != AV_NOPTS_VALUE && pkt->dts <= last_dtses[in_idx])
        pkt->dts = last_dtses[in_idx] + 1;
    if (pkt->dts != AV_NOPTS_VALUE && pkt->pts < pkt->dts)
        pkt->pts = pkt->dts;
    last_dtses[in_idx] = pkt->dts;

    // write
    auto ret = av_interleaved_write_frame(output.get(), pkt.get());
    pkt.unref();

    last_write_time.start();
    return ret;
}

bool
writer_base::make_output_streams() {
    out_stream_map.fill(-1);
    first_ptses.fill(-1);
    last_dtses.fill(-1);

    size_t stream_count = 0;
    if (sd)
        stream_count = sd->input_ctx->nb_streams;
    else
        stream_count = 1; // no input

    int out_stream_counter{0};
    for (size_t i = 0; i < stream_count && i < max_streams; ++i) {
        const auto* in_codecpar =
            sd ? sd->input_ctx->streams[i]->codecpar : nullptr;
        if (in_codecpar && in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;

        out_stream_map[i] = out_stream_counter++;
        auto* stream      = avformat_new_stream(output.get(), nullptr);
        if (!stream) {
            logFatal(
                "failed to create output stream: src: %s  stream: %d",
                sd->iargs.name,
                i);
            return false;
        }

        if (in_codecpar &&
            ((!is_valid(sd->view_encoding.video) &&
              in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
             (!is_valid(sd->view_encoding.audio) &&
              in_codecpar->codec_type ==
                  AVMEDIA_TYPE_AUDIO))) { // only if the stream must be
            // remuxed (no re-encoding)
            int nret = avcodec_parameters_copy(stream->codecpar, in_codecpar);
            if (nret < 0) {
                logFatal(
                    "failed to copy input codec parameters to output stream: "
                    "src: %s stream: %d",
                    sd->iargs.name,
                    i);
                return false;
            }
            stream->codecpar->codec_tag = 0;
            stream->start_time          = 0;
        } else {
            auto context = sd->iencoder.context(
                !in_codecpar || in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO
                    ? sd->view_encoding.video
                    : sd->view_encoding.audio);
            int ret =
                avcodec_parameters_from_context(stream->codecpar, context);
            if (ret < 0) {
                logFatal(
                    "failed to copy encoder codec parameters to output stream: "
                    "src: %s stream: %d",
                    sd->iargs.name,
                    i);
                return false;
            }

            stream->time_base = context->time_base;
        }
    }

    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_AUTO;
    return true;
}

codec_t
writer_base::alternate_proper_audio_codec() {
    if (!sd)
        return codec_t::unknown;
    auto& fctx = sd->input_ctx;
    for (size_t i = 0; i < fctx->nb_streams && i < max_streams; ++i) {
        if (fctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;
        // only add the codec if it can be used with this container
        if (avformat_query_codec(
                output->oformat,
                fctx->streams[i]->codecpar->codec_id,
                FF_COMPLIANCE_NORMAL) == 1) {
            return codec_t::unknown;
        }
    }
    for (auto c = codec_t::ac3; c <= codec_t::aac;
         c      = static_cast<codec_t>(static_cast<int>(c) + 1)) {
        if (avformat_query_codec(
                output->oformat,
                get_encoder(c)->id,
                FF_COMPLIANCE_EXPERIMENTAL) == 1) {
            return c;
        }
    }
    return codec_t::unknown;
}

} // namespace lxstreamer
