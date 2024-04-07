extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}
#include<fstream>

const int sampling_frequencies[] = {
    96000,  // 0x0
    88200,  // 0x1
    64000,  // 0x2
    48000,  // 0x3
    44100,  // 0x4
    32000,  // 0x5
    24000,  // 0x6
    22050,  // 0x7
    16000,  // 0x8
    12000,  // 0x9
    11025,  // 0xa
    8000   // 0xb
    // 0xc d e f是保留的
};
//adts格式头部文件
int adts_header(char * const p_adts_header, const int data_length,
                const int profile, const int samplerate,
                const int channels)
{

    int sampling_frequency_index = 3; // 默认使用48000hz
    int adtsLen = data_length + 7;
    //ADTS不是单纯的data，是hearder+data的，所以加7这个头部hearder的长度，这里7是因为后面protection absent位设为1，不做校验，所以直接加7，如果做校验，可能会是9

    int frequencies_size = sizeof(sampling_frequencies) / sizeof(sampling_frequencies[0]);
    int i = 0;
    for(i = 0; i < frequencies_size; i++)   //查找采样率
    {
        if(sampling_frequencies[i] == samplerate)
        {
            sampling_frequency_index = i;
            break;
        }
    }
    if(i >= frequencies_size)
    {
        printf("unsupport samplerate:%d\n", samplerate);
        return -1;
    }

    p_adts_header[0] = 0xff;         //syncword:0xfff                          高8bits
    p_adts_header[1] = 0xf0;         //syncword:0xfff                          低4bits
    p_adts_header[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
    p_adts_header[1] |= (0 << 1);    //Layer:0                                 2bits
    p_adts_header[1] |= 1;           //protection absent:1                     1bit

    p_adts_header[2] = (profile)<<6;            //profile:profile               2bits
    p_adts_header[2] |= (sampling_frequency_index & 0x0f)<<2; //sampling frequency index:sampling_frequency_index  4bits
    p_adts_header[2] |= (0 << 1);             //private bit:0                   1bit
    p_adts_header[2] |= (channels & 0x04)>>2; //channel configuration:channels  高1bit

    p_adts_header[3] = (channels & 0x03)<<6; //channel configuration:channels 低2bits
    p_adts_header[3] |= (0 << 5);               //original：0                1bit
    p_adts_header[3] |= (0 << 4);               //home：0                    1bit
    p_adts_header[3] |= (0 << 3);               //copyright id bit：0        1bit
    p_adts_header[3] |= (0 << 2);               //copyright id start：0      1bit
    p_adts_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits

    p_adts_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
    p_adts_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
    p_adts_header[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
    p_adts_header[6] = 0xfc;      //11111100       //buffer fullness:0x7ff 低6bits
    // number_of_raw_data_blocks_in_frame：
    //    表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。

    return 0;
}

    //提取aac音频
int main(int argc,char* argv[]){
    av_log_set_level(AV_LOG_DEBUG);//设置打印级别
    // if(argc<3){
    //     av_log(NULL,AV_LOG_ERROR,"%s infileName.\n",argv[0]);
    //     return -1;
    // }

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
    int audioIndex=av_find_best_stream(Ct,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);   //type：指定获取音频流
    if(audioIndex<0){
        char errstr[256]={0};
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        avformat_close_input(&Ct);
        return -1;
    }else{
        av_log(NULL,AV_LOG_INFO,"audio的索引为:%d\n",audioIndex);
    }

    std::fstream ofs;
    ofs.open(outfileName,std::ios::out | std::ios::binary);
    if(!ofs.is_open()){
        av_log(NULL,AV_LOG_ERROR,"文件打开失败！");
        return -1;
    }
    AVPacket *pkt;
    pkt=av_packet_alloc();    //初始化
    //循环读取
    while(av_read_frame(Ct,pkt)==0){
        if(pkt->stream_index==audioIndex){
            char adtsHeader[7]={0};
            adts_header(adtsHeader, pkt->size,
                        Ct->streams[audioIndex]->codecpar->profile,
                        Ct->streams[audioIndex]->codecpar->sample_rate,
                        2); //获取音频通道数有问题
            ofs.write(adtsHeader,sizeof(adtsHeader));
            ofs.write((char*)pkt->data,pkt->size);
        }
        av_packet_unref(pkt);
    }

    if(Ct){
        avformat_close_input(&Ct);
    }
    if(ofs){
        ofs.close();
    }
    
    return 0;
}