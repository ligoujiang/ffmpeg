#include <stdio.h>

extern "C" {  
 
#include <libavformat/avformat.h>  

}  
  
int main(int argc,char** argv) {  
    //av_register_all(); // 初始化所有注册的编解码器  //此函数已被废弃，新版本会自动注册
    // ... 你的代码，使用FFmpeg API ...  
    //avcodec_cleanup(); // 清理并关闭FFmpeg库  //此函数已被废弃，新版本会自动关闭
    // av_log_set_level(AV_LOG_DEBUG);
    // av_log(NULL,AV_LOG_ERROR,"this is error log level\n");
    // av_log(NULL,AV_LOG_INFO,"this is INFO log level\n");
    // av_log(NULL,AV_LOG_WARNING,"this is WARNING log level\n");
    // av_log(NULL,AV_LOG_DEBUG,"this is DEBUG log level\n");

    const char *default_filename = "believe.flv";

    char *in_filename = NULL;

    if(argv[1] == NULL)
    {
        //in_filename = default_filename;
    }
    else
    {
        in_filename = argv[1];
    }
    printf("in_filename = %s\n", in_filename);

    AVFormatContext *ifmt_ctx = NULL;           // 输入文件的demux

    int videoindex = -1;        // 视频索引
    int audioindex = -1;        // 音频索引


    // 打开文件，主要是探测协议类型，如果是网络文件则创建网络链接
    int ret = avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL);

    ret = avformat_find_stream_info(ifmt_ctx, NULL);

    //打开媒体文件成功
    printf("\n==== av_dump_format in_filename:%s ===\n", in_filename);
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    printf("\n==== av_dump_format finish =======\n\n");
    // url: 调用avformat_open_input读取到的媒体文件的路径/名字
    printf("media name:%s\n", ifmt_ctx->url);
    // nb_streams: nb_streams媒体流数量
    printf("stream number:%d\n", ifmt_ctx->nb_streams);
    // bit_rate: 媒体文件的码率,单位为bps
    printf("media average ratio:%ldkbps\n",(int64_t)(ifmt_ctx->bit_rate/1024));
    return 0;  
}