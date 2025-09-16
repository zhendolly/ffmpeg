#include "audio_filter.h"
#include <iostream>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/mathematics.h>
}

int init_audio_filters(AVCodecContext* dec_ctx,
                      AVCodecContext* enc_ctx,
                      AVFilterGraph** graph,
                      AVFilterContext** src_ctx,
                      AVFilterContext** sink_ctx,
                      float speed) {
    int ret;
    char args[512];
    
    // 打印输入音频参数
    // std::cout << "音频滤波器输入参数：" << std::endl
    //           << "采样率: " << dec_ctx->sample_rate << std::endl
    //           << "采样格式: " << av_get_sample_fmt_name(dec_ctx->sample_fmt) << std::endl
    //           << "声道布局: 0x" << std::hex << dec_ctx->channel_layout << std::dec << std::endl
    //           << "时间基准: " << dec_ctx->time_base.num << "/" << dec_ctx->time_base.den << std::endl;

    enc_ctx->time_base = dec_ctx->time_base;

    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%lx",
            dec_ctx->time_base.num, dec_ctx->time_base.den,
            dec_ctx->sample_rate,
            av_get_sample_fmt_name(dec_ctx->sample_fmt),
            dec_ctx->channel_layout);

    *graph = avfilter_graph_alloc();
    if (!*graph) {
        std::cerr << "无法分配滤波器图" << std::endl;
        return -1;
    }

    // 创建输入滤波器（abuffer）
    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    ret = avfilter_graph_create_filter(src_ctx, abuffer, "in", args, NULL, *graph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        std::cerr << "无法创建音频输入滤波器: " << err_buf << std::endl;
        return ret;
    }

    // 创建输出滤波器（abuffersink）
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    ret = avfilter_graph_create_filter(sink_ctx, abuffersink, "out", NULL, NULL, *graph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        std::cerr << "无法创建音频输出滤波器: " << err_buf << std::endl;
        return ret;
    }

    // 设置输出参数
    enum AVSampleFormat out_sample_fmts[] = { enc_ctx->sample_fmt, AV_SAMPLE_FMT_NONE };
    int out_sample_rates[] = { enc_ctx->sample_rate, -1 };
    uint64_t out_channel_layouts[] = { enc_ctx->channel_layout, 0 };
    
    ret = av_opt_set_int_list(*sink_ctx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        std::cerr << "无法设置输出采样格式" << std::endl;
        return ret;
    }
    
    ret = av_opt_set_int_list(*sink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        std::cerr << "无法设置输出采样率" << std::endl;
        return ret;
    }
    
    ret = av_opt_set_int_list(*sink_ctx, "channel_layouts", out_channel_layouts, 0, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        std::cerr << "无法设置输出声道布局" << std::endl;
        return ret;
    }

    // 创建滤波器链
    AVFilterContext* last_filter = *src_ctx;

    // 创建atempo滤波器用于变速
    if (speed != 1.0) {
        AVFilterContext* atempo_ctx;
        const AVFilter* atempo = avfilter_get_by_name("atempo");
        char atempo_args[64];
        snprintf(atempo_args, sizeof(atempo_args), "%f", speed);
        ret = avfilter_graph_create_filter(&atempo_ctx, atempo, "atempo", atempo_args, NULL, *graph);
        if (ret < 0) {
            std::cerr << "无法创建atempo滤波器" << std::endl;
            return ret;
        }

        // 连接到上一个滤波器
        ret = avfilter_link(last_filter, 0, atempo_ctx, 0);
        if (ret < 0) {
            std::cerr << "无法连接到atempo滤波器" << std::endl;
            return ret;
        }
        last_filter = atempo_ctx;
    }


    AVFilterContext* asetpts_ctx;
    const AVFilter* asetpts = avfilter_get_by_name("asetpts");
    char asetpts_args[128];
    
    // 根据速度调整PTS
    if (speed != 1.0) {
        snprintf(asetpts_args, sizeof(asetpts_args), "(%f*PTS)", 1.0/speed);
    } else {
        snprintf(asetpts_args, sizeof(asetpts_args), "PTS");
    }
    
    ret = avfilter_graph_create_filter(&asetpts_ctx, asetpts, "asetpts", asetpts_args, NULL, *graph);
    if (ret < 0) {
        std::cerr << "无法创建asetpts滤波器" << std::endl;
        return ret;
    }

    // 连接到上一个滤波器
    ret = avfilter_link(last_filter, 0, asetpts_ctx, 0);
    if (ret < 0) {
        std::cerr << "无法连接到asetpts滤波器" << std::endl;
        return ret;
    }
    last_filter = asetpts_ctx;


    AVFilterContext* aformat_ctx;
    const AVFilter* aformat = avfilter_get_by_name("aformat");
    char aformat_args[512];
    snprintf(aformat_args, sizeof(aformat_args),
             "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%lx",
             av_get_sample_fmt_name(enc_ctx->sample_fmt),
             enc_ctx->sample_rate,
             enc_ctx->channel_layout);
    
    ret = avfilter_graph_create_filter(&aformat_ctx, aformat, "aformat", aformat_args, NULL, *graph);
    if (ret < 0) {
        std::cerr << "无法创建aformat滤波器" << std::endl;
        return ret;
    }


    ret = avfilter_link(last_filter, 0, aformat_ctx, 0);
    if (ret < 0) {
        std::cerr << "无法连接到aformat滤波器" << std::endl;
        return ret;
    }
    last_filter = aformat_ctx;


    AVFilterContext* asetnsamples_ctx;
    const AVFilter* asetnsamples = avfilter_get_by_name("asetnsamples");
    ret = avfilter_graph_create_filter(&asetnsamples_ctx, asetnsamples, "asetnsamples", "n=1024", NULL, *graph);
    if (ret < 0) {
        std::cerr << "无法创建asetnsamples滤波器" << std::endl;
        return ret;
    }


    ret = avfilter_link(last_filter, 0, asetnsamples_ctx, 0);
    if (ret < 0) {
        std::cerr << "无法连接到asetnsamples滤波器" << std::endl;
        return ret;
    }
    last_filter = asetnsamples_ctx;


    ret = avfilter_link(last_filter, 0, *sink_ctx, 0);
    if (ret < 0) {
        std::cerr << "无法连接到输出滤波器" << std::endl;
        return ret;
    }

    ret = avfilter_graph_config(*graph, NULL);
    if (ret < 0) {
        std::cerr << "无法配置滤波器图" << std::endl;
        return ret;
    }

    // std::cout << "音频滤波器初始化成功" << std::endl
    //           << "输出参数：" << std::endl
    //           << "采样率: " << enc_ctx->sample_rate << std::endl
    //           << "采样格式: " << av_get_sample_fmt_name(enc_ctx->sample_fmt) << std::endl
    //           << "声道布局: 0x" << std::hex << enc_ctx->channel_layout << std::dec << std::endl
    //           << "时间基准: " << enc_ctx->time_base.num << "/" << enc_ctx->time_base.den << std::endl
    //           << "帧大小: 1024 采样" << std::endl
    //           << "速度: " << speed << "倍" << std::endl;

    return 0;
}

