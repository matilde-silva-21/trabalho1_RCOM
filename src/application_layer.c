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
        return;
    }

    if(lr == 1){

        unsigned char packet[1200], bytes[200], fileNotOver = 1;
        int sizePacket = 0;

        FILE *fileptr;

        int nBytes = 200, curByte=0, index=0, nSequence = 0;

        fileptr = fopen(filename, "rb");        // Open the file in binary mode
        if(fileptr == NULL){
            printf("Couldn't find a file with that name, sorry :(\n");
            return;
        }

        if(!getControlPacket(filename,1,packet,sizePacket)){
            return;
        }

        if(llwrite(packet, sizePacket) == -1){
            return;
        }


        while(fileNotOver){
            
            if(!fread(&curByte, (size_t)1, (size_t) 1, fileptr)){
                fileNotOver = 0;
                sizePacket = getDataPacket(bytes, packet, nSequence++, index);
                if(llwrite(packet, sizePacket) == -1){
                    return;
                }
            }

            else if(nBytes == (index-1)) {
                getDataPacket(bytes, packet, nSequence++, index);
                if(llwrite(packet, sizePacket) == -1){
                    return;
                }
                memset(bytes,0,sizeof(bytes));
                memset(packet,0,sizeof(packet));
                index = 0;
            }

            bytes[index++] = curByte;
        }

        fclose(fileptr);

        if(!getControlPacket(filename,0,packet,sizePacket)){
            return;
        }

        if(llwrite(packet, sizePacket) == -1){
            return;
        }

    }

    else{
        // 1º chamar llread
        // 2º ler o packet do llread, se for um control packet START, criar um ficheiro novo, quando receber o close fecho o ficheiro que estou a escrever e paro de chamar llread, se for 0, prox iteraçao chamr llread de novo
        // 3º escrever os dataPacket no ficheiro que criei
        FILE *fileptr;

        while(1){
            unsigned char packet[1200] = {0};
            int sizeOfPacket = 0;
            llread(packet, sizeOfPacket);
            if(packet[4] == 0x03){
                fclose(fileptr);
                break;
            }
            else if(packet[4]==0x02){
                fileptr = fopen("penguin-copy.gif", "wb");   
            }
            else{
                fwrite(packet, sizeOfPacket, 1, fileptr);
            }
        }
    }

    return;

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

	sizeOfPacket = index;
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






