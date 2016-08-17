#include <iostream>
#include <unistd.h>
#include "netstation.h"

using namespace std;
using namespace NetStation;

int main() {
    class EGIConnection egitest;
    egitest.connect("10.10.10.42", 55513);
    egitest.sendBeginSession(kLittleEndian);
    egitest.sendBeginRecording();

    char mark = 'A';
    int i = 0;
    for(i = 0;i < 26;i++){
        mark = 'A' + i;
        egitest.sendAttention();
        egitest.sendSynch(i);
        egitest.sendTrigger(&mark, i, 50);
        sleep(5);
    }

    egitest.sendEndRecording();
    egitest.sendEndSession();
    egitest.disconnect();
    return 0;
}