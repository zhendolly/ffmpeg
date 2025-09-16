#include <thread>
#include <iostream>
#include "demuxer.h"
#include "muxer.h"
#include "video_decoder.h"
#include "video_encoder.h"
#include "video_filter.h"
#include "audio_decoder.h"
#include "audio_encoder.h"
#include "audio_filter.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavfilter/avfilter.h"
}

int main(int argc, char* argv[]) {


    float speed = 1.0;
    if (argc > 1) {
        speed = atof(argv[1]);
        if (speed < 0.5) speed = 0.5;
        if (speed > 3.0) speed = 3.0;
    }


    AVFormatContext* fmt_ctx = nullptr;
    const char* input_file = "1.mp4";
    if (avformat_open_input(&fmt_ctx, input_file, nullptr, nullptr) != 0) {
        std::cerr << "无法打开输入文件: " << input_file << std::endl;
        return -1;
    }
    
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "无法获取流信息" << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // 打印输入文件信息
    av_dump_format(fmt_ctx, 0, input_file, 0);

    // 查找视频流
    int video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream < 0) {
        std::cerr << "找不到视频流" << std::endl;
        return -1;
    }

    // 初始化视频解码器
    AVCodec* video_dec_codec = avcodec_find_decoder(fmt_ctx->streams[video_stream]->codecpar->codec_id);
    AVCodecContext* video_dec_ctx = avcodec_alloc_context3(video_dec_codec);
    avcodec_parameters_to_context(video_dec_ctx, fmt_ctx->streams[video_stream]->codecpar);
    avcodec_open2(video_dec_ctx, video_dec_codec, nullptr);

    // 输出文件初始化
    AVFormatContext* out_fmt = nullptr;
    const char* output_file = "lzyresult.mp4";
    int ret = avformat_alloc_output_context2(&out_fmt, nullptr, nullptr, output_file);
    if (ret < 0 || !out_fmt) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法创建输出上下文: " << errbuf << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // 获取输入视频流的参数
    AVStream* in_video_stream = fmt_ctx->streams[video_stream];
    int in_width = in_video_stream->codecpar->width;
    int in_height = in_video_stream->codecpar->height;
    int64_t in_bit_rate = in_video_stream->codecpar->bit_rate;
    AVRational in_time_base = in_video_stream->time_base;
    AVRational in_frame_rate = in_video_stream->avg_frame_rate;
    
    std::cout << "输入视频参数：" << std::endl
              << "分辨率: " << in_width << "x" << in_height << std::endl
              << "帧率: " << av_q2d(in_frame_rate) << " fps" << std::endl
              << "比特率: " << (in_bit_rate / 1000) << " kb/s" << std::endl
              << "时间基准: " << in_time_base.num << "/" << in_time_base.den << std::endl;

    // 初始化视频编码器 - 尝试多种编码器
    AVCodec* video_enc_codec = nullptr;
    
    // 尝试不同的编码器，按优先级排序
    const char* video_encoders[] = {"libx264", "mpeg4", "h264", "libxvid", "mjpeg", nullptr};
    int encoder_index = 0;
    
    while (video_encoders[encoder_index] && !video_enc_codec) {
        video_enc_codec = avcodec_find_encoder_by_name(video_encoders[encoder_index]);
        if (video_enc_codec) {
            std::cout << "使用视频编码器: " << video_encoders[encoder_index] << std::endl;
            break;
        }
        encoder_index++;
    }
    
    // 如果找不到任何指定的编码器，尝试使用MPEG4
    if (!video_enc_codec) {
        std::cout << "找不到指定的视频编码器，尝试使用默认MPEG4编码器" << std::endl;
        video_enc_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    }
    
    if (!video_enc_codec) {
        std::cerr << "找不到可用的视频编码器" << std::endl;
        return -1;
    }
    
    AVStream* video_out_stream = avformat_new_stream(out_fmt, nullptr);
    AVCodecContext* video_enc_ctx = avcodec_alloc_context3(video_enc_codec);
    

    video_enc_ctx->width = in_width;
    video_enc_ctx->height = in_height;
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_enc_ctx->bit_rate = in_bit_rate > 0 ? in_bit_rate : 431000;
    

    if (strcmp(video_enc_codec->name, "mpeg4") == 0) {
        if (in_time_base.den > 65535) {
            video_enc_ctx->time_base = (AVRational){1, 25000};
        } else {
            video_enc_ctx->time_base = in_time_base; // 使用输入文件的时间基准
        }
    } else {
        video_enc_ctx->time_base = in_time_base;
    }
    
    video_enc_ctx->framerate = in_frame_rate;
    video_enc_ctx->gop_size = 25;
    video_enc_ctx->max_b_frames = 3;
    video_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    // 如果是libx264编码器，设置预设和配置文件
    if (strcmp(video_enc_codec->name, "libx264") == 0) {
        av_opt_set(video_enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set(video_enc_ctx->priv_data, "profile", "main", 0);
        av_opt_set(video_enc_ctx->priv_data, "tune", "film", 0);
    }
    
    // 打开视频编码器
    ret = avcodec_open2(video_enc_ctx, video_enc_codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法打开视频编码器: " << errbuf << std::endl;
        return -1;
    }
    
    // 从编码器上下文复制参数到输出流
    avcodec_parameters_from_context(video_out_stream->codecpar, video_enc_ctx);
    video_out_stream->time_base = video_enc_ctx->time_base;

    // 初始化音频流
    AVCodecContext *audio_dec_ctx = nullptr, *audio_enc_ctx = nullptr;
    AVStream* audio_out_stream = nullptr;
    int audio_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream >= 0) {
        std::cout << "找到音频流，索引: " << audio_stream << std::endl;
        
        // 获取输入音频流的参数
        AVStream* in_audio_stream = fmt_ctx->streams[audio_stream];
        int in_sample_rate = in_audio_stream->codecpar->sample_rate;
        int in_channels = in_audio_stream->codecpar->channels;
        int64_t in_channel_layout = in_audio_stream->codecpar->channel_layout;
        int64_t in_audio_bit_rate = in_audio_stream->codecpar->bit_rate;
        
        if (in_channel_layout == 0) {
            in_channel_layout = av_get_default_channel_layout(in_channels);
        }
        
        std::cout << "输入音频参数：" << std::endl
                  << "采样率: " << in_sample_rate << " Hz" << std::endl
                  << "声道数: " << in_channels << std::endl
                  << "声道布局: 0x" << std::hex << in_channel_layout << std::dec << std::endl
                  << "比特率: " << (in_audio_bit_rate / 1000) << " kb/s" << std::endl;
        
        // 初始化音频解码器
        AVCodec* audio_dec_codec = avcodec_find_decoder(fmt_ctx->streams[audio_stream]->codecpar->codec_id);
        audio_dec_ctx = avcodec_alloc_context3(audio_dec_codec);
        avcodec_parameters_to_context(audio_dec_ctx, fmt_ctx->streams[audio_stream]->codecpar);
        avcodec_open2(audio_dec_ctx, audio_dec_codec, nullptr);

        // 初始化音频编码器 - 尝试多种编码器
        AVCodec* audio_enc_codec = nullptr;
        
        // 尝试不同的音频编码器，按优先级排序
        const char* audio_encoders[] = {"libfdk_aac", "libfaac", "aac", "mp3", "libmp3lame", nullptr};
        int audio_encoder_index = 0;
        
        while (audio_encoders[audio_encoder_index] && !audio_enc_codec) {
            audio_enc_codec = avcodec_find_encoder_by_name(audio_encoders[audio_encoder_index]);
            if (audio_enc_codec) {
                std::cout << "使用音频编码器: " << audio_encoders[audio_encoder_index] << std::endl;
                break;
            }
            audio_encoder_index++;
        }
        
        // 如果找不到任何指定的编码器，尝试使用MP3
        if (!audio_enc_codec) {
            std::cout << "找不到指定的音频编码器，尝试使用默认MP3编码器" << std::endl;
            audio_enc_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
        }
        
        if (!audio_enc_codec) {
            std::cerr << "找不到可用的音频编码器，将只处理视频" << std::endl;
        } else {
            audio_enc_ctx = avcodec_alloc_context3(audio_enc_codec);
            

            audio_enc_ctx->sample_rate = in_sample_rate;
            audio_enc_ctx->channel_layout = in_channel_layout;
            audio_enc_ctx->channels = in_channels;

            // 根据编码器选择合适的采样格式（位深度）
            if (audio_enc_codec->sample_fmts) {
                // 尝试使用32位浮点格式，如果支持的话
                bool found_format = false;
                for (int i = 0; audio_enc_codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++) {
                    if (audio_enc_codec->sample_fmts[i] == AV_SAMPLE_FMT_FLT ||
                        audio_enc_codec->sample_fmts[i] == AV_SAMPLE_FMT_FLTP) {
                        audio_enc_ctx->sample_fmt = audio_enc_codec->sample_fmts[i];
                        found_format = true;
                        std::cout << "使用32位浮点音频格式" << std::endl;
                        break;
                    }
                }
                // 如果不支持32位浮点，则使用编码器支持的第一个格式
                if (!found_format) {
                    audio_enc_ctx->sample_fmt = audio_enc_codec->sample_fmts[0];
                    std::cout << "使用编码器默认音频格式" << std::endl;
                }
            } else {
                // 如果编码器没有指定支持的格式，使用默认的浮点格式
                audio_enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
                std::cout << "使用默认32位浮点音频格式" << std::endl;
            }

            // 设置音频比特率 - 使用与输入文件相同的比特率
            audio_enc_ctx->bit_rate = in_audio_bit_rate > 0 ? in_audio_bit_rate : 130000; // 使用输入文件的比特率，如果没有则使用130 kb/s
            std::cout << "音频比特率: " << (audio_enc_ctx->bit_rate / 1000) << " kb/s" << std::endl;

            // 计算并显示原始音频数据率
            int64_t raw_bit_rate = (int64_t)audio_enc_ctx->sample_rate * 
                                 (audio_enc_ctx->sample_fmt == AV_SAMPLE_FMT_FLT || 
                                  audio_enc_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP ? 32 : 16) * 
                                 audio_enc_ctx->channels;
            
            std::cout << "原始音频数据率: " << (raw_bit_rate / 1000.0) << " kbps" << std::endl;
            std::cout << "压缩后音频数据率: " << (audio_enc_ctx->bit_rate / 1000.0) << " kbps" << std::endl;
            std::cout << "压缩比: " << (raw_bit_rate / (double)audio_enc_ctx->bit_rate) << ":1" << std::endl;

            audio_enc_ctx->time_base = (AVRational){1, audio_enc_ctx->sample_rate};
            audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            
            // 打开音频编码器
            ret = avcodec_open2(audio_enc_ctx, audio_enc_codec, nullptr);
            if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "无法打开音频编码器: " << errbuf << std::endl;
                audio_enc_ctx = nullptr;
            } else {
                // 创建音频输出流
                audio_out_stream = avformat_new_stream(out_fmt, nullptr);
                avcodec_parameters_from_context(audio_out_stream->codecpar, audio_enc_ctx);
                audio_out_stream->time_base = audio_enc_ctx->time_base;
                
                std::cout << "音频编码器初始化完成，编码器: " << audio_enc_codec->name
                          << ", 采样率: " << audio_enc_ctx->sample_rate 
                          << ", 声道数: " << audio_enc_ctx->channels << std::endl;
            }
        }
    }

    // 打印输出文件信息
    av_dump_format(out_fmt, 0, output_file, 1);

    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&out_fmt->pb, output_file, AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "无法打开输出文件: " << errbuf << std::endl;
            avformat_close_input(&fmt_ctx);
            avformat_free_context(out_fmt);
            return -1;
        }
    }

    // 创建队列
    PacketQueue video_packet_queue, audio_packet_queue, encoded_video_queue, encoded_audio_queue;
    FrameQueue video_frame_queue, audio_frame_queue, filtered_audio_queue;

     // 解复用线程
     std::cout << "解复用线程已启动" << std::endl;
     std::thread demux_thread(demuxer, fmt_ctx, std::ref(video_packet_queue), std::ref(audio_packet_queue), video_stream, audio_stream);

     // 视频处理线程
     std::cout << "视频处理线程已启动" << std::endl;
     std::thread video_decode_thread(video_decoder, video_dec_ctx, std::ref(video_packet_queue), std::ref(video_frame_queue));
     std::thread video_encode_thread(video_encoder, video_enc_ctx, std::ref(video_frame_queue), std::ref(encoded_video_queue), speed);

     // 音频处理线程
     std::cout << "音频处理线程已启动" << std::endl;
     std::thread audio_decode_thread, audio_filter_thread, audio_encode_thread;
     if (audio_stream >= 0 && audio_enc_ctx) {
         audio_decode_thread = std::thread(audio_decoder, audio_dec_ctx, std::ref(audio_packet_queue), std::ref(audio_frame_queue));
         AVFilterContext *src_ctx = nullptr, *sink_ctx = nullptr;
         AVFilterGraph *filter_graph = nullptr;
         init_audio_filters(audio_dec_ctx, audio_enc_ctx, &filter_graph, &src_ctx, &sink_ctx, speed);
         audio_filter_thread = std::thread(audio_filter_process, src_ctx, sink_ctx, std::ref(audio_frame_queue), std::ref(filtered_audio_queue));
         audio_encode_thread = std::thread(audio_encoder, audio_enc_ctx, std::ref(filtered_audio_queue), std::ref(encoded_audio_queue));
     }

     // 复用线程
     std::cout << "复用线程已启动" << std::endl;
     std::thread mux_thread(muxer, out_fmt, std::ref(encoded_video_queue), std::ref(encoded_audio_queue));

     // 等待所有线程完成
     demux_thread.join();
     std::cout << "解复用线程已结束" << std::endl;
     video_decode_thread.join();
     video_encode_thread.join();
     std::cout << "视频处理线程已结束" << std::endl;

     if (audio_stream >= 0 && audio_enc_ctx) {
         audio_decode_thread.join();
         if (audio_filter_thread.joinable()) audio_filter_thread.join();
         if (audio_encode_thread.joinable()) audio_encode_thread.join();
     }
     std::cout << "音频处理线程已结束" << std::endl;

     mux_thread.join();
     std::cout << "复用线程已结束" << std::endl;

     std::cout << "所有线程已完成" << std::endl;

    // 资源释放
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&video_enc_ctx);
    if (audio_stream >= 0 && audio_enc_ctx) {
        avcodec_free_context(&audio_dec_ctx);
        avcodec_free_context(&audio_enc_ctx);
    }
    
    if (out_fmt && out_fmt->pb) {
        avio_closep(&out_fmt->pb);
    }
    
    if (out_fmt) {
        avformat_free_context(out_fmt);
    }

    std::cout << "转码完成，输出文件: " << output_file << std::endl;
    return 0;
}


