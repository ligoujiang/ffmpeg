extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/parseutils.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <iostream>
#include <string>


/*
sws_scale()函数
有可能不一致导致视频不能播放，可使width与linesize宽度一致
1.图像色彩空间转换（yuv各格式之间的转换或者转换成RGB）
2.分辨率缩放
3.前后图像滤镜波处理
*/


//int count=0;
class DEMuxer{
private:
    //输入和输出参数变量
    const char* m_inFileName=nullptr;
    const char* m_outFileName=nullptr;
    //输入和输出句柄
    AVFormatContext* m_inCtx=nullptr;
    AVFormatContext* m_outCtx=nullptr;
    //创建解码器句柄
    AVCodecContext* m_codeCtx=nullptr;
    //打开输出文件
    FILE* m_ofs=nullptr;
    //更改分辨率
    char* m_destVideoSizeString=nullptr; //接收传入的参数，格式为1920x1080
    int m_destWidth=0;
    int m_destHeight=0;
    //修改视频分辨率句柄
    SwsContext *m_swsCtx=nullptr;
public:
    DEMuxer(char* argv1,char* argv2,char* argv3){
        m_inFileName=argv1;
        m_outFileName=argv2;
        m_destVideoSizeString=argv3;
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
        if(m_outCtx && !(m_outCtx->oformat->flags & AVFMT_NOFILE)){
            avio_closep(&m_outCtx->pb);
        }
        if(m_codeCtx){
            avcodec_free_context(&m_codeCtx);
            m_codeCtx=nullptr;
            std::cout<<"m_codeCtx已被释放！"<<std::endl;
        }
        if(m_ofs){
            fclose(m_ofs);
            m_ofs=nullptr;
            std::cout<<"m_ofs已被释放！"<<std::endl;
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
        m_ofs=fopen(m_outFileName,"wb+");
        if(m_ofs==NULL){
            av_log(NULL,AV_LOG_ERROR,"open FILE ofs failed!\n");
            return false;
        }        
        AVPacket *pkt=av_packet_alloc();
        //AVFrame *frame=av_frame_alloc();
        while(av_read_frame(m_inCtx,pkt)==0){
            if(pkt->stream_index==videoIndex){
                if(decodeVideo(m_codeCtx,pkt,m_ofs)==0){
                    return false;
                };               
            }
            av_packet_unref(pkt);
        }
        //flush decoder
        decodeVideo(m_codeCtx,NULL,m_ofs);
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
        m_ofs=fopen(m_outFileName,"wb+");
        if(m_ofs==NULL){
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
            fwrite(destframe->data[0],1,m_destWidth*m_destHeight,m_ofs);    //写入Y数据
            fwrite(destframe->data[1],1,m_destWidth*m_destHeight/4,m_ofs);    //写入U数据
            fwrite(destframe->data[2],1,m_destWidth*m_destHeight/4,m_ofs);    //写入V数据
            av_log(NULL,AV_LOG_INFO,"linesize[0]=%d,linesize[1]=%d,linesize[2]=%d,width=%d,height=%d\n",destframe->linesize[0],destframe->linesize[1],destframe->linesize[2],
            m_destWidth,m_destHeight);
        }  
        if(frame){
            av_frame_free(&frame);
        }
        return true;
    }
};
    //解码视频提取YUV  
int main(int argc,char** argv){

    // int num1 = std::stod(argv[3]);
    // int num2 = std::stod(argv[4]);
    //printf("%d %d\n",num1,num2);
    DEMuxer de(argv[1],argv[2],argv[3]);
    std::cout<<de.openInput()<<std::endl;
    std::cout<<de.getFileMesage()<<std::endl;
    //de.getDuration();
    //de.getTimeBase();
    //de.getPtsDts();
    //de.cutFile((int)argv[3],(int)argv[4]);
    //std::cout<<de.cutFile(num1,num2);
    //std::cout<<de.deCodecYUV()<<std::endl;
    de.destVideoSize();
    return 0;
}