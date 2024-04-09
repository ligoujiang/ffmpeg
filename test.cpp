#include <iostream>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavcodec/bsf.h>
}
 
using namespace std;
 
int main(int argc,char**argv)
{
    if(argc != 3)
    {
        printf("invalid parameters\n");
        return -1;
    }
 
    av_log_set_level(AV_LOG_DEBUG);
 
    char input[64] = {0},
         output[64] = {0};
 
    strncpy(input,argv[1],sizeof(input)-1);
    strncpy(output,argv[2],sizeof(output)-1);
    av_log(nullptr,AV_LOG_DEBUG,"input:%s output:%s\n",input,output);
 
    AVFormatContext *ifmt_ctx = nullptr,
                    *ofmt_ctx = nullptr;
    int ret = 0;
 
    if((ret = avformat_open_input(&ifmt_ctx,input,0,0)) < 0)
    {
        av_log(nullptr,AV_LOG_ERROR,"Could not find open input file.\n");
        return -1;
    }
 
    if((ret = avformat_find_stream_info(ifmt_ctx,0)) < 0 )
    {
        av_log(nullptr,AV_LOG_ERROR,"Could not find stream.\n");
        return -1;
    }
    av_log(nullptr,AV_LOG_DEBUG,"input name:%s longname:%s\n",ifmt_ctx->iformat->name,ifmt_ctx->iformat->long_name);
 
    av_dump_format(ifmt_ctx,0,input,0);   //打印输入文件细节
    
    av_log(nullptr,AV_LOG_DEBUG,"duration=\n");
    av_log(nullptr,AV_LOG_DEBUG,"filename=%s\n",ifmt_ctx->url);
    if((ret = avformat_alloc_output_context2(&ofmt_ctx,nullptr,nullptr,output)) < 0)
    {
        av_log(nullptr,AV_LOG_ERROR,"Could not alloc output context.\n");
        return -1;
    }
    bool isBsf = false; //是否转换h264模式
    if(!strncmp(ofmt_ctx->oformat->name,"avi",strlen("avi")))
        isBsf = true;
	
    av_log(nullptr,AV_LOG_DEBUG,"output name:%s longname:%s\n",ofmt_ctx->oformat->name,ofmt_ctx->oformat->long_name);
 
    /*必须申请一个内存，否则后续各种段错误，ffmpeg存在很多结果初始化需要分配内存*/
    AVCodecParameters *par = avcodec_parameters_alloc();
    for(int i = 0;i < ifmt_ctx->nb_streams;i++)
    {
        AVStream *in_stream = ifmt_ctx->streams[i];
        const AVCodec *codec = avcodec_find_encoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx,codec);
        if(!out_stream)
        {
            av_log(nullptr,AV_LOG_ERROR,"Failed allocating output Stream.");
            return -1;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if(ret < 0)
        {
            av_log(nullptr,AV_LOG_ERROR,"Failed paste context to output.");
            return -1;
        }
 
        av_log(nullptr,AV_LOG_DEBUG,"codec_tag=%d\n",out_stream->codecpar->codec_tag);
        /*ffmpeg中，avformat_write_header 调用init_muxer时会判断par->codec_tag，如果不为0，会进行附加验证*/
        out_stream->codecpar->codec_tag = 0;
        if(in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            ret = avcodec_parameters_copy(par, in_stream->codecpar);
            if(ret < 0)
            {
                av_log(nullptr,AV_LOG_ERROR,"Failed paste context to par.");
                return -1;
            }
        }
    }
    av_dump_format(ofmt_ctx,0,output,1);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, output, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output file %s ", output);
            return -1;
        }
    }
 
    /*AVPacket是存储压缩编码数据相关信息的结构体。 保存的是解码前的数据，也就是压缩后的数据。 
    该结构体不直接包含数据，其中有一个执行数据域的指针，ffmpeg中的很多数据结构都是用这种方法来管理
    它保存了解复用之后，解码之前的数据（仍然是压缩后的数据）和关于这些数据的一些附加信息，如
    显示时间戳（pts）、解码时间戳（dts）、数据时长，所在媒体流的索引等*/
    AVPacket *pkt = av_packet_alloc();
    AVBSFContext *bsf_ctx = nullptr;
    if(isBsf)
    {
        const AVBitStreamFilter *pfilter = av_bsf_get_by_name("h264_mp4toannexb");
        av_bsf_alloc(pfilter, &bsf_ctx);
        bsf_ctx->par_in = par;
        av_bsf_init(bsf_ctx);
    }
    ret = avformat_write_header(ofmt_ctx, nullptr);
    int frames = 0;
    int frame_index = 0;
    while(1)
    {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx,pkt);
        if(ret < 0)
        {
            av_log(nullptr,AV_LOG_INFO,"read frame from ifmt.\n");
            break;
        }
        in_stream = ifmt_ctx->streams[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];
        /*实际上time_base的意思就是时间的刻度：
         * 如（1,25），那么时间刻度就是1/25 （1,9000），那么时间刻度就是1/90000 那么，在刻度为1/25的体系下的time=5，转换成在刻度为1/90000体系下的时间time为(5*1/25) / (1/90000) = 3600*5 = 18000，ffmpeg中作pts计算时，存在大量这种转换。*/
        /*如果没有显示时间戳自己加上时间戳并且将显示时间戳赋值给解码时间戳*/
        if(pkt->pts==AV_NOPTS_VALUE)
        {
            av_log(nullptr,AV_LOG_DEBUG,"AV_NOPTS_VALUE\n");
            //Write PTS
            AVRational time_base1=in_stream->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
            //Parameters
            pkt->pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt->dts=pkt->pts;
            pkt->duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            frame_index++;
        }
        pkt->pts = av_rescale_q_rnd(pkt->pts,in_stream->time_base,out_stream->time_base,static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts,in_stream->time_base,out_stream->time_base,static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration,in_stream->time_base,out_stream->time_base);
        pkt->pos = -1;   //< byte position in stream, -1 if unknown
 
        if(pkt->stream_index == 0 && isBsf)  //video
        {
            AVPacket *fpkt = av_packet_clone(pkt);            
            av_bsf_send_packet(bsf_ctx, pkt);
            while(av_bsf_receive_packet(bsf_ctx, fpkt) == 0)
            {
                if(av_interleaved_write_frame(ofmt_ctx,fpkt) < 0)
                {
                    av_log(nullptr,AV_LOG_ERROR,"Error muxing packed");
                    break;
                }
            }
 
            av_packet_unref(fpkt);
        }
        else
        {
            ret = av_interleaved_write_frame(ofmt_ctx, pkt);
            if(ret < 0)
            {
                av_log(nullptr,AV_LOG_ERROR,"Error muxing packed");
                break;
            }
        }
 
        av_packet_unref(pkt);
    }
    av_write_trailer(ofmt_ctx);
    avformat_free_context(ifmt_ctx);
    if(par)
    {
        avcodec_parameters_free(&par);
        if(bsf_ctx)
            bsf_ctx->par_in = nullptr;
    }
    if(bsf_ctx)
        av_bsf_free(&bsf_ctx);
 
    if(ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);
 
    return 0;
}