#include <iostream>
#include <sstream>
#include "decoder.h"
#include "time_util.h"

using namespace std;

void Decoder::decode(std::string source, std::string desc) {

    int ret;
    if ((ret = initDecode(source)) < 0) {
        cout << "init decode fail :" << ret << endl;
        return;
    }

    if ((ret = initEncode()) < 0) {
        cout << "init encode fail :" << ret << endl;
        return;
    }

    if ((ret = initFilter()) < 0) {
        cout << "init filter fail :" << ret << endl;
        return;
    }

    if ((ret = initPusher(desc)) < 0) {
        cout << "init pusher fail :" << ret << endl;
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

    //打开视频文件：读取文件头、文件格式等信息存储到AVFormatContext。
    ret = avformat_open_input(&pFormatCtx, source.c_str(), nullptr, &pAvDict);
    if (ret != 0) {
        cout << "avformat_open_input failed, ret: " << ret << endl;
        return ret;
    }

    //搜索流信息：读取一段视频文件数据，尝试解码，将取到的流信息存储到AVFormatContext。nb_streams(流的数量)和streams(流的指针数据)。
    ret = avformat_find_stream_info(pFormatCtx, nullptr);
    if (ret < 0) {
        cout << "avformat_find_stream_info failed, ret: " << ret << endl;
        return ret;
    }

    //查找视频流索引编号
    if ((ret = findVideoStreamIndex()) < 0) {
        cout << "findVideoStreamIndex failed, ret: " << ret << endl;
        return ret;
    }

    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    // codec_id 是编解码器的id
    // 根据编解码器id 获取解码器 AVCodec
    // Find the decoder for the video stream
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL) {
        cout << "Unsupported codec!" << endl;
        return Constat::system_error; // Codec not found
    }

    //分配AVCodecContext并初始化默认值
    pCodecCtx = avcodec_alloc_context3(pCodec);
    //将源AVCodecContext的设置复制到目标AVCodecContext中。
    //得到的"目标编解码器上下文"将是未打开的，必须调用avcodec_open2()才能打开它；才能用它编解码音视频。
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        cout << "Couldn't copy codec context!" << ret << endl;
        return Constat::system_error;
    }

    // Open codec
    //初始化AVCodecContext以使用给定的AVCodec。
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        cout << "Could not open codec!" << ret << endl;
        return Constat::system_error;
    }

    //framerate帧速率 分子/分母 分开存储，相除才是最终值fps
    //cout << "framerate.num: " << pCodecCtx->framerate.num << endl;
    //cout << "framerate.den: " << pCodecCtx->framerate.den << endl;
    //cout << "fps: " << (int)(pCodecCtx->framerate.num / pCodecCtx->framerate.den) << endl;
    //cout << "pix_fmt is AV_PIX_FMT_YUV420P : " << (pCodecCtx->pix_fmt == AV_PIX_FMT_YUV420P) << endl;

    //可以通过设定解码速率来控制解码跳帧
    if(pCodecCtx->framerate.den > 0) {
        decodeFps = (int)(pCodecCtx->framerate.num / pCodecCtx->framerate.den);
        if(encodeFps == 0) {
            encodeFps = decodeFps;
        }
        skip = decodeFps / encodeFps;
    }

    //创建AVFrame来存放从AVPacket中解码出来的原始数据。但是av_frame_alloc并不会分配数据的缓存空间。
    pFrame = av_frame_alloc();

    //解码得到原始的分辨率
    imgWidth = pCodecCtx->width;
    imgHeight = pCodecCtx->height;
    //imgSize = avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);

    cout << "open stream succ, source: " << source << ", find video stream idx: " << videoStream
    << ", decodeFps: " << decodeFps << ", encodeFps: " << encodeFps << ", skip: " << skip
    << ", width: " << imgWidth << ", height: " << imgHeight << endl;
    return Constat::ok;
}

