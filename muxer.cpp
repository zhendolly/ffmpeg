#include "muxer.h"
#include <iostream>
#include <iomanip>

extern "C" {
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
}

void muxer(AVFormatContext* out_fmt,
          PacketQueue& video_queue,
          PacketQueue& audio_queue) {
    // 写入头部前确保输出格式已正确配置
    if (!out_fmt || !out_fmt->pb) {
        std::cerr << "输出格式上下文未正确初始化" << std::endl;
        return;
    }

    // 检查流的数量
    std::cout << "输出格式中的流数量: " << out_fmt->nb_streams << std::endl;
    if (out_fmt->nb_streams < 1) {
        std::cerr << "输出格式中没有流" << std::endl;
        return;
    }

    // 写入文件头
    int ret = avformat_write_header(out_fmt, NULL);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法写入输出文件头部: " << errbuf << std::endl;
        return;
    }

    // std::cout << "成功写入文件头部" << std::endl;
    // std::cout << "视频流索引: " << (out_fmt->nb_streams > 0 ? 0 : -1) << std::endl;
    // std::cout << "音频流索引: " << (out_fmt->nb_streams > 1 ? 1 : -1) << std::endl;

    // 打印时间基准信息
    for (unsigned int i = 0; i < out_fmt->nb_streams; i++) {
        std::cout << "流 #" << i << " 时间基准: " 
                  << out_fmt->streams[i]->time_base.num << "/" 
                  << out_fmt->streams[i]->time_base.den << std::endl;
    }

    AVPacket* video_pkt = NULL;
    AVPacket* audio_pkt = NULL;
    bool error_occurred = false;
    
    // 用于跟踪最后写入的时间戳
    int64_t last_video_pts = 0;
    int64_t last_audio_pts = 0;
    
    // 用于统计写入的包数量
    int video_packet_count = 0;
    int audio_packet_count = 0;
    
    // 用于计算音视频同步
    double video_time = 0;
    double audio_time = 0;
    
    try {
        while(!error_occurred) {
            if(!video_pkt) video_pkt = video_queue.pop();
            if(!audio_pkt) audio_pkt = audio_queue.pop();

            if(!video_pkt && !audio_pkt) {
                std::cout << "视频和音频队列都为空，复用结束" << std::endl;
                break;
            }

            // 计算视频和音频的实际时间（秒）
            double video_ts = video_pkt ? av_q2d(out_fmt->streams[0]->time_base) * video_pkt->pts : INFINITY;
            double audio_ts = audio_pkt ? av_q2d(out_fmt->streams[1]->time_base) * audio_pkt->pts : INFINITY;

            // 打印当前时间戳信息（调试用）
            if (video_pkt && audio_pkt) {
                // std::cout << "当前时间戳 - 视频: " << std::fixed << std::setprecision(3) << video_ts
                //           << "秒, 音频: " << audio_ts << "秒, 差值: "
                //           << std::setprecision(3) << (video_ts - audio_ts) << "秒" << std::endl;
            }

            // 选择时间戳较早的包
            if(video_ts <= audio_ts && video_pkt) {
                // 确保视频包的流索引正确
                if (out_fmt->nb_streams > 0) {
                    video_pkt->stream_index = 0;
                    
                    // 检查时间戳是否有效
                    if (video_pkt->pts == AV_NOPTS_VALUE) {
                        std::cerr << "视频包没有有效的PTS" << std::endl;
                        video_pkt->pts = last_video_pts + 1;
                        video_pkt->dts = video_pkt->pts;
                    }
                    
                    // 确保DTS不大于PTS
                    if (video_pkt->dts > video_pkt->pts) {
                        video_pkt->dts = video_pkt->pts;
                    }
                    
                    // 记录最后写入的视频时间戳
                    last_video_pts = video_pkt->pts;
                    video_time = video_ts;
                    
                    // 打印调试信息
                    // std::cout << "写入视频帧 #" << ++video_packet_count
                    //           << " PTS: " << video_pkt->pts
                    //           << " DTS: " << video_pkt->dts
                    //           << " 大小: " << video_pkt->size << " 字节" << std::endl;
                    
                    ret = av_interleaved_write_frame(out_fmt, video_pkt);
                    if (ret < 0) {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        std::cerr << "写入视频帧失败: " << errbuf << std::endl;
                        error_occurred = true;
                    }
                } else {
                    std::cerr << "没有视频流" << std::endl;
                }
                av_packet_free(&video_pkt);
                video_pkt = NULL;
            } else if (audio_pkt) {
                // 确保音频包的流索引正确
                if (out_fmt->nb_streams > 1) {
                    audio_pkt->stream_index = 1;
                    
                    // 检查时间戳是否有效
                    if (audio_pkt->pts == AV_NOPTS_VALUE) {
                        std::cerr << "音频包没有有效的PTS" << std::endl;
                        audio_pkt->pts = last_audio_pts + 1;
                        audio_pkt->dts = audio_pkt->pts;
                    }
                    
                    // 确保DTS不大于PTS
                    if (audio_pkt->dts > audio_pkt->pts) {
                        audio_pkt->dts = audio_pkt->pts;
                    }
                    
                    // 记录最后写入的音频时间戳
                    last_audio_pts = audio_pkt->pts;
                    audio_time = audio_ts;
                    
                    // 打印调试信息
                    // std::cout << "写入音频帧 #" << ++audio_packet_count
                    //           << " PTS: " << audio_pkt->pts
                    //           << " DTS: " << audio_pkt->dts
                    //           << " 大小: " << audio_pkt->size << " 字节" << std::endl;
                    
                    ret = av_interleaved_write_frame(out_fmt, audio_pkt);
                    if (ret < 0) {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        std::cerr << "写入音频帧失败: " << errbuf << std::endl;
                        error_occurred = true;
                    }
                } else {
                    std::cerr << "没有音频流" << std::endl;
                }
                av_packet_free(&audio_pkt);
                audio_pkt = NULL;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "复用过程异常: " << e.what() << std::endl;
        error_occurred = true;
    }

    // 确保释放所有资源
    if (video_pkt) {
        av_packet_free(&video_pkt);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }

    // 打印最终时间戳信息
    // std::cout << "最后视频PTS: " << last_video_pts << " (" << video_time << "秒)" << std::endl;
    // std::cout << "最后音频PTS: " << last_audio_pts << " (" << audio_time << "秒)" << std::endl;
    // std::cout << "音视频时间差: " << std::fixed << std::setprecision(3) << (video_time - audio_time) << "秒" << std::endl;
    // std::cout << "总共写入视频包: " << video_packet_count << " 个" << std::endl;
    // std::cout << "总共写入音频包: " << audio_packet_count << " 个" << std::endl;

    std::cout << "正在写入文件尾部..." << std::endl;
    ret = av_write_trailer(out_fmt);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "写入文件尾部失败: " << errbuf << std::endl;
    } else {
        std::cout << "文件尾部写入成功" << std::endl;
    }
}


