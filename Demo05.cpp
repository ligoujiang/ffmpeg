extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}
#include <iostream>

class DEMuxer{
private:
    //输入和输出参数变量
    const char* m_inFileName=nullptr;
    const char* m_outFileName=nullptr;
    //输入和输出句柄
    AVFormatContext* m_inCtx=nullptr;
    AVFormatContext* m_outCtx=nullptr;
public:
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
};
    //获取时间基和时间戳
int main(int argc,char** argv){

    DEMuxer de(argv[1],argv[2]);
    std::cout<<de.openInput()<<std::endl;
    std::cout<<de.getFileMesage()<<std::endl;
    de.getDuration();
    de.getTimeBase();
    de.getPtsDts();
    return 0;
}