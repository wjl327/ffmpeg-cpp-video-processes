#ifndef FFMPEG_VIDEO_OVERLAY_TRANS_UTIL_H
#define FFMPEG_VIDEO_OVERLAY_TRANS_UTIL_H

void createImage() {

    cv::Mat mat(720, 1280, CV_8UC3, CV_RGB(255, 255, 255));
    IplImage *image = new IplImage(mat);

    int arr[1]= {5};
    CvPoint ** pt = new CvPoint*[1];
    pt[0] = new CvPoint[4];
    pt[0][0] = cvPoint(100, 200);
    pt[0][1] = cvPoint(400, 80);
    pt[0][2] = cvPoint(800, 250);
    pt[0][3] = cvPoint(900, 600);
    pt[0][4] = cvPoint(200, 600);
    cvPolyLine(image, pt, arr, 1, 1, CV_RGB(255, 0, 0), 2);

    IplImage *dstImage = 0;
    CvSize dst_cvsize;
    dst_cvsize.width = image->width;
    dst_cvsize.height = image->height;
    dstImage = cvCreateImage( dst_cvsize, image->depth, 4);

    int x;
    int y;
    uchar r, g, b;
    for (y = 0; y < image->height; y++) {
        uchar *ptrSrc = (uchar*)(image->imageData + y * image->widthStep);
        uchar *ptrDst = (uchar*)(dstImage->imageData + y * dstImage->widthStep);
        for (x = 0; x < image->width; x++) {
            r = ptrSrc[3 * x];
            g = ptrSrc[3 * x + 1];
            b = ptrSrc[3 * x + 2];
            ptrDst[4 * x] = r;
            ptrDst[4 * x + 1] = g;
            ptrDst[4 * x + 2] = b;
            if (255 == r && 255 == g && 255 == b) {
                ptrDst[4 * x + 3] = 0;
            }
            else {
                ptrDst[4 * x + 3] = 255;
            }
        }
    }
    cvSaveImage("test.png", dstImage);
}

#endif //FFMPEG_VIDEO_OVERLAY_TRANS_UTIL_H
