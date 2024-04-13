extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/parseutils.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
}
#include <iostream>
#include <string>



//BMF结构体
//对结构体内存对齐的预设操作
#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t
#pragma pack(2) //预设成2个字节的对齐
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
}BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

int frameCount=0;
int writePacketCount=0;

/*
sws_scale()函数
有可能不一致导致视频不能播放，可使width与linesize宽度一致
1.图像色彩空间转换（yuv各格式之间的转换或者转换成RGB）
2.分辨率缩放
3.前后图像滤镜波处理
*/


class DEMuxer{
private:
    //输入和输出参数变量
    const char* m_inFileName=nullptr;
    const char* m_outFileName=nullptr;
    //输入和输出句柄
    AVFormatContext* m_inCtx=nullptr;
    AVFormatContext* m_outCtx=nullptr;
    //packet
    AVPacket* m_pkt=nullptr;
    //frame
    AVFrame* m_frame;
    //创建解码器句柄
    AVCodecContext* m_codeCtx=nullptr;
    //打开输出文件
    FILE* m_dest_fp=nullptr;
    //更改分辨率
    char* m_destVideoSizeString=nullptr; //接收传入的参数，格式为1920x1080
    int m_destWidth=0;
    int m_destHeight=0;
    //修改视频分辨率句柄
    SwsContext *m_swsCtx=nullptr;
    //视频编码
    const char* m_encoderName=nullptr;//编码器名
    AVCodecContext* m_encodeCtx=nullptr;    //编码器的句柄
public:
    DEMuxer(char* argv1,char* argv2,char* argv3,char* argv4){
        m_inFileName=argv1;
        m_outFileName=argv2;
        m_encoderName=argv3;
        m_destVideoSizeString=argv4;
    }
    DEMuxer(char* argv1,char* argv2){
        m_inFileName=argv1;
        m_outFileName=argv2;
    }
    ~DEMuxer(){
        if(m_inCtx){
            avformat_close_input(&m_inCtx);
            m_inCtx=nullptr;
            std::cout<<"m_inCtx已被释放！"<<std::endl;
        }
        if(m_outCtx){
            avformat_free_context(m_outCtx);
            m_outCtx=nullptr;
            std::cout<<"m_outCtx已被释放！"<<std::endl;
        }

        if(m_frame){
            av_frame_free(&m_frame);
        }

        if(m_outCtx && !(m_outCtx->oformat->flags & AVFMT_NOFILE)){
            avio_closep(&m_outCtx->pb);
        }
        if(m_codeCtx){
            avcodec_free_context(&m_codeCtx);
            m_codeCtx=nullptr;
            std::cout<<"m_codeCtx已被释放！"<<std::endl;
        }
        // if(m_dest_fp){
        //     fclose(m_dest_fp);
        //     m_dest_fp=nullptr;
        //     std::cout<<"m_dest_fp已被释放！"<<std::endl;
        // }
        if(m_encodeCtx){
            avcodec_free_context(&m_encodeCtx);
            m_encodeCtx=nullptr;
            std::cout<<"m_encodeCtx已被释放！"<<std::endl;
        }
    }
    //打开输入文件
    bool openInput(){
        if(avformat_open_input(&m_inCtx,m_inFileName,NULL,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"open input failed!\n");
            return -1;
        };
        return true;
    }
    //获取文件信息并输出
    bool getFileMesage(){
    //如果获取失败，文件可能有问题
    if(avformat_find_stream_info(m_inCtx,NULL)<0){
        char errstr[256]={0};
        av_log(NULL,AV_LOG_ERROR,"find stream info failed!\n");
        return -1;
    }
        av_dump_format(m_inCtx,0,m_inFileName,0);
        return true;
    }
    //获取并输出输入文件的时间长度
    void getDuration(){
        //duration本身乘以100w,转换成秒数
        float time=m_inCtx->duration*av_q2d(AV_TIME_BASE_Q);  //AV_TINE_BASE_Q为ffmpeg内部时间基 av_q2d转换成double类型
        std::cout<<time<<"s"<<std::endl;
    }
    //获取视频文件里的时间戳
    void getTimeBase(){
        AVRational videoTimeBase;
        AVRational audioTimeBase;
        for(int i=0;i<m_inCtx->nb_streams;i++){
            AVStream* inStream=m_inCtx->streams[i];
            if(inStream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
                videoTimeBase=inStream->time_base;
                av_log(NULL,AV_LOG_INFO,"videoTimeBase num:%d den:%d\n",videoTimeBase.num,videoTimeBase.den); //num为秒
            }
            if(inStream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
                audioTimeBase=inStream->time_base;
                av_log(NULL,AV_LOG_INFO,"audioTimeBase num:%d den:%d\n",audioTimeBase.num,audioTimeBase.den);
            }
        }
    }
    //获取视频流pts和dts
    void getPtsDts(){
        AVPacket *pkt=av_packet_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            AVStream *inStream=m_inCtx->streams[pkt->stream_index];
            av_log(NULL,AV_LOG_INFO,"streamIndex:%d,pts:%ld,ptsTime:%lf,dts:%ld,dtsTime:%lf\n",pkt->stream_index,pkt->pts,pkt->pts*av_q2d(inStream->time_base),
            pkt->dts,pkt->dts*av_q2d(inStream->time_base));
        }
    }
    //截取视频
    bool cutFile(int startTime,int endTime){
        if(avformat_alloc_output_context2(&m_outCtx,NULL,NULL,m_outFileName)<0){
            av_log(NULL,AV_LOG_INFO,"alloc output context2 failed!\n");
            return false;
        };
        for(int i=0;i<m_inCtx->nb_streams;i++){
            AVStream* inStream=m_inCtx->streams[i];
            AVStream* outStream=avformat_new_stream(m_outCtx,NULL);
            if(outStream==NULL){
                av_log(NULL,AV_LOG_INFO,"new stream failed!\n");
                return false;
            }
            //复制源文件编码参数
            if(avcodec_parameters_copy(outStream->codecpar,inStream->codecpar)<0){
                av_log(NULL,AV_LOG_INFO,"avcodec copy failed!\n");
                return false;
            }
            outStream->codecpar->codec_tag=0;
        }

        if(!(m_outCtx->oformat->flags & AVFMT_NOFILE)){
            if(avio_open(&m_outCtx->pb,m_outFileName,AVIO_FLAG_WRITE)<0){
                av_log(NULL,AV_LOG_INFO,"open avio failed!\n");
                return false;
            };
        }
        //写入头部文件
        if(avformat_write_header(m_outCtx,NULL)<0){
            av_log(NULL,AV_LOG_INFO,"write header failed!\n");
            return false;
        };
        //查找包的位置
        if(av_seek_frame(m_inCtx,-1,startTime*AV_TIME_BASE,AVSEEK_FLAG_ANY)<0){ //startTime*AV_TIME_BASE转换时间戳
            av_log(NULL,AV_LOG_INFO,"seek frame failed!\n");
            return false;
        };

        //保存路数
        //int64_t *startPTS=av_malloc_array(m_inCtx->nb_streams,sizeof(int64_t));
        int64_t *startPTS=new int64_t[m_inCtx->nb_streams]{};
        int64_t *startDTS=new int64_t[m_inCtx->nb_streams]{};

        //开始截取文件
        AVPacket *pkt=av_packet_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            AVStream *inStream=m_inCtx->streams[pkt->stream_index];
            AVStream *outStream=m_outCtx->streams[pkt->stream_index];
            //当截取的视频大于结束时间时，退出while
            if(endTime< pkt->pts * av_q2d(inStream->time_base)){
                av_packet_unref(pkt);
                break;
            }
            if(startPTS[pkt->stream_index]==0){
                startPTS[pkt->stream_index]=pkt->pts;
            }
            if(startDTS[pkt->stream_index]==0){
                startDTS[pkt->stream_index]=pkt->dts;
            }
            pkt->pts=av_rescale_q(pkt->pts-startPTS[pkt->stream_index],inStream->time_base,outStream->time_base);
            pkt->dts=av_rescale_q(pkt->dts-startPTS[pkt->stream_index],inStream->time_base,outStream->time_base);
            if(pkt->pts<0){
                pkt->pts=0;
            }
            if(pkt->dts<0){
                pkt->dts=0;
            }
            pkt->duration=av_rescale_q(pkt->duration,inStream->time_base,outStream->time_base);
            pkt->pos=-1;

            //当dts大于pts时，丢弃该帧
            if(pkt->pts<pkt->dts){
                continue;
            }

            //需要验证,pts大于dts会写不进去文件
            av_interleaved_write_frame(m_outCtx,pkt);
            av_packet_unref(pkt);
        }
        av_write_trailer(m_outCtx);
        delete []startDTS;
        delete []startPTS;
        return true;
    }

    //解决YUV数据最后几帧没写进去
    bool decodeVideo(AVCodecContext *m_codeCtx,AVPacket* pkt,FILE* ofs){         
        if(avcodec_send_packet(m_codeCtx,pkt)!=0){//发送包给解码器
            av_log(NULL,AV_LOG_ERROR,"avcodec send pacaket failed!\n");
            av_packet_unref(pkt);
            return false;
        }
        AVFrame *frame=av_frame_alloc();
        //这一步是解码数据
        while(avcodec_receive_frame(m_codeCtx,frame)==0){
            fwrite((char*)frame->data[0],1,m_codeCtx->width*m_codeCtx->height,ofs);    //写入Y数据
            fwrite((char*)frame->data[1],1,m_codeCtx->width*m_codeCtx->height/4,ofs);    //写入U数据
            fwrite((char*)frame->data[2],1,m_codeCtx->width*m_codeCtx->height/4,ofs);    //写入V数据
            av_log(NULL,AV_LOG_INFO,"linesize[0]=%d,linesize[1]=%d,linesize[2]=%d,width=%d,height=%d\n",frame->linesize[0],frame->linesize[1],frame->linesize[2],
            m_codeCtx->width,m_codeCtx->height);
        }  
        if(frame){
            av_frame_free(&frame);
        }
        return true;
    }
    //解码并提取YUV
    bool deCodecYUV(){
        //查找视频索引
        int videoIndex=0;
        if(videoIndex=av_find_best_stream(m_inCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)<0){
            av_log(NULL,AV_LOG_ERROR,"find best stream failed!\n");
            return false;
        };
        //创建解码器句柄
        m_codeCtx=avcodec_alloc_context3(NULL);
        if(m_codeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec alloc context3 failed!\n");
            return false;
        }
        //拷贝解码参数
        if(avcodec_parameters_to_context(m_codeCtx,m_inCtx->streams[videoIndex]->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec parameters to context failed!\n");
            return false;
        };
        //查找解码器
        const AVCodec* decoder=avcodec_find_decoder(m_codeCtx->codec_id);
        if(decoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec find decoder failed!,codecID:%d\n",m_codeCtx->codec_id);
            return false;
        }
        //打开解码器
        if(avcodec_open2(m_codeCtx,decoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec fopen2 failed!\n");
            return false;
        };
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open FILE ofs failed!\n");
            return false;
        }        
        AVPacket *pkt=av_packet_alloc();
        //AVFrame *frame=av_frame_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            if(pkt->stream_index==videoIndex){
                if(decodeVideo(m_codeCtx,pkt,m_dest_fp)==0){
                    return false;
                };               
            }
            av_packet_unref(pkt);
        }
        //flush decoder
        decodeVideo(m_codeCtx,NULL,m_dest_fp);
        return true;
    }
    

    //解码YUV数据,并且修改分辨率
    bool destVideoSize(){
        //获取视频分辨率
        if(av_parse_video_size(&m_destWidth,&m_destHeight,m_destVideoSizeString)<0){
            av_log(NULL,AV_LOG_ERROR,"av parse video size failed!\n");
            return false;
        };
        av_log(NULL,AV_LOG_INFO,"destwidth=%d,destheight=%d\n",m_destWidth,m_destHeight);

        //查找视频索引
        int videoIndex=0;
        if(videoIndex=av_find_best_stream(m_inCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)<0){
            av_log(NULL,AV_LOG_ERROR,"find best stream failed!\n");
            return false;
        };
        //创建解码器句柄
        m_codeCtx=avcodec_alloc_context3(NULL);
        if(m_codeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec alloc context3 failed!\n");
            return false;
        }
        //拷贝解码参数
        if(avcodec_parameters_to_context(m_codeCtx,m_inCtx->streams[videoIndex]->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec parameters to context failed!\n");
            return false;
        };
        //查找解码器
        const AVCodec* decoder=avcodec_find_decoder(m_codeCtx->codec_id);
        if(decoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec find decoder failed!,codecID:%d\n",m_codeCtx->codec_id);
            return false;
        }
        //打开解码器
        if(avcodec_open2(m_codeCtx,decoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec fopen2 failed!\n");
            return false;
        };


        //设置输入和输出的参数
        enum AVPixelFormat destPixFmt=m_codeCtx->pix_fmt;//设置要转换的编码格式
        m_swsCtx=sws_getContext(m_codeCtx->width,m_codeCtx->height,m_codeCtx->pix_fmt,m_destWidth,m_destHeight,destPixFmt,SWS_FAST_BILINEAR,NULL,NULL,NULL);
        if(m_swsCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"sws getContext failed!\n");
            return false;
        }
        AVFrame* destFrame=av_frame_alloc();
        //开辟空间
        uint8_t* outBuffer=(static_cast<uint8_t*>(av_malloc((av_image_get_buffer_size(destPixFmt,m_destWidth,m_destHeight,1)))));  //获取大小
        //从获取的大小outBuffer，分配空间给destFrame->data
        av_image_fill_arrays(destFrame->data,destFrame->linesize,outBuffer,destPixFmt,m_destWidth,m_destHeight,1);

        //打开输出的文件
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open FILE ofs failed!\n");
            return false;
        }        
        AVPacket *pkt=av_packet_alloc();
        //AVFrame *frame=av_frame_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            if(pkt->stream_index==videoIndex){
                if(decodeVideo(m_codeCtx,pkt,destFrame)==0){
                    return false;
                };               
            }
            av_packet_unref(pkt);
        }
        //flush decoder
        decodeVideo(m_codeCtx,NULL,destFrame);
        if(destFrame){
            av_frame_free(&destFrame);
        }
        if(outBuffer){
            av_freep(&outBuffer);
        }
        return true;
    }
    //解决YUV数据最后几帧没写进去，并且修改分辨率版
    bool decodeVideo(AVCodecContext *m_codeCtx,AVPacket* pkt,AVFrame* destframe){         
        if(avcodec_send_packet(m_codeCtx,pkt)!=0){//发送包给解码器
            av_log(NULL,AV_LOG_ERROR,"avcodec send pacaket failed!\n");
            av_packet_unref(pkt);
            return false;
        }
        AVFrame *frame=av_frame_alloc();
        //这一步是解码数据
        while(avcodec_receive_frame(m_codeCtx,frame)==0){
            //接收到数据再修改分辨率
            sws_scale(m_swsCtx,frame->data,frame->linesize,0,m_codeCtx->height,destframe->data,destframe->linesize);
            fwrite(destframe->data[0],1,m_destWidth*m_destHeight,m_dest_fp);    //写入Y数据
            fwrite(destframe->data[1],1,m_destWidth*m_destHeight/4,m_dest_fp);    //写入U数据
            fwrite(destframe->data[2],1,m_destWidth*m_destHeight/4,m_dest_fp);    //写入V数据
            av_log(NULL,AV_LOG_INFO,"linesize[0]=%d,linesize[1]=%d,linesize[2]=%d,width=%d,height=%d\n",destframe->linesize[0],destframe->linesize[1],destframe->linesize[2],
            m_destWidth,m_destHeight);
        }  
        if(frame){
            av_frame_free(&frame);
        }
        return true;
    }
    
    
    //解码YUV数据，修改分辨率，转成RGB24编码格式
    bool destVideoSize_RGB24(){
        //获取视频分辨率
        if(av_parse_video_size(&m_destWidth,&m_destHeight,m_destVideoSizeString)<0){
            av_log(NULL,AV_LOG_ERROR,"av parse video size failed!\n");
            return false;
        };
        av_log(NULL,AV_LOG_INFO,"destwidth=%d,destheight=%d\n",m_destWidth,m_destHeight);

        //查找视频索引
        int videoIndex=0;
        if(videoIndex=av_find_best_stream(m_inCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)<0){
            av_log(NULL,AV_LOG_ERROR,"find best stream failed!\n");
            return false;
        };
        //创建解码器句柄
        m_codeCtx=avcodec_alloc_context3(NULL);
        if(m_codeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec alloc context3 failed!\n");
            return false;
        }
        //拷贝解码参数
        if(avcodec_parameters_to_context(m_codeCtx,m_inCtx->streams[videoIndex]->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec parameters to context failed!\n");
            return false;
        };
        //查找解码器
        const AVCodec* decoder=avcodec_find_decoder(m_codeCtx->codec_id);
        if(decoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec find decoder failed!,codecID:%d\n",m_codeCtx->codec_id);
            return false;
        }
        //打开解码器
        if(avcodec_open2(m_codeCtx,decoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec fopen2 failed!\n");
            return false;
        };


        //设置输入和输出的参数
        enum AVPixelFormat destPixFmt=AV_PIX_FMT_RGB24;//设置要转换的编码格式
        m_swsCtx=sws_getContext(m_codeCtx->width,m_codeCtx->height,m_codeCtx->pix_fmt,m_destWidth,m_destHeight,destPixFmt,SWS_FAST_BILINEAR,NULL,NULL,NULL);
        if(m_swsCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"sws getContext failed!\n");
            return false;
        }
        AVFrame* destFrame=av_frame_alloc();
        //开辟空间
        uint8_t* outBuffer=(static_cast<uint8_t*>(av_malloc((av_image_get_buffer_size(destPixFmt,m_destWidth,m_destHeight,1)))));  //获取大小
        //从获取的大小outBuffer，分配空间给destFrame->data
        av_image_fill_arrays(destFrame->data,destFrame->linesize,outBuffer,destPixFmt,m_destWidth,m_destHeight,1);

        //打开输出的文件
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open FILE ofs failed!\n");
            return false;
        }        
        AVPacket *pkt=av_packet_alloc();
        //AVFrame *frame=av_frame_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            if(pkt->stream_index==videoIndex){
                if(decodeVideo_RGB24(m_codeCtx,pkt,destFrame)==0){
                    return false;
                };               
            }
            av_packet_unref(pkt);
        }
        //flush decoder
        decodeVideo_RGB24(m_codeCtx,NULL,destFrame);
        if(destFrame){
            av_frame_free(&destFrame);
        }
        if(outBuffer){
            av_freep(&outBuffer);
        }
        return true;
    }
    //解决YUV数据最后几帧没写进去，并且修改分辨率,RGB24编码格式
    bool decodeVideo_RGB24(AVCodecContext *m_codeCtx,AVPacket* pkt,AVFrame* destframe){         
        if(avcodec_send_packet(m_codeCtx,pkt)!=0){//发送包给解码器
            av_log(NULL,AV_LOG_ERROR,"avcodec send pacaket failed!\n");
            av_packet_unref(pkt);
            return false;
        }
        AVFrame *frame=av_frame_alloc();
        //这一步是解码数据
        while(avcodec_receive_frame(m_codeCtx,frame)==0){
            //接收到数据再修改分辨率
            sws_scale(m_swsCtx,frame->data,frame->linesize,0,m_codeCtx->height,destframe->data,destframe->linesize);
            //以YUV格式写入数据
            // fwrite(destframe->data[0],1,m_destWidth*m_destHeight,m_ofs);    //写入Y数据
            // fwrite(destframe->data[1],1,m_destWidth*m_destHeight/4,m_ofs);    //写入U数据
            // fwrite(destframe->data[2],1,m_destWidth*m_destHeight/4,m_ofs);    //写入V数据
            //以RGB24格式写入数据
            fwrite(destframe->data[0],1,m_destWidth*m_destHeight*3,m_dest_fp);  //RGB24是3字节
            av_log(NULL,AV_LOG_INFO,"linesize[0]=%d,linesize[1]=%d,linesize[2]=%d,width=%d,height=%d\n",destframe->linesize[0],destframe->linesize[1],destframe->linesize[2],
            m_destWidth,m_destHeight);
        }  
        if(frame){
            av_frame_free(&frame);
        }
        return true;
    }


    //解码YUV数据，修改分辨率，转成RGB24编码格式保存为BMF格式的图片
    bool destVideoSize_RGB24_BMF(){
        //获取视频分辨率
        if(av_parse_video_size(&m_destWidth,&m_destHeight,m_destVideoSizeString)<0){
            av_log(NULL,AV_LOG_ERROR,"av parse video size failed!\n");
            return false;
        };
        av_log(NULL,AV_LOG_INFO,"destwidth=%d,destheight=%d\n",m_destWidth,m_destHeight);

        //查找视频索引
        int videoIndex=0;
        if(videoIndex=av_find_best_stream(m_inCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)<0){
            av_log(NULL,AV_LOG_ERROR,"find best stream failed!\n");
            return false;
        };
        //创建解码器句柄
        m_codeCtx=avcodec_alloc_context3(NULL);
        if(m_codeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec alloc context3 failed!\n");
            return false;
        }
        //拷贝解码参数
        if(avcodec_parameters_to_context(m_codeCtx,m_inCtx->streams[videoIndex]->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec parameters to context failed!\n");
            return false;
        };
        //查找解码器
        const AVCodec* decoder=avcodec_find_decoder(m_codeCtx->codec_id);
        if(decoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec find decoder failed!,codecID:%d\n",m_codeCtx->codec_id);
            return false;
        }
        //打开解码器
        if(avcodec_open2(m_codeCtx,decoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec fopen2 failed!\n");
            return false;
        };


        //设置输入和输出的参数
        enum AVPixelFormat destPixFmt=AV_PIX_FMT_BGR24;//设置要转换的编码格式 ,AV_PIX_FMT_RGB24这个格式导出的BMF图片颜色会异常
        m_swsCtx=sws_getContext(m_codeCtx->width,m_codeCtx->height,m_codeCtx->pix_fmt,m_destWidth,m_destHeight,destPixFmt,SWS_FAST_BILINEAR,NULL,NULL,NULL);
        if(m_swsCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"sws getContext failed!\n");
            return false;
        }
        AVFrame* destFrame=av_frame_alloc();
        //开辟空间
        uint8_t* outBuffer=(static_cast<uint8_t*>(av_malloc((av_image_get_buffer_size(destPixFmt,m_destWidth,m_destHeight,1)))));  //获取大小
        //从获取的大小outBuffer，分配空间给destFrame->data
        av_image_fill_arrays(destFrame->data,destFrame->linesize,outBuffer,destPixFmt,m_destWidth,m_destHeight,1);

        //打开输出的文件
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open FILE ofs failed!\n");
            return false;
        }        
        AVPacket *pkt=av_packet_alloc();
        //AVFrame *frame=av_frame_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            if(pkt->stream_index==videoIndex){
                if(decodeVideo_RGB24_BMF(m_codeCtx,pkt,destFrame)==0){
                    return false;
                };               
            }
            av_packet_unref(pkt);
        }
        //flush decoder
        decodeVideo_RGB24_BMF(m_codeCtx,NULL,destFrame);
        if(destFrame){
            av_frame_free(&destFrame);
        }
        if(outBuffer){
            av_freep(&outBuffer);
        }
        return true;
    }
    //解决YUV数据最后几帧没写进去，并且修改分辨率,RGB24编码格式
    bool decodeVideo_RGB24_BMF(AVCodecContext *m_codeCtx,AVPacket* pkt,AVFrame* destframe){         
        if(avcodec_send_packet(m_codeCtx,pkt)!=0){//发送包给解码器
            av_log(NULL,AV_LOG_ERROR,"avcodec send pacaket failed!\n");
            av_packet_unref(pkt);
            return false;
        }
        AVFrame *frame=av_frame_alloc();
        //这一步是解码数据
        while(avcodec_receive_frame(m_codeCtx,frame)==0){
            //接收到数据再修改分辨率
            sws_scale(m_swsCtx,frame->data,frame->linesize,0,m_codeCtx->height,destframe->data,destframe->linesize);
            //以YUV格式写入数据
            // fwrite(destframe->data[0],1,m_destWidth*m_destHeight,m_ofs);    //写入Y数据
            // fwrite(destframe->data[1],1,m_destWidth*m_destHeight/4,m_ofs);    //写入U数据
            // fwrite(destframe->data[2],1,m_destWidth*m_destHeight/4,m_ofs);    //写入V数据
            //以RGB24格式写入数据
            fwrite(destframe->data[0],1,m_destWidth*m_destHeight*3,m_dest_fp);  //RGB24是3字节
            frameCount++;
            //保存BMF格式图片
            char bmpFileName[64]={0};
            snprintf(bmpFileName,sizeof(bmpFileName),"./picture/%d.bmp",frameCount);
            printf("%d\n",frameCount);
            saveBMF(bmpFileName,destframe->data[0],m_destWidth,m_destHeight);
            av_log(NULL,AV_LOG_INFO,"linesize[0]=%d,linesize[1]=%d,linesize[2]=%d,width=%d,height=%d\n",destframe->linesize[0],destframe->linesize[1],destframe->linesize[2],
            m_destWidth,m_destHeight);
        }  
        if(frame){
            av_frame_free(&frame);
        }
        return true;
    }
    //提取RGB24格式的图片，并保存成BMF格式（无损）
    bool saveBMF(char* fileName,unsigned char* rgbData,int width,int height){
        int bmpDataSize=width*height*3;
        BITMAPFILEHEADER bmpFileHeader={0};
        bmpFileHeader.bfType=0x4d42;
        bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpDataSize;//整个文件的大小
        bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);

        BITMAPINFOHEADER bmpInfoHeader={0};
        bmpInfoHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmpInfoHeader.biWidth=width;
        bmpInfoHeader.biHeight=height*(-1);
        bmpInfoHeader.biPlanes=1;
        bmpInfoHeader.biBitCount=24;
        bmpInfoHeader.biCompression=0;
        bmpInfoHeader.biSizeImage=0;
        bmpInfoHeader.biXPelsPerMeter=0;
        bmpInfoHeader.biYPelsPerMeter=0;
        bmpInfoHeader.biClrUsed=0;
        bmpInfoHeader.biClrImportant=0;

        FILE* fp=fopen(fileName,"wb");
        fwrite(&bmpFileHeader,1,sizeof(BITMAPFILEHEADER),fp);
        fwrite(&bmpInfoHeader,1,sizeof(BITMAPINFOHEADER),fp);
        fwrite(rgbData,1,bmpDataSize,fp);
        fclose(fp);
        return true;
    }
    
    
    //YUV视频编码
    bool enCodeVideo(){
        //获取输入的分辨率
        if(av_parse_video_size(&m_destWidth,&m_destHeight,m_destVideoSizeString)<0){    //从m_destVideoSizeString，提取分辨率格式，并赋值给m_destWidth和m_destHeight
            av_log(NULL,AV_LOG_ERROR,"av_parse video size failed!\n");
            return false;
        };
        //指定编码器
        const AVCodec* encoder=avcodec_find_encoder_by_name(m_encoderName);
        if(encoder==NULL){
        av_log(NULL,AV_LOG_ERROR,"avcodec_find_encoder_by_name failed!\n");
        return false;
        }
        //创建编码器句柄
        m_encodeCtx=avcodec_alloc_context3(encoder);
        if(m_encodeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec_alloc_context3 failed!\n");
            return false;
        }
        //获取源视频色彩格式（根据原视频格式指定）
        enum AVPixelFormat pixFmt=AV_PIX_FMT_YUV420P;
        //指定帧率
        int fps=30;
        //给编码器句柄设置参数
        m_encodeCtx->codec_type=AVMEDIA_TYPE_VIDEO;
        m_encodeCtx->pix_fmt=pixFmt;
        m_encodeCtx->width=m_destWidth;
        m_encodeCtx->height=m_destHeight;
        m_encodeCtx->time_base=(AVRational){1,fps};     //强制类型转换
        m_encodeCtx->bit_rate=8488000;
        m_encodeCtx->max_b_frames=0;
        m_encodeCtx->gop_size=10;
        //打开编码器
        if(avcodec_open2(m_encodeCtx,encoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec_open2 failed!\n");
            return false;
        };
        //打开输入文件
        FILE* src_fp=fopen(m_inFileName,"rb");
        if(src_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"str_fp fopen failed!\n");
            return false;
        }
        //打开输出文件
        FILE* dest_fp=fopen(m_outFileName,"wb+");
        if(dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"m_dest_fp fopen failed!\n");
            return false;
        }

        AVFrame* frame=av_frame_alloc();
        int frameSize=av_image_get_buffer_size(pixFmt,m_destWidth,m_destHeight,1);
        uint8_t* frameBuffer=(static_cast<uint8_t*>(av_malloc(frameSize)));
        av_image_fill_arrays(frame->data,frame->linesize,frameBuffer,pixFmt,m_destWidth,m_destHeight,1);
        frame->format=pixFmt;
        frame->width=m_destWidth;
        frame->height=m_destHeight;


        int pictureSize=m_destWidth*m_destHeight;
        AVPacket *pkt=av_packet_alloc();

        //读取输入文件的数据
        int readFrameCount=0;
        while(fread(frameBuffer,1,pictureSize*3/2,src_fp)==pictureSize*3/2){ //pictureSize*3/2 yuv420p的数据=width*height*1.5字节
            //Y 1 U1/4 V 1/4
            frame->data[0]=frameBuffer;
            frame->data[1]=frameBuffer+pictureSize;
            frame->data[2]=frameBuffer+pictureSize+pictureSize/4;
            frame->pts=readFrameCount;
            readFrameCount++;
            printf("readFrameCount=%d\n",readFrameCount);
            encodec(frame,pkt,dest_fp);
        }
        encodec(NULL,pkt,dest_fp);

        if(src_fp){
            fclose(src_fp);
        }
        if(frameBuffer){
            av_freep(&frameBuffer);
        }
        return true;
    }
    //用于编码，flash encodec
    bool encodec(AVFrame* frame,AVPacket* pkt,FILE* dest_fp){
        int ret=avcodec_send_frame(m_encodeCtx,frame);
        if(ret<0){
            av_log(NULL,AV_LOG_ERROR,"vcodec_send_frame failed!\n");
            return false;
        }
        while(avcodec_receive_packet(m_encodeCtx,pkt)==0){
            fwrite(pkt->data,1,pkt->size,dest_fp);
            writePacketCount++;
            printf("writePacketCount=%d\n",writePacketCount);
        }
        av_packet_unref(pkt);


        // while(ret>=0){
        //     ret=avcodec_receive_packet(m_encodeCtx,pkt);
        //     if(ret==AVERROR(EAGAIN)||AVERROR_EOF){
        //         av_log(NULL,AV_LOG_ERROR,"eof\n");
        //         return true;
        //     }
        //     if(ret<0){
        //         av_log(NULL,AV_LOG_ERROR,"encodec frame failed!\n");
        //         return false;
        //     }
        //     fwrite(pkt->data,1,pkt->size,dest_fp);
        //     writePacketCount++;
        //     printf("writePacketCount=%d\n",writePacketCount);
        //     av_packet_unref(pkt);
        // }
        return true;
    }


