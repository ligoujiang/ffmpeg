extern "C" { 
#include <libavformat/avformat.h>  
#include <libavutil/avutil.h>  
}

#include <iostream>

// C++中使用av_err2str宏
// static char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
// #define av_err2str(errnum) av_make_error_string((char[AV_ERROR_MAX_STRING_SIZE]){0},AV_ERROR_MAX_STRING_SIZE,errnum)



//输出Metadata源文件信息
int main(int argc,char** argv){
    av_log_set_level(AV_LOG_DEBUG);//设置打印级别
    if(argc<2){
        av_log(NULL,AV_LOG_ERROR,"%s infileName.\n",argv[0]);
        return -1;
    }

    const char* infileName=argv[1];
    AVFormatContext* pFormatCtx=NULL;

    //打开
    int ret=avformat_open_input(&pFormatCtx,infileName,NULL,NULL);
    if(ret!=0){
        char errstr[256]={0};
        //av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,av_err2str(ret));    //c++使用不了av_err2str，可以使用如下代替
        av_strerror(ret,errstr,sizeof(errstr));
        av_log(NULL,AV_LOG_ERROR,"open input file:%s failed:%s\n",infileName,errstr);
        return -1;
    }
    //获取
    av_dump_format(pFormatCtx,0,infileName,0);
    //关闭
    avformat_close_input(&pFormatCtx);
    return 0;
}