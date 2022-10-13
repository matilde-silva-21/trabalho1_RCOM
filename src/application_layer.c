// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayerRole lr;

    if(strcmp (role, "tx") == 0){
        lr = 1;
    }
    else {lr = 0;}

    LinkLayer ll;
    strcpy(ll.serialPort, serialPort);
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;
    ll.role = lr;

    if(llopen(ll)==-1) {
        return -1;
    }
    
}
