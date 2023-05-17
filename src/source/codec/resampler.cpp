/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "resampler.hpp"
#include "../source_data.hpp"
#include "utils.hpp"

struct resample_config {
    AVCodecContext* src_ctx{nullptr};
    AVCodecContext* dest_ctx{nullptr};

    bool operator==(const resample_config& o) const {
        if (!src_ctx || !dest_ctx || !o.src_ctx || !o.dest_ctx)
            return true;
        return src_ctx->sample_fmt == o.src_ctx->sample_fmt &&
               src_ctx->sample_rate == o.src_ctx->sample_rate &&
               !av_channel_layout_compare(
                   &src_ctx->ch_layout, &o.src_ctx->ch_layout) &&
               dest_ctx->sample_fmt == o.dest_ctx->sample_fmt &&
               dest_ctx->sample_rate == o.dest_ctx->sample_rate &&
               !av_channel_layout_compare(
                   &dest_ctx->ch_layout, &o.dest_ctx->ch_layout);
    }
};

// implement hashing for scale_config type
namespace std {

template <> struct hash<AVChannelLayout> {
    auto operator()(const AVChannelLayout& h) const {
        return std::hash<AVChannelOrder>{}(h.order) ^
               std::hash<int>{}(h.nb_channels) ^
               std::hash<uint64_t>{}(h.u.mask);
    }
};

template <> struct hash<resample_config> {
    auto operator()(const resample_config& h) const {
        if (!h.src_ctx || !h.dest_ctx)
            return std::hash<int>{}(0);
        return std::hash<AVSampleFormat>{}(h.src_ctx->sample_fmt) ^
               std::hash<int>{}(h.src_ctx->sample_rate) ^
               std::hash<AVChannelLayout>{}(h.src_ctx->ch_layout) ^
               std::hash<AVSampleFormat>{}(h.dest_ctx->sample_fmt) ^
               std::hash<int>{}(h.dest_ctx->sample_rate) ^
               std::hash<AVChannelLayout>{}(h.dest_ctx->ch_layout);
    }
};

} // namespace std

namespace lxstreamer {
namespace {

struct filter_data {
    resample_config               config;
    std::unique_ptr<filter_graph> filter =
        std::unique_ptr<filter_graph>{new filter_graph};
    AVFilterContext* buffersrc_ctx{nullptr};
    AVFilterContext* buffersink_ctx{nullptr};
    int64_t          first_pts{0};
    elapsed_timer    tt;
};

} // namespace

struct resampler::impl {

    const source_data&                               super;
    std::unordered_map<resample_config, filter_data> filters;

    explicit impl(const source_data& sup) : super{sup} {}

    ~impl() {}

    int init_filters(filter_data& fd);

