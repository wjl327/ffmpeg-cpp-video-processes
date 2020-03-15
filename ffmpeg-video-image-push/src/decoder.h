#ifndef HELLO_DECODE_DECODE_H
#define HELLO_DECODE_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/avfiltergraph.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sys/time.h>
#include <sys/select.h>

class Decoder {

public:
    void decode(std::string);

private:
    int initDecode();
    int initEncode();
    int initPusher(std::string);
    int decoding();
    bool isExistFile(std::string);
    std::string getImageName(int);
    long getImageSize(std::string);
    void closeDecode();

    int decodeJpg(int64_t pts);
    int encodeYuvToH264(int64_t pts);
    uint64_t getCurTimestamp();

private:

    //decode
    AVPacket *pPacket = nullptr;
    AVCodecParserContext *parserJpg = nullptr;
    AVCodecContext *pCodecCtx = nullptr;
    AVFrame *pFrame = nullptr;

    //encode
    bool first = true;
    int encodeFps = 5;
    int frame_step = 200;
    AVCodec *h264Codec = nullptr;
    AVDictionary *h264Dict = nullptr;
    AVCodecContext *h264CodecCtx = nullptr;
    AVPacket *h264Packet = nullptr;

    //pushstream
    int outVideoindex = -1;
    AVFormatContext *outFormatCtx = nullptr;

};

enum Constat {
    ok = 0,
    system_error = -1
};

static void sleep_ms(unsigned int secs){

    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}

#endif //HELLO_DECODE_DECODE_H
