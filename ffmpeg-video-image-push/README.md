该模块的能力是将视频流解码成图片。

运行时，输入解码数量和视频资源地址两个参数即可。对了，想要测试直播流地址的话。可以找下热门的直播网站，在直播间通过浏览器F12开发者工具抓取直播视频流地址即可，快捷方便。

编译和运行方式，依赖ffmpeg、opencv两个第三方库，需要自行编译和安装到third目录。

    cd ffmpeg-video-image-push
    mkdir build
    mkdir third (depend on ffmpeg & opencv)
    cd build 
    cmake ../
    make
    ./image_push rtsp://....
    
  