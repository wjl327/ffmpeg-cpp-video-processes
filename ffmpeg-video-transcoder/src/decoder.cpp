#include <iostream>
#include <sstream>
#include "decoder.h"
#include "time_util.h"

using namespace std;

void Decoder::decode(std::string source, std::string desc) {

    decodeId++;
    decodeCount = 0;

    int ret;
    if ((ret = initDecode(source)) < 0) {
        cout << "init decode fail :" << ret << endl;
        return;
    }

    if ((ret = initEncode(desc)) < 0) {
        cout << "init encode fail :" << ret << endl;
        return;
    }

//    if ((ret = initFilter()) < 0) {
//        cout << "init filter fail :" << ret << endl;
//        return;
//    }

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

int Decoder::initEncode(std::string desc) {

    int ret;
    if((ret = avformat_alloc_output_context2(&outFormatCtx, NULL, "rtsp", desc.c_str())) < 0) {
        cout << "avformat_alloc_output_context2 failed, ret: " << ret << endl;
        return Constat::system_error;
    }

    outFormat = outFormatCtx->oformat;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {

        //Create output AVStream according to input AVStream
        AVStream *in_stream = pFormatCtx->streams[i];
        AVStream *out_stream = avformat_new_stream(outFormatCtx, in_stream->codec->codec);
        if (!out_stream) {
            cout << "Failed allocating output stream " << endl;
            return Constat::system_error;
        }

        //Copy the settings of AVCodecContext
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (in_stream->codec->codec_id == AV_CODEC_ID_NONE) {
            out_stream->codec->codec_id = AV_CODEC_ID_AAC;
        }

        if (ret < 0) {
            cout << "Failed to copy context from input to output stream codec context!" << endl;
            return Constat::system_error;
        }

        out_stream->codec->codec_tag = 0;
        if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    //Dump Format------------------
    av_dump_format(outFormatCtx, 0, desc.c_str(), 1);

    if (!(outFormat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFormatCtx->pb, desc.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            cout << "Could not open output URL: " << desc << endl;
            return ret;
        }
    }

    av_dict_set(&dict, "rtsp_transport","tcp",0);
    av_dict_set(&dict, "muxdelay", "0.1", 0);

    outFormatCtx->audio_codec_id = outFormatCtx->oformat->audio_codec;
    outFormatCtx->video_codec_id = outFormatCtx->oformat->video_codec;
    ret = avformat_write_header(outFormatCtx, &dict);
    if (ret < 0) {
        cout << "Error occurred when opening output URL: " << desc << endl;
        return ret;
    }

    cout << "init encode succ !!!" << endl;

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

    start_time = av_gettime();
    while(1) {

        AVStream *in_stream, *out_stream;
        
        int ret = av_read_frame(pFormatCtx, &packet);
        if (ret != 0) {
            if (ret == AVERROR_EOF) {
                cout << "av_read_frame failed, ret: " << ret << endl;
            } else {
                cout << "av_read_frame ret eof!" << endl;
            }
            break;
        }

        //FIX：No PTS (Example: Raw H.264)
        //如果容器没有提供pts/dts，则需要填充它们
        if (packet.pts == AV_NOPTS_VALUE) {
            AVRational time_base = pFormatCtx->streams[videoStream]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);
            //Parameters
            packet.pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base)*AV_TIME_BASE);
            packet.dts = packet.pts;
            packet.duration = (double)calc_duration / (double)(av_q2d(time_base)*AV_TIME_BASE);
        }

        //Important:Delay 延迟
//        if (packet.stream_index == videoStream) {
//            AVRational time_base = pFormatCtx->streams[videoStream]->time_base;
//            AVRational time_base_q = { 1, AV_TIME_BASE };
//            int64_t pts_time = av_rescale_q(packet.dts, time_base, time_base_q);
//            int64_t now_time = av_gettime() - start_time;
//            if (pts_time > now_time) {
//                av_usleep(pts_time - now_time);
//            }
//        }

        if (packet.stream_index != videoStream) {
            av_packet_unref(&packet);
            continue;
        }

        in_stream = pFormatCtx->streams[packet.stream_index];
        out_stream = outFormatCtx->streams[packet.stream_index];

        /* copy packet */
        packet.pos = -1;
        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        if (packet.stream_index == videoStream) {
            frame_index++;
        }

        ret = av_interleaved_write_frame(outFormatCtx, &packet);
        if (ret < 0) {
            cout << "error muxing packet, ret:" << ret << endl;
            break;
        }
        av_free_packet(&packet);


    }

    av_packet_unref(&packet);

    //Write file trailer
    av_write_trailer(outFormatCtx);

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