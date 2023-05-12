/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "transcoder.hpp"
#include "../source_data.hpp"

namespace lxstreamer {

struct transcoder::impl {

    source_data&                                          super;
    const AVPacket*                                       ipacket{nullptr};
    const AVFrame*                                        iframe{nullptr};
    std::list<packet_ref>                                 unchanged;
    std::list<frame_ref>                                  frames;
    std::unordered_map<encoding_t, std::list<packet_ref>> packets;
    enum class packet_type { video = 0, audio = 1 } type{packet_type::video};

    explicit impl(source_data& sup, const AVPacket* pkt, const AVFrame* frm)
        : super{sup}, ipacket{pkt}, iframe{frm} {
        if (pkt) {
            unchanged.emplace_back(pkt);
            if (ipacket->stream_index ==
                super.demux_data.audio_stream.stream_idx)
                type = packet_type::audio;
        }
        if (frm)
            frames.emplace_back(frm);
    }

    ~impl() {}

    inline void encode(const encoding_t& config) {
        if (packets.find(config) != packets.cend())
            return;
        else
            packets.insert(std::make_pair<encoding_t, std::list<packet_ref>>(
                encoding_t{config}, {}));
        for (const auto& f : frames) {
            if (type == packet_type::video) {
                if (config.height < f->height ||
                    super.is_webcam) { // scale and encode
                    frame frm;
                    super.iscaler.perform_scale(
                        f.get(), -1, config.height, frm);
                    super.iencoder.encode_packets(
                        config, frm.get(), packets[config]);
                } else
                    super.iencoder.encode_packets(
                        config, f.get(), packets[config]);
            } else {
                for (const auto& f : super.iresampler.make_frames(
                         f.get(),
                         super.idecoder.audio_context(),
                         super.iencoder.context(config)))
                    super.iencoder.encode_packets(
                        config, f.get(), packets[config]);
            }
        }
    }

    const std::list<packet_ref>& make_packets(const encoding_t& config) {
        if ((is_video(config) && (iframe || type == packet_type::video)) ||
            (is_audio(config) && type == packet_type::audio)) {
            if (frames.empty())
                super.idecoder.decode_frames(ipacket, frames);
            encode(config);
            return packets[config];
        } else {
            return unchanged;
        }
    }
};

transcoder::transcoder(source_data& s, const AVPacket* p, const AVFrame* f)
    : pimpl{std::make_unique<impl>(s, p, f)} {}

transcoder::~transcoder() {}

const std::list<packet_ref>&
transcoder::make_packets(const encoding_t& config) {
    return pimpl->make_packets(config);
}

std::list<frame_ref>&
transcoder::frames() const {
    return pimpl->frames;
}

} // namespace lxstreamer