    std::list<frame>
    make_frames(const AVFrame* src, const resample_config& config);
};

int
resampler::impl::init_filters(filter_data& fd) {
    fd.filter->realloc();
    int  ret   = 0;
    auto graph = fd.filter->get();
    if (!graph)
        return AVERROR(ENOMEM);

    auto& sctx = fd.config.src_ctx;
    auto& dctx = fd.config.dest_ctx;
    if (sctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(
            &sctx->ch_layout, sctx->ch_layout.nb_channels);

    char src_ch_desc[64];
    char dest_ch_desc[64];
    av_channel_layout_describe(
        &sctx->ch_layout, src_ch_desc, sizeof(src_ch_desc));
    av_channel_layout_describe(
        &dctx->ch_layout, dest_ch_desc, sizeof(dest_ch_desc));
    std::string input = format_string(
        "abuffer=time_base=%d/"
        "%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s [in]; ",
        sctx->time_base.num,
        sctx->time_base.den,
        sctx->sample_rate,
        av_get_sample_fmt_name(sctx->sample_fmt),
        src_ch_desc);
    std::string aformat_descr = format_string(
        "[in] aformat=sample_rates=%d:sample_fmts=%s:channel_layouts=%s "
        "[aformat_out]; ",
        dctx->sample_rate,
        av_get_sample_fmt_name(dctx->sample_fmt),
        dest_ch_desc);
    std::string asetnsamples_descr = format_string(
        "[aformat_out] asetnsamples=n=%d [asetnsamples_out]; ",
        dctx->frame_size);
    // generate timestamps by counting samples
    std::string asetpts_descr =
        "[asetnsamples_out] asetpts=N/SR/TB [asetpts_out]; ";
    std::string sink = "[asetpts_out] abuffersink";
    auto        filters_descr =
        input + aformat_descr + asetnsamples_descr + asetpts_descr + sink;

    AVFilterInOut* unlinked_inputs  = nullptr;
    AVFilterInOut* unlinked_outputs = nullptr;
    if ((ret = avfilter_graph_parse2(
             graph,
             filters_descr.c_str(),
             &unlinked_inputs,
             &unlinked_outputs)) < 0)
        return ret;

    if (ret = avfilter_graph_config(graph, nullptr); ret < 0) {
        logError(
            "resample: invalid filter graph: src: %s err:%d, %s",
            super.iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        logTrace("filter graph dot description:\n %s", get_digraph(graph));
    }

    avfilter_inout_free(&unlinked_inputs);
    avfilter_inout_free(&unlinked_outputs);

    for (unsigned int i = 0; i < graph->nb_filters; ++i) {
        auto& f = graph->filters[i];
        if (std::string{f->filter->name} == "abuffer")
            fd.buffersrc_ctx = avfilter_graph_get_filter(graph, f->name);
        else if (std::string{f->filter->name} == "abuffersink")
            fd.buffersink_ctx = avfilter_graph_get_filter(graph, f->name);
    }

    return ret;
}

std::list<frame>
resampler::impl::make_frames(
    const AVFrame* src, const resample_config& config) {
    if (filters.find(config) == filters.cend()) {
        filters.insert({config, filter_data{}});
        filters[config].config = config;
    }
    auto& fd = filters[config];

    if (!fd.buffersink_ctx) {
        if (auto ec = init_filters(fd); ec < 0) {
            logError(
                "resample: failed to initialize filters: src: %s err:%d",
                super.iargs.name,
                ec);
            return {};
        }
    }

    fd.tt.start();

    /* push source frame into the filtergraph */
    if (auto ret = av_buffersrc_add_frame_flags(
            fd.buffersrc_ctx,
            const_cast<AVFrame*>(src),
            AV_BUFFERSRC_FLAG_PUSH | AV_BUFFERSRC_FLAG_KEEP_REF);
        ret < 0) {
        logError(
            "resample: failed to initialize filters: src: %s err:%d, %s",
            super.iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        return {};
    }

    /* pull filtered frames from the filtergraph */
    std::list<frame> frames;
    int              ret = 0;
    while (ret >= 0) {
        frame frm;
        ret = av_buffersink_get_frame(fd.buffersink_ctx, frm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;

        if (fd.first_pts <= 0)
            fd.first_pts = src->pts;
        frm.get()->pts += fd.first_pts;
        frm.get()->duration  = frm.get()->nb_samples;
        frm.get()->time_base = av_buffersink_get_time_base(fd.buffersink_ctx);

        if (ret >= 0)
            frames.emplace_back(std::move(frm));
    }

    return frames;
}

resampler::resampler(const source_data& s) : pimpl{std::make_unique<impl>(s)} {}

resampler::~resampler() {}

std::list<frame>
resampler::make_frames(
    const AVFrame* frm, AVCodecContext* in_ctx, AVCodecContext* out_ctx) {
    resample_config config{in_ctx, out_ctx};
    if (!in_ctx || !out_ctx) {
        logError(
            "resample: invalid AVCodecContext: src: %s",
            pimpl->super.iargs.name);
        return {};
    }
    return pimpl->make_frames(frm, config);
}

void
resampler::prune() {
    for (auto it = pimpl->filters.begin(); it != pimpl->filters.end();)
        if (it->second.tt.seconds() > 5)
            it = pimpl->filters.erase(it);
        else
            ++it;
}

} // namespace lxstreamer
