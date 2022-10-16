// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_


#include "link_layer.h"

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename);

int getControlPacket(char* filename, int start, unsigned char* packet, int *sizeOfPacket);

int getDataPacket(unsigned char* bytes, unsigned char* packet, int nSequence, int nBytes);


#endif // _APPLICATION_LAYER_H_
