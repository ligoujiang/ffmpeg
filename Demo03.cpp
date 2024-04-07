extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
}
#include<fstream>

   //提取h264视频数据
int main(int argc,char* argv[]){
    av_log_set_level(AV_LOG_DEBUG);//设置打印级别
    if(argc<3){
        av_log(NULL,AV_LOG_ERROR,"%s infileName.\n",argv[0]);
        return -1;
    }

    const char* infileName=argv[1]; //输入文件
    const char* outfileName=argv[2];   //输出文件
    AVFormatContext* Ct=NULL;
    //打开文件
    int ret=avformat_open_input(&Ct,infileName,NULL,NULL);
    if(ret!=0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        return -1;
    }

    ret=avformat_find_stream_info(Ct,NULL); //如果获取失败，文件可能有问题
    if(ret<0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        avformat_close_input(&Ct);
        return -1;
    }
    //获取音视频流
    int videoIndex=av_find_best_stream(Ct,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);   //type：指定获取视频流
    if(videoIndex<0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        avformat_close_input(&Ct);
        return -1;
    }else{
        av_log(NULL,AV_LOG_INFO,"video的索引为:%d\n",videoIndex);
    }

    std::fstream ofs;
    ofs.open(outfileName,std::ios::out | std::ios::binary);
    if(!ofs.is_open()){
        av_log(NULL,AV_LOG_ERROR,"文件打开失败！");
        return -1;
    }


    AVPacket *pkt;
    pkt=av_packet_alloc();    //初始化

    //h264有两种格式,需要转成对应的格式播放器才能播放
    const AVBitStreamFilter* bsf=av_bsf_get_by_name("h264_mp4toannexb");
    AVBSFContext *bsfCt=NULL;
    av_bsf_alloc(bsf,&bsfCt);
    avcodec_parameters_copy(bsfCt->par_in,Ct->streams[videoIndex]->codecpar);
    av_bsf_init(bsfCt);

    while(av_read_frame(Ct,pkt)==0){
        if(pkt->stream_index==videoIndex){
            if(av_bsf_send_packet(bsfCt,pkt)==0){
                while(av_bsf_receive_packet(bsfCt,pkt)==0){
                    ofs.write((char*)pkt->data,pkt->size);
                }
            }
        }
        av_packet_unref(pkt);
    }
    if(Ct){
        avformat_close_input(&Ct);
    }
    if(bsfCt){
        av_bsf_free(&bsfCt);
    }
    if(ofs){
        ofs.close();
    }
    
    return 0;
}