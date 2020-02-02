#include <iostream>
#include "decoder.h"

using namespace std;

/**
 * ./image_push rtsp://....
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char** argv) {

    if (argc <= 1) {
        cout << "please input valid arguments." << endl;
        return 0;
    }

    Decoder e;
    e.decode(argv[1]);
    return 0;
}
