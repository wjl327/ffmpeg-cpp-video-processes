#include <iostream>
#include "decoder.h"

using namespace std;

/**
 * ./video_avstream http://.... rtsp://....
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char** argv) {

    if (argc <= 2) {
        cout << "please input valid arguments." << endl;
        return 0;
    }

    Decoder d;
    d.decode(argv[1], argv[2]);
    return 0;
}
