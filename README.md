# ffmpeg-cpp-video-processes

ffmpeg-video-decoder  该模块是将视频解码成jpg图片，主要依赖ffmpeg、opencv这两个第三方库。如果视频资源地址是https，则还需要ssl库。

ffmpeg-video-avfilter 该模块是将视频解码成jpg图片，加上avfilter实现的动态文字水印能力。

ffmpeg-video-overlay  该模块是将视频解码成jpg图片，同时加上文字水印和图片水印的能力。

ffmpeg-video-transcoder 该模块是将拉到的视频流直接转码推流到rtsp服务。

ffmpeg-video-avstream 该模块是将视频流解码、avfilter、编码、推流到rtsp服务。

ffmpeg-video-image-push 该模块与其他不同是，解码的是图片集合，而不是视频流，将图片解码后，编码成h264后再推流。


参考资料

雷神github：https://github.com/leixiaohua1020

ffmpeg官方例子：https://ffmpeg.org/doxygen/3.4/examples.html