int Decoder::initEncode() {

    //获取h264编码器
    h264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!h264Codec){
        cout << "avcodec_find_encoder failed" << endl;
        return Constat::system_error;
    }

    //创建编解码器的context，context可以用来设置编解码过程所需要的各种配置信息
    h264CodecCtx = avcodec_alloc_context3(h264Codec);
    if (!h264CodecCtx){
        cout << "avcodec_find_encoder failed" << endl;
        return Constat::system_error;
    }

    //h264CodecCtx->bit_rate = 400000; //码率。这里设置固定码率应该没啥用。
    h264CodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;    //编码的原始数据格式
    h264CodecCtx->codec_type = AVMEDIA_TYPE_VIDEO; //指定为视频编码
    h264CodecCtx->width  = imgWidth;      //分辨率宽
    h264CodecCtx->height = imgHeight;     //分辨率高
    h264CodecCtx->channels = 0;           //音频通道数
    h264CodecCtx->time_base = {1, encodeFps};  //时间基，表示每个时间刻度是多少秒。
    h264CodecCtx->framerate = {encodeFps, 1};  //帧率，没秒
    h264CodecCtx->gop_size = 2 * encodeFps;    //图像组两个关键帧（I帧）的距离，也就是一组帧的数量 原来是10
    h264CodecCtx->max_b_frames = 5;            //指定B帧数量，B帧是双向参考帧，填充更多B帧，压缩率更高但是延迟也高
    h264CodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //暂时不明


    //av_opt_set(h264CodecCtx->priv_data, "preset", "slow", 0);       //慢速压缩编码，慢的可以保证视频质量
    //av_opt_set(h264CodecCtx->priv_data, "preset", "veryfast", 0);
    av_opt_set(h264CodecCtx->priv_data, "preset", "ultrafast", 0);    //快速编码，但会损失质量
    //av_opt_set(h264CodecCtx->priv_data, "tune", "zerolatency", 0);  //适用于快速编码和低延迟流式传输,但是会出现绿屏

    //打开编码器
    if (avcodec_open2(h264CodecCtx, h264Codec, NULL) < 0){
        cout << "fail: avcodec_open2" << endl;
        return false;
    }

    h264Frame = av_frame_alloc();
    h264ImgSize = avpicture_get_size(h264CodecCtx->pix_fmt, h264CodecCtx->width, h264CodecCtx->height);
    h264Buffer = (uint8_t *)av_malloc(h264ImgSize * sizeof(uint8_t));
    avpicture_fill((AVPicture *)h264Frame, h264Buffer, h264CodecCtx->pix_fmt, h264CodecCtx->width, h264CodecCtx->height);

    cout << "init encode succ !!!" << endl;
    return Constat::ok;
}

int Decoder::initFilter() {

    //注册所有过滤器
    avfilter_register_all();

    //获取输入输出过滤器AVFilter的实例
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc(); //TODO: Must be freed with avfilter_inout_free()
    AVFilterInOut *inputs  = avfilter_inout_alloc(); //TODO: Must be freed with avfilter_inout_free()
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };


    //avfilter_graph_create_filter初始化输入的AVFilterContext即AVFilter的上下文。
    int ret;
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
             pCodecCtx->time_base.num, pCodecCtx->time_base.den,
             pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);
    filterGraph = avfilter_graph_alloc();
    ret = avfilter_graph_create_filter(&buffersrcCtx, buffersrc, "in", args, NULL, filterGraph);
    if (ret < 0) {
        cout << "avfilter_graph_create_filter buffersrc failed:" << ret << endl;
        return ret;
    }

    //avfilter_graph_create_filter初始化输出的AVFilterContext即AVFilter的上下文。
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out", NULL, buffersink_params, filterGraph);
    av_free(buffersink_params);
    if (ret < 0) {
        cout << "avfilter_graph_create_filter buffersink failed:" << ret << endl;
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

    cout << "init filter succ !!!" << endl;
    return Constat::ok;
}

