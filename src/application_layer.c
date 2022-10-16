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

    unsigned char packet[1200], bytes[124], fileNotOver = 1;
	int sizeDataPacket = 0;

    FILE *fileptr;

    int nBytes = 124, curByte=0, index=0, nSequence = 0;

    fileptr = fopen(filename, "rb");        // Open the file in binary mode
    if(fileptr == NULL){
        printf("Couldn't find a file with that name, sorry :(\n");
        return -1;
    }

    while(fileNotOver){

		if(!fread(&curByte, (size_t)1, (size_t) 1, fileptr)){
			fileNotOver = 0;
			sizeDataPacket = getDataPacket(bytes, packet, nSequence++, index);
            //llwrite(packet, sizeDataPacket);
		}

        else if(nBytes == (index-1)) {
            getDataPacket(bytes, packet, nSequence++, index);
            //llwrite(packet, sizeDataPacket);
            memset(bytes,0,sizeof(bytes));
            memset(packet,0,sizeof(packet));
            index = 0;
        }

        bytes[index++] = curByte;
    }

}

int getControlPacket(char* filename, int start, unsigned char* packet, int *sizeOfPacket){

	if(strlen(filename) > 255){
        printf("size of filename couldn't fit in one byte: %d\n",2);
        return -1;
    }

	char hex_string[20];

    struct stat file;
    stat(filename, &file);
    sprintf(hex_string, "%02lX", file.st_size);
	
	int index = 3, fileSizeBytes = strlen(hex_string);

    if(fileSizeBytes > 256){
        printf("size of file couldn't fit in one byte\n");
        return -1;
    }
    
    if(start){
		packet[0] = 0x02; 
    }
    else{
        packet[0] = 0x03;
    }

    packet[1] = 0x00; // 0 = tamanho do ficheiro 
    packet[2] = fileSizeBytes;


	for(int i=0; i<strlen(hex_string); i++){
		packet[index++] = hex_string[i];
	}
    

    packet[index++] = 0x01;
    packet[index++] = strlen(filename);

	for(int i=0; i<strlen(filename); i++){
		packet[index++] = filename[i];
	}

	*sizeOfPacket = index;
    return 1;

}



int getDataPacket(unsigned char* bytes, unsigned char* packet, int nSequence, int nBytes){

	int l2 = div(nBytes, 256).quot , l1 = div(nBytes, 256).rem;

    packet[0] = 0x01;
	packet[1] = div(nSequence, 255).rem;
    packet[2] = l2;
    packet[3] = l1;

    for(int i=0; i<nBytes; i++){
        packet[i+4] = bytes[i];
    }

	return nBytes+4; //tamanho do data packet
}






