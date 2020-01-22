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
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

class Decoder {

public:
    void decode(int, std::string);

private:
    int initDecode(std::string);
    int initFilter();
    int findVideoStreamIndex();
    int decoding();
    std::string getImageName();
    void saveImage(std::string);
    void closeDecode();

private:

    int videoStream = -1;
    AVDictionary *pAvDict = nullptr;
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pCodecCtxOrig = nullptr;
    AVCodecContext *pCodecCtx = nullptr;
    SwsContext *sws_ctx = nullptr;

    AVFrame *pFrame = nullptr;
    AVFrame *pFrameRGB = nullptr;
    AVFrame *pFrameOut = nullptr;
    uint8_t *buffer = nullptr;

    int imgWidth = 0;
    int imgHeight = 0;
    int imgSize = 0;
    int decodeFps = 1;
    int skip = 0;
    int pts = 0;

    int decodeId = 0;
    int decodeCount = 0;
    int decodeFrameNum = 0;

    AVFilterGraph *filterGraph;
    AVFilterContext *buffersrcCtx;
    AVFilterContext *buffersinkCtx;
    AVFilterContext *filterCtx;

    //char *filtersDesc = "drawtext=fontfile=/usr/share/fonts/dejavu/DejaVuSans.ttf:fontcolor=blue:x=300:y=300:fontsize=45:text='hello avfilter'";
    //char *filtersDesc = "movie=test.png [watermark]; [in][watermark] overlay=0:0 [out]";
    char *filtersDesc = "[in]drawtext=fontfile=/usr/share/fonts/dejavu/DejaVuSans.ttf:fontcolor=blue:x=300:y=300:fontsize=45:text='hello avfilter'[text];movie=test.png[wm];[text][wm]overlay=0:0[out]";

};

enum Constat {
    ok = 0,
    system_error = -1
};

#endif //HELLO_DECODE_DECODE_H
