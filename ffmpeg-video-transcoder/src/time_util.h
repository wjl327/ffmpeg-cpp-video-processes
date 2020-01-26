#ifndef FFMPEG_VIDEO_AVFILTER_TIME_UTIL_H
#define FFMPEG_VIDEO_AVFILTER_TIME_UTIL_H

#include <ctime>
#include <string>
using namespace std;

string getFormatTime()
{
    time_t now_time;
    char buf[64] = {0};
    now_time=time(NULL);
    strftime(buf, 128,"%Y-%m-%d %H:%M:%S", localtime(&now_time));
    return buf;
}

#endif