//----------------------------------------------//
    //音频解码
    bool deCodeAudio(){
        //查找音频索引
        int audioIndex=0;
        if(audioIndex=av_find_best_stream(m_inCtx,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0)<0){
            av_log(NULL,AV_LOG_ERROR,"find best stream failed!\n");
            return false;
        }; 
        //创建解码器句柄
        m_codeCtx=avcodec_alloc_context3(NULL);
        if(m_codeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec alloc context3 failed!\n");
            return false;
        }
        //拷贝解码参数
        if(avcodec_parameters_to_context(m_codeCtx,m_inCtx->streams[audioIndex]->codecpar)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec parameters to context failed!\n");
            return false;
        };
        //查找解码器
        const AVCodec* decoder=avcodec_find_decoder(m_codeCtx->codec_id);
        if(decoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec find decoder failed!,codecID:%d\n",m_codeCtx->codec_id);
            return false;
        }
        //打开解码器
        if(avcodec_open2(m_codeCtx,decoder,NULL)!=0){
            av_log(NULL,AV_LOG_ERROR,"aavcodec_open2 failed!\n");
            return false;
        };
        //打开输出文件
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open m_dest_fp failed!\n");
            return false;
        }

        //获取音频通道数
        int channels=2;         


        m_frame=av_frame_alloc();
        //enum AVSampleFormat sample_fmt=m_codeCtx->sample_fmt;
        int frameSize=av_samples_get_buffer_size(NULL,channels,m_frame->nb_samples,m_codeCtx->sample_fmt,1);
        uint8_t* frameBuffer=(static_cast<uint8_t*>(av_malloc(frameSize)));
        avcodec_fill_audio_frame(m_frame,channels,m_codeCtx->sample_fmt,frameBuffer,frameSize,1);

        m_pkt=av_packet_alloc();
        while(av_read_frame(m_inCtx,m_pkt)==0){
            if(m_pkt->stream_index==audioIndex){
                deCodeAudio_PCM(m_pkt,channels);
            }
            av_packet_unref(m_pkt);
        }
        deCodeAudio_PCM(NULL,channels);
        return true;
    }
    bool deCodeAudio_PCM(AVPacket* pkt,int channels){
        if(avcodec_send_packet(m_codeCtx,pkt)!=0){
            av_log(NULL,AV_LOG_ERROR,"avcodec_send_packet failed!\n");
            return false;
        };
        while(int ret=avcodec_receive_frame(m_codeCtx,m_frame)==0){
            if(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF){
                return true;
            }
            int dataSize=av_get_bytes_per_sample(m_codeCtx->sample_fmt); //获取本机sample_fmt的占用空间的大小 4字节
            if(dataSize<0){
                av_log(NULL,AV_LOG_ERROR,"get dataSize failed!\n");
                return false;
            }
            /*
            frame fltp 2 LC
            data[0] L L L L
            data[1] R R R R
            ==> L R L R L R L R
            */
           int i,channel;
            for(i=0;i<m_frame->nb_samples;i++){ //循环采样点数
                for(channel=0;channel<channels;channel++){ //循环写入每个音频通道，实现L R L R
                    fwrite(m_frame->data[channel]+dataSize*i,1,dataSize,m_dest_fp);
                }
            }
        }
        return true;
    }

    //音频编码
    bool enCodeAudio(){
        m_frame=av_frame_alloc();
        m_frame->sample_rate=48000;
        m_frame->ch_layout.nb_channels=2;
        //m_frame->channel_layout=AV_CH_LAYOUT_STEREO;
        m_frame->ch_layout=(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
        m_frame->format=AV_SAMPLE_FMT_S16;
        m_frame->nb_samples=1024;
        av_frame_get_buffer(m_frame,0);

        const AVCodec* encoder=avcodec_find_encoder_by_name("libfdk_aac");
        if(encoder==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec_find_encoder_by_name failed!\n");
            return false;
        }
        //const AVChannelLayout *layout=encoder->ch_layouts;



        m_encodeCtx=avcodec_alloc_context3(encoder);
        if(m_encodeCtx==NULL){
            av_log(NULL,AV_LOG_ERROR,"avcodec_alloc_context3 failed!\n");
            return false;
        }
        m_encodeCtx->sample_fmt=AV_SAMPLE_FMT_S16;
        m_encodeCtx->sample_rate=m_frame->sample_rate;
        m_encodeCtx->ch_layout.nb_channels=m_frame->ch_layout.nb_channels;
        m_encodeCtx->ch_layout=m_frame->ch_layout;

        if(avcodec_open2(m_encodeCtx,encoder,NULL)<0){
            av_log(NULL,AV_LOG_ERROR,"avcodec_open2 failed!\n");
            return false;
        }

        FILE* src_fp=fopen(m_inFileName,"rb");
        if(src_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open src_fp failed!\n");
            return false;
        }
        m_dest_fp=fopen(m_outFileName,"wb+");
        if(m_dest_fp==NULL){
            av_log(NULL,AV_LOG_ERROR,"open m_dest_fp failed!\n");
            return false;
        }
        
        m_pkt=av_packet_alloc();
        //读数据
        while(1){
            if(fread(m_frame->data[0],1,m_frame->linesize[0],src_fp)==0){
                av_log(NULL,AV_LOG_INFO,"finish read infile!\n");
                break;
            }
            enCodeAudio_AAC(m_frame);
        }
        enCodeAudio_AAC(NULL);
        if(src_fp){
            fclose(src_fp);
        }
        return true;
    }


    bool enCodeAudio_AAC(AVFrame* frame){
        int ret=avcodec_send_frame(m_encodeCtx,frame);
        if(ret<0){
            av_log(NULL,AV_LOG_ERROR,"fvcodec_send_frame failed!\n");
            return false;
        }
        while(ret>=0){
            ret=avcodec_receive_packet(m_encodeCtx,m_pkt);
            if(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF){
                return true;
            }else if(ret<0){
                av_log(NULL,AV_LOG_ERROR,"vcodec_receive_packet failed!\n");
                return false;
            }
            fwrite(m_pkt->data,1,m_pkt->size,m_dest_fp);
            av_packet_unref(m_pkt);
        }
        return true;
    }
};


    //音频解码 AAC解码成PCM
int main(int argc,char** argv){

    // int num1 = std::stod(argv[3]);
    // int num2 = std::stod(argv[4]);
    //printf("%d %d\n",num1,num2);
    DEMuxer de(argv[1],argv[2]);
    //de.openInput();
    //de.getFileMesage();
    //de.getDuration();
    //de.getTimeBase();
    //de.getPtsDts();
    //de.cutFile((int)argv[3],(int)argv[4]);
    //std::cout<<de.cutFile(num1,num2);
    //std::cout<<de.deCodecYUV()<<std::endl;
    //de.destVideoSize_RGB24_BMF();
    //de.enCodeVideo();
    //de.deCodeAudio();
    de.enCodeAudio();
    return 0;
}