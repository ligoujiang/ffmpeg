extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}
//转封装（不解码）
int main(int argc,char** argv){
    if(argc<2){
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
    //创建输出流
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
        if(inStream->codecpar->codec_type!=AVMEDIA_TYPE_VIDEO&&
            inStream->codecpar->codec_type!=AVMEDIA_TYPE_AUDIO&&
            inStream->codecpar->codec_type!=AVMEDIA_TYPE_SUBTITLE){
                handleStreamIndexArray[i]=-1;
                continue;
        }
        handleStreamIndexArray[i]=streamIndex++;
        

    }


fail:
    if(inCtx){
        avformat_close_input(&inCtx);
    }
    return 0;
}
