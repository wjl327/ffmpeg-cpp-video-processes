#include <iostream>
#include <sstream>
#include "decoder.h"
#include "time.h"
#include <unistd.h>

using namespace std;

void Decoder::decode(std::string desc) {

    int ret;
    if ((ret = initDecode()) < 0) {
        cout << "init decode fail :" << ret << endl;
        return;
    }

    if ((ret = initEncode()) < 0) {
        cout << "init encode fail :" << ret << endl;
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

int Decoder::initDecode() {

    av_register_all();

    int ret;
    if ((ret = avformat_network_init()) != 0) {
        cout << "avformat_network_init failed, ret: " << ret << endl;
        return ret;
    }

    const AVCodec *pCodec;
    pCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    parserJpg = av_parser_init(pCodec->id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        cout << "decode avcodec_open2 failed." << endl;
        return Constat::system_error;
    }

    //解码的包
    pPacket = av_packet_alloc();

    //解码的帧
    pFrame = av_frame_alloc();

    cout << "init decode succ !!!" << endl;
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
    h264CodecCtx->width  = 1920;      //分辨率宽
    h264CodecCtx->height = 1080;     //分辨率高
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
        cout << "encode avcodec_open2 failed!" << endl;
        return Constat::system_error;
    }

    h264Packet = av_packet_alloc();

    cout << "init encode succ !!!" << endl;
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
    av_dict_set(&h264Dict, "stimeout", "6000000", 0);
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

int Decoder::decoding() {

    //start_time=av_gettime();
    for (int i = 1; i <= 10000; i++) {

        uint64_t startTime = getCurTimestamp();
        std::string imageData;
        string imageName = getImageName(i);

        if (!isExistFile(imageName)) {
            cout << "==========finish push===========" << endl;
            return 0;
        }

        long filesize = getImageSize(imageName);
        if (filesize > imageData.size()) {
            imageData.resize(filesize);
        }
        FILE* fileImage = fopen(imageName.c_str(), "rb");
        fread(&(imageData[0]), 1, filesize, fileImage);
        fclose(fileImage);

        //读取的jpeg数据。属于没有封装格式的裸流，区别就是不包含PTS、DTS这些参数的。
        uint8_t *in_data = (uint8_t *)(imageData.data());
        size_t in_len = filesize;
        while (in_len > 0) {

            //通过av_parser_parse2拿到AVPaket数据
            int len = av_parser_parse2(parserJpg, pCodecCtx, &pPacket->data, &pPacket->size, in_data, in_len, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (len < 0) {
                cout << "av_parser_parse2 failed!" << endl;
                return Constat::system_error;
            }
            in_data += len;
            in_len -= len;

            if (pPacket->size) {
                cout << "parserJpg file: " << imageName << " i: " << i << " len: " << pPacket->size << endl;
                decodeJpg(i);
            }
        }

        //Important:Delay
        //av_usleep(pts_time - now_time);

        //TODO 推流其实需要pts、duration；也需要用av_usleep进行暂停
        uint64_t costTime = getCurTimestamp() - startTime;
        if ( frame_step > costTime ) {
            sleep_ms(frame_step - costTime);
        }

    }

    return Constat::ok;
}

int Decoder::decodeJpg(int64_t pts) {

    int ret = avcodec_send_packet(pCodecCtx, pPacket); //将原始数据包传给ffmpeg解码器
    if (ret < 0) {
        cout << "decodeJpg avcodec_send_packet failed, ret:" << ret << endl;
        return Constat::system_error;
    }

    while (ret >= 0) {

        ret = avcodec_receive_frame(pCodecCtx, pFrame); //从解码队列中取出1个frame
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }

        if (ret < 0) {
            cout << "decodeJpg avcodec_receive_frame failed, ret:" << ret << endl;
            return Constat::system_error;
        }

        cout << "avcodec_receive_frame frame " << pCodecCtx->frame_number << endl;

        encodeYuvToH264(pts);
    }

}

int Decoder::encodeYuvToH264(int64_t pts) {

    pFrame->pts = pts;
    int ret = avcodec_send_frame(h264CodecCtx, pFrame); //将帧数据包传给ffmpeg编码器
    if (ret < 0){
        cout << "encodeYuvToH264 avcodec_send_frame failed, ret:" << ret << endl;
        return Constat::system_error;
    }

    while (ret >= 0) {

        ret = avcodec_receive_packet(h264CodecCtx, h264Packet); //从编码队列中取出1个packagt
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            break;
        }

        if (ret < 0){
            cout << "encodeYuvToH264 avcodec_receive_packet failed, ret:" << ret << endl;
            break;
        }

        //h264Packet->stream_index = outVideoindex;
        if ((ret = av_interleaved_write_frame(outFormatCtx, h264Packet)) < 0) {
            cout << "encodeYuvToH264 av_interleaved_write_frame failed, ret:" << ret << endl;
        }
    }

    av_frame_unref(pFrame);
}


std::string Decoder::getImageName(int num) {
    std::stringstream ss;
    ss << "/root/ffmpeg-cpp-video-processes/image/";
    ss << num;
    ss << ".jpg";
    return ss.str();
}

long Decoder::getImageSize(string filename) {
    FILE* f = fopen(filename.c_str(), "rb");
    fseek(f,0,SEEK_END);
    long s = ftell(f);
    fclose(f);
    return s;
}

bool Decoder::isExistFile(string filename) {
    return access(filename.c_str(), 0) == 0;
}

uint64_t Decoder::getCurTimestamp(){
    struct timeval cur;
    gettimeofday(&cur, NULL);
    return cur.tv_sec * 1000 + cur.tv_usec / 1000;
}


void Decoder::closeDecode() {
    avcodec_free_context(&h264CodecCtx);
    av_frame_free(&pFrame);
    av_packet_free(&h264Packet);
    av_packet_free(&pPacket);
    av_parser_close(parserJpg);
    avcodec_free_context(&pCodecCtx);
}