int Decoder::initPusher(std::string desc) {

    //初始化rtsp推流的AVFormatContext
    int ret;
    if ((ret = avformat_alloc_output_context2(&outFormatCtx, NULL, "rtsp", desc.c_str())) < 0){
        cout << "avformat_alloc_output_context2 failed, ret: " << ret << endl;
        return Constat::system_error;
    }
    //检查所有流是否都有数据，如果没有数据会等待最大时间，单位微秒。
    outFormatCtx->max_interleave_delta = 1000000;

    //创建一个视频流AVStream
    AVStream *outAvStream = avformat_new_stream(outFormatCtx, h264Codec);
    if (!outAvStream){
        cout << "avformat_new_stream failed!" << endl;
        return Constat::system_error;
    }

    //记录视频流的id。这里肯定是0，因为也只是创建了一个视频流
    outVideoindex = outFormatCtx->nb_streams - 1;

    //输出流的时间基
    outAvStream->time_base = { 1, encodeFps };

    //设置流Id。
    outAvStream->id = outFormatCtx->nb_streams - 1;

    //复制编码器AVCodecContext上下文到输出流的codec字段
    if (avcodec_copy_context(outAvStream->codec, h264CodecCtx) < 0) {
        cout << "failed avcodec_copy_context" << endl;
        return Constat::system_error;
    }

    //这是用来解决一些编码器的错误。先传0。
    outAvStream->codec->codec_tag = 0;

    //暂时不懂
    if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        outAvStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    //流拷贝编码器的参数配置
    avcodec_parameters_from_context(outAvStream->codecpar, h264CodecCtx);

    av_dump_format(outFormatCtx, 0, outFormatCtx->filename, 1);

    //如果不是文件，则不能调用avio_open进行打开，
    if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        //打开输出URL
        if (avio_open(&outFormatCtx->pb, outFormatCtx->filename, AVIO_FLAG_WRITE) < 0) {
            cout << "avio_open failed, file: " << desc << endl;
            return Constat::system_error;
        }
    }

    //使用tcp协议传输 这是一种参数设置方式
    //av_opt_set(outFormatCtx->priv_data, "rtsp_transport", "tcp", 0);

    //另外一种参数设置方式
    av_dict_set(&h264Dict, "bufsize", "10240", 0);
    av_dict_set(&h264Dict, "stimeout", "2000000", 0);
    av_dict_set(&h264Dict, "rtsp_transport","tcp",0);
    av_dict_set(&h264Dict, "muxdelay", "0.1", 0);
    av_dict_set(&h264Dict, "tune", "zerolatency", 0);
    outFormatCtx->audio_codec_id = outFormatCtx->oformat->audio_codec;
    outFormatCtx->video_codec_id = outFormatCtx->oformat->video_codec;
    ret = avformat_write_header(outFormatCtx, &h264Dict);
    if (ret < 0) {
        cout << "error occurred when opening output url: " << desc << endl;
        return ret;
    }

    cout << "init pusher succ !!!" << endl;
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
    AVPacket h264Packet;
    av_init_packet(&packet);
    av_init_packet(&h264Packet);
    //h264Packet = av_packet_alloc();

    int ret = 0;
    int got_frame = 0;
    while(1) {

        //读取一帧
        if ((ret = av_read_frame(pFormatCtx, &packet)) != 0) {
            if (ret == AVERROR_EOF) {
                cout << "av_read_frame failed, ret: " << ret << endl;
            } else {
                cout << "av_read_frame ret eof!" << endl;
            }
            break;
        }

        //判断解码包的流是视频流
        if (packet.stream_index != videoStream) {
            av_packet_unref(&packet);
            continue;
        }

        //1、解码帧
        if ((ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_frame, &packet)) < 0) {
            cout << "avcodec_decode_video2 ret: " << ret << endl;
            av_packet_unref(&packet);
            continue;
        }
        if(!got_frame){
            cout << "avcodec_decode_video2 got_frame: " << got_frame << endl;
            av_packet_unref(&packet);
            continue;
        }

        if(skip == 0 || decodeFrameNum % skip == 0) {

            //2、avfilter逻辑
            //获取解码后的pts - 显示时间
            pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);

            stringstream ss;
            ss << "avfilter - " << getFormatTime();
            av_opt_set(filterCtx->priv, "text", ss.str().c_str(), 0);

            // push the decoded frame into the filtergraph
            if (av_buffersrc_add_frame(buffersrcCtx, pFrame) < 0) {
                cout << "av_buffersrc_add_frame failed!!" << endl;
                break;
            }

            // pull filtered pictures from the filtergraph
            if (av_buffersink_get_frame(buffersinkCtx, h264Frame) < 0) {
                cout << "av_buffersink_get_frame failed!!" << endl;
                break;
            }

            //3、编码帧
            ret = avcodec_send_frame(h264CodecCtx, h264Frame);
            if (ret < 0){
                cout << "avcodec_send_frame failed, ret:" << ret << endl;
                return Constat::system_error;
            }

            while (ret >= 0) {

                ret = avcodec_receive_packet(h264CodecCtx, &h264Packet);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                    break;
                }

                if (ret < 0){
                    cout << "avcodec_receive_packet failed, ret:" << ret << endl;
                    break;
                }

                h264Packet.stream_index = outVideoindex;
                if ((ret = av_interleaved_write_frame(outFormatCtx, &h264Packet)) < 0) {
                    cout << "av_interleaved_write_frame failed, ret:" << ret << endl;
                }
            }

            av_frame_unref(pFrame);
            av_frame_unref(h264Frame);
            encodeFrameNum++;
        }

        av_packet_unref(&packet);
        av_packet_unref(&h264Packet);
        decodeFrameNum++;
        cout << "time: " << getFormatTime() << " encodeFrameNum: " << encodeFrameNum << " decodeFrameNum: " << decodeFrameNum << endl;
    }

    //Write file trailer
    av_write_trailer(outFormatCtx);
    return Constat::ok;
}

//void Decoder::saveImage(std::string filename) {
//    cv::Mat mat;
//    mat.create(cv::Size(imgWidth, imgHeight), CV_8UC3);
//    mat.data = buffer;
//    if (!cv::imwrite(filename, mat)) {
//        cout<< "saveImage failed, filename: " << filename << ", pts: " << pts << endl;
//    } else {
//        cout<< "saveImage success, filename: " << filename << ", pts: " << pts << endl;
//    }
//}
//
//std::string Decoder::getImageName() {
//    time_t ts = time(NULL);
//    std::stringstream ss;
//    ss << "/tmp/";
//    ss << ts;
//    ss << "_";
//    ss << decodeFrameNum;
//    ss << ".";
//    ss << "jpg";
//    return ss.str();
//}

void Decoder::closeDecode() {

    avfilter_graph_free(&filterGraph);

    av_frame_free(&h264Frame);
    av_frame_free(&pFrame);

    avcodec_close(pCodecCtx);
    avcodec_free_context(&pCodecCtx);

    avcodec_close(pCodecCtxOrig);
    //avcodec_free_context(&pCodecCtxOrig); //Mustn't free

    //avfilter_free(filterCtx);

    avformat_close_input(&pFormatCtx);
    av_dict_free(&pAvDict);
}