void audio_filter_process(AVFilterContext* src_ctx,
                         AVFilterContext* sink_ctx,
                         FrameQueue& input_queue,
                         FrameQueue& output_queue) {
    AVFrame* frame = av_frame_alloc();
    int frame_count = 0;
    int output_count = 0;
    int64_t last_pts = AV_NOPTS_VALUE;
    
    std::cout << "开始处理音频帧..." << std::endl;
    
    while(AVFrame* input_frame = input_queue.pop()) {
        frame_count++;

        if (input_frame->pts == AV_NOPTS_VALUE) {
            if (last_pts != AV_NOPTS_VALUE) {

                input_frame->pts = last_pts + input_frame->nb_samples;
            } else {

                input_frame->pts = 0;
            }
        }
        
        last_pts = input_frame->pts;
        
        // std::cout << "处理音频帧 #" << frame_count
        //           << " PTS: " << input_frame->pts
        //           << " 采样数: " << input_frame->nb_samples << std::endl;
        
        int ret = av_buffersrc_add_frame(src_ctx, input_frame);
        if(ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err_buf, sizeof(err_buf));
            std::cerr << "发送音频帧到滤波器失败: " << err_buf << std::endl;
            av_frame_free(&input_frame);
            continue;
        }

        while(true) {
            ret = av_buffersink_get_frame(sink_ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if(ret < 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                std::cerr << "从滤波器获取音频帧失败: " << err_buf << std::endl;
                break;
            }

            output_count++;
            AVFrame* cloned = av_frame_clone(frame);
            if (!cloned) {
                std::cerr << "无法克隆音频帧" << std::endl;
                av_frame_unref(frame);
                continue;
            }


            if (cloned->pts == AV_NOPTS_VALUE) {
                std::cerr << "输出音频帧没有有效的PTS" << std::endl;
            }

            // std::cout << "输出音频帧 #" << output_count
            //           << " PTS: " << cloned->pts
            //           << " 采样数: " << cloned->nb_samples << std::endl;

            output_queue.push(cloned);
            av_frame_unref(frame);
        }
        av_frame_free(&input_frame);
    }

    // std::cout << "音频滤波处理完成，共处理 " << frame_count
    //           << " 个输入帧，输出 " << output_count << " 个帧" << std::endl;
              
    output_queue.set_eof();
    av_frame_free(&frame);
}
