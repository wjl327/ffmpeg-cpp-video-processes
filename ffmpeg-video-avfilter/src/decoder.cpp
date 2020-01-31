#include <iostream>
#include <sstream>
#include "decoder.h"
#include "time_util.h"

using namespace std;

void Decoder::decode(int num, std::string source) {

    decodeId++;
    decodeCount = num;

    int ret;
    if ((ret = initDecode(source)) < 0) {
        cout << "init decode fail :" << ret << endl;
        return;
    }

    if ((ret = initFilter()) < 0) {
        cout << "init filter fail :" << ret << endl;
        return;
    }

    if ((ret = decoding()) < 0) {
        cout << "decoding fail :" << ret << endl;
        return;
    }

    closeDecode();
}

int Decoder::initDecode(std::string source) {

    av_register_all();

    int ret;
    if ((ret = avformat_network_init()) != 0) {
        cout << "avformat_network_init failed, ret: " << ret << endl;
        return ret;
    }

    ret = avformat_open_input(&pFormatCtx, source.c_str(), nullptr, &pAvDict);
    if (ret != 0) {
        cout << "avformat_open_input failed, ret: " << ret << endl;
        return ret;
    }

    ret = avformat_find_stream_info(pFormatCtx, nullptr);
    if (ret < 0) {
        cout << "avformat_find_stream_info failed, ret: " << ret << endl;
        return ret;
    }

    if ((ret = findVideoStreamIndex()) < 0) {
        cout << "findVideoStreamIndex failed, ret: " << ret << endl;
        return ret;
    }

    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    AVCodec *pCodec = nullptr;
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL) {
        cout << "Unsupported codec!" << endl;
        return Constat::system_error; // Codec not found
    }

    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        cout << "Couldn't copy codec context!" << ret << endl;
        return Constat::system_error;
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        cout << "Could not open codec!" << ret << endl;
        return Constat::system_error;
    }

    if(pCodecCtx->framerate.den > 0) {
        int fps = (int)(pCodecCtx->framerate.num / pCodecCtx->framerate.den);
        if(decodeFps == 0) {
            decodeFps = fps;
        }
        skip = fps / decodeFps;
    }

    // Allocate video frame
    pFrame=av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameOut=av_frame_alloc();

    // Determine required buffer size and allocate buffer
    imgWidth = pCodecCtx->width;
    imgHeight = pCodecCtx->height;
    imgSize = avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
    buffer = (uint8_t *)av_malloc(imgSize * sizeof(uint8_t));

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_BGR24, imgWidth, imgHeight);

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                             pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                             AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    cout << "open stream succ, source: " << source << ", find video stream idx: " << videoStream
    << ", width: " << imgWidth << ", height: " << imgHeight << ", size: " << imgSize << ", skip: " << skip
    << ", decodeCount: " << decodeCount << endl;
    return Constat::ok;
}

int Decoder::initFilter() {

    avfilter_register_all();

    char args[512];
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    filterGraph = avfilter_graph_alloc();
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
             pCodecCtx->time_base.num, pCodecCtx->time_base.den,
             pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);

    int ret = avfilter_graph_create_filter(&buffersrcCtx, buffersrc, "in", args, NULL, filterGraph);
    if (ret < 0) {
        cout << "Cannot create buffer source!" << endl;
        return ret;
    }

    /* buffer video sink: to terminate the filter chain. */
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out", NULL, buffersink_params, filterGraph);
    av_free(buffersink_params);
    if (ret < 0) {
        cout << "Cannot create buffer sink!" << endl;
        return ret;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrcCtx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersinkCtx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filterGraph, filtersDesc, &inputs, &outputs, NULL)) < 0){
        cout << "avfilter_graph_parse_ptr failed, ret: " << ret << endl;
        return ret;
    }

    if ((ret = avfilter_graph_config(filterGraph, NULL)) < 0) {
        cout << "avfilter_graph_config failed, ret: " << ret << endl;
        return ret;
    }

    filterCtx = filterGraph->filters[2];
    return Constat::ok;
}

int Decoder::findVideoStreamIndex() {
    for (size_t i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            return 0;
        }
    }
    return Constat::system_error;
}


int Decoder::decoding() {

    AVPacket packet;
    av_init_packet(&packet);
    int got_frame = 0;

    while(1) {

        int ret = av_read_frame(pFormatCtx, &packet);
        if (ret != 0) {
            if (ret == AVERROR_EOF) {
                cout << "av_read_frame failed, ret: " << ret << endl;
            } else {
                cout << "av_read_frame ret eof!" << endl;
            }
            break;
        }

        if (packet.stream_index != videoStream) {
            av_packet_unref(&packet);
            continue;
        }

        if ((ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_frame, &packet)) < 0) {
            cout << "avcodec_decode_video2 ret: " << ret << endl;
            av_packet_unref(&packet);
            continue;
        }

        pts = packet.pts;
        if (got_frame > 0) {

            if(decodeCount == 0) {
                cout << "decode finish!!!" << endl;
                break;
            }

            if(skip == 0 || decodeFrameNum % skip == 0) {

                //avfilter
                pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);

                //av_opt_set
                stringstream ss;
                ss << "avfilter - " << getFormatTime();
                av_opt_set(filterCtx->priv, "text", ss.str().c_str(), 0);

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame(buffersrcCtx, pFrame) < 0) {
                    cout << "Error while feeding the filtergraph!" << endl;
                    break;
                }

                /* pull filtered pictures from the filtergraph */
                if (av_buffersink_get_frame(buffersinkCtx, pFrameOut) < 0) {
                    cout << "Error get filter frame the filtergraph!" << endl;
                    break;
                }

                decodeCount--;
                sws_scale(sws_ctx, (const uint8_t *const *)pFrameOut->data,
                          pFrameOut->linesize, 0, imgHeight, pFrameRGB->data, pFrameRGB->linesize);
                //saveImage(getImageName());

                av_frame_unref(pFrame);
                av_frame_unref(pFrameOut);
            }

            decodeFrameNum++;
            cout << "time: " << getFormatTime() << " decodeFrameNum: " << decodeFrameNum << endl;
        }

        av_packet_unref(&packet);
    }
    return Constat::ok;
}

void Decoder::saveImage(std::string filename) {
    cv::Mat mat;
    mat.create(cv::Size(imgWidth, imgHeight), CV_8UC3);
    mat.data = buffer;
    if (!cv::imwrite(filename, mat)) {
        cout<< "saveImage failed, filename: " << filename << ", pts: " << pts << endl;
    } else {
        cout<< "saveImage success, filename: " << filename << ", pts: " << pts << endl;
    }
}

std::string Decoder::getImageName() {
    time_t ts = time(NULL);
    std::stringstream ss;
    ss << "/tmp/";
    ss << decodeId;
    ss << "_";
    ss << ts;
    ss << "_";
    ss << decodeFrameNum;
    ss << ".";
    ss << "jpg";
    return ss.str();
}

void Decoder::closeDecode() {

    avfilter_graph_free(&filterGraph);

    av_frame_free(&pFrameOut);
    av_frame_free(&pFrameRGB);
    av_frame_free(&pFrame);

    avcodec_close(pCodecCtx);
    avcodec_free_context(&pCodecCtx);

    avcodec_close(pCodecCtxOrig);
    //avcodec_free_context(&pCodecCtxOrig); //Mustn't free

    //avfilter_free(filterCtx);

    avformat_close_input(&pFormatCtx);
    av_dict_free(&pAvDict);
}