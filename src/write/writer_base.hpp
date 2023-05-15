/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef WRITER_BASE_HPP
#define WRITER_BASE_HPP

#include "source/source_data.hpp"

#include <array>

namespace lxstreamer {

static constexpr size_t max_streams = 16;

struct writer_base {

    enum class writer_type {
        view = 0,
        record,
        unknown = -1,
    };

    writer_type                      type{writer_type::unknown};
    source_data*                     sd{nullptr};
    unique_ptr<AVFormatContext>      output{nullptr};
    std::array<int, max_streams>     out_stream_map{};
    std::array<int64_t, max_streams> first_ptses{};
    std::array<int64_t, max_streams> last_dtses{};
    elapsed_timer                    last_write_time;

    explicit writer_base(writer_type t) : type{t} {}

    writer_base() = default;

    int  write_packet(const AVPacket* pkt);
    bool make_output_streams();
    /// returns a proper audio codec compatible with output format. Returns
    /// Unknown if current source audio codec is compatible or no proper codec
    /// is found
    codec_t alternate_proper_audio_codec();
};

} // namespace lxstreamer

#endif // WRITER_BASE_HPP
