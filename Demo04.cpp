extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}
#include <iostream>
//mp4转flv格式转封装（不解码）
int main(int argc,char** argv){
    if(argc<3){
        av_log(NULL,AV_LOG_ERROR,"%s infileName.\n",argv[0]);
        return -1;
    }

    const char* infileName=argv[1];
    const char* outfileName=argv[2];
    AVFormatContext* inCtx=NULL;

    //打开
    int ret=avformat_open_input(&inCtx,infileName,NULL,NULL);
    if(ret!=0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        return -1;
    }
    //获取音视频流
    ret=avformat_find_stream_info(inCtx,NULL);
    if(ret<0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        avformat_close_input(&inCtx);
        return -1;
        
    }
    av_dump_format(inCtx,0,infileName,0);   //打印输入文件细节

    //创建输出流句柄
    AVFormatContext *outCtx=NULL;
    ret=avformat_alloc_output_context2(&outCtx,NULL,NULL,outfileName);
    if(ret<0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        avformat_close_input(&inCtx);
        avformat_free_context(outCtx);
        return -1;
    }

    //要处理的音视频流的个数
    int streamCount=inCtx->nb_streams;
    int *handleStreamIndexArray=new int[streamCount];
    if(handleStreamIndexArray==NULL){
        av_log(NULL,AV_LOG_ERROR,"new Array failed!\n");
        avformat_close_input(&inCtx);
        avformat_free_context(outCtx);
        delete []handleStreamIndexArray;
        return -1;
    }

    //循环获取音视频流
    int streamIndex=0;
    for(int i=0;i<streamCount;i++){
        AVStream *inStream=inCtx->streams[i];
        if(inStream->codecpar->codec_type!=AVMEDIA_TYPE_VIDEO&& //当不是视频流 音频流或字幕流时，不处理
            inStream->codecpar->codec_type!=AVMEDIA_TYPE_AUDIO&&
            inStream->codecpar->codec_type!=AVMEDIA_TYPE_SUBTITLE){
                handleStreamIndexArray[i]=-1;
                continue;
        }
        //创建输出流
        handleStreamIndexArray[i]=streamIndex++;
        AVStream *outStream=avformat_new_stream(outCtx,NULL);
        if(outStream==NULL){
            av_log(NULL,AV_LOG_ERROR,"create outStream failed!\n");
            avformat_close_input(&inCtx);
            avformat_free_context(outCtx);
            return -1;
        }
        //拷贝源音视频流编码参数
        if(avcodec_parameters_copy(outStream->codecpar,inStream->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"copy outStream failed!\n");
            avformat_close_input(&inCtx);
            avformat_free_context(outCtx);
            return -1;
        }
        outStream->codecpar->codec_tag=0;
    }

    //打开输出文件
        if(!(outCtx->oformat->flags&AVFMT_NOFILE)){
            ret=avio_open(&outCtx->pb,outfileName,AVIO_FLAG_WRITE);
            if(ret<0){
                av_log(NULL,AV_LOG_ERROR,"avio_open failed!\n");
                avformat_close_input(&inCtx);
                avformat_free_context(outCtx);
                return -1;
            }
        }
        ret=avformat_write_header(outCtx,NULL);
        if(ret<0){
                av_log(NULL,AV_LOG_ERROR,"avio_open failed!\n");
                avformat_close_input(&inCtx);
                avformat_free_context(outCtx);
                return -1;
        }

        AVPacket *pkt=av_packet_alloc();
        while(av_read_frame(inCtx,pkt)==0){
            if(pkt->stream_index>=streamCount||handleStreamIndexArray[pkt->stream_index]==-1){
                av_packet_unref(pkt);
                continue;
            }
            AVStream *inStream=inCtx->streams[pkt->stream_index];
            AVStream *outStream=outCtx->streams[pkt->stream_index];
            pkt->stream_index=handleStreamIndexArray[pkt->stream_index];

            //时间基转换
            pkt->pts=av_rescale_q(pkt->pts,inStream->time_base,outStream->time_base);
            pkt->dts=av_rescale_q(pkt->dts,inStream->time_base,outStream->time_base);
            pkt->duration=av_rescale_q(pkt->duration,inStream->time_base,outStream->time_base);
            pkt->pos=-1;

            //写入输出文件
            ret=av_interleaved_write_frame(outCtx,pkt);
            if(ret!=0){
                av_log(NULL,AV_LOG_ERROR,"avio_open failed!\n");
                avformat_close_input(&inCtx);
                avio_closep(&outCtx->pb);
                avformat_free_context(outCtx);
                return -1;
            }
            av_packet_unref(pkt);
        }
        ret=av_write_trailer(outCtx);
        if(ret!=0){
                av_log(NULL,AV_LOG_ERROR,"avio_open failed!\n");
                avformat_close_input(&inCtx);
                avio_closep(&outCtx->pb);
                avformat_free_context(outCtx);
                return -1;
            }
    avio_closep(&outCtx->pb);
    if(inCtx){
        avformat_close_input(&inCtx);
    }
    if(outCtx){
        avformat_free_context(outCtx);
    }
    return 0;
}
