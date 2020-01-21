#ifndef HELLO_DECODE_DECODE_H
#define HELLO_DECODE_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

};

enum Constat {
    ok = 0,
    system_error = -1
};

#endif //HELLO_DECODE_DECODE_H
