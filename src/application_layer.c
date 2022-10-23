// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayerRole tr;
    
    int resTX = strcmp (role, "tx");
    int resRX = strcmp (role, "rx");

    int statistics = 1;

    if(resTX==0){
        tr = LlTx;
    }
    else if(resRX==0){tr = LlRx;}
    else {printf("\nERROR! Invalid role.\n"); return;}

    LinkLayer ll;
    strcpy(ll.serialPort, serialPort);
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;
    ll.role = tr;

    if(llopen(ll)==-1) {return;}
    
    clock_t start, end;
    
    start = clock();
    //segmento testado e funcional (a excecao de llwrite)
    if(tr == LlTx){
        unsigned char packet[300], bytes[200], fileNotOver = 1;
        int sizePacket = 0;
       

        FILE *fileptr;
        

        int nBytes = 200, curByte=0, index=0, nSequence = 0;
        
        
        fileptr = fopen(filename, "rb");        // Open the file in binary mode
        if(fileptr == NULL){
            printf("Couldn't find a file with that name, sorry :(\n");
            return;
        }
        


        sizePacket = getControlPacket(filename,1,&packet);

        if(llwrite(packet, sizePacket) == -1){
            return;
        }


        while(fileNotOver){

            //comeco por ler a file stream
            //se deixar de haver coisas para ler corro este codigo
            if(!fread(&curByte, (size_t)1, (size_t) 1, fileptr)){
                fileNotOver = 0;
                sizePacket = getDataPacket(bytes, &packet, nSequence++, index);

                if(llwrite(packet, sizePacket) == -1){
                    return;
                }
            }

            //se o valor de index for igual a nBytes, significa que o ja passamos por nByte elementos
            else if(nBytes == index) {
                sizePacket = getDataPacket(bytes, &packet, nSequence++, index);

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

        sizePacket = getControlPacket(filename,0,&packet);

        if(llwrite(packet, sizePacket) == -1){
            return;
        }

    }

    else{
        // 1º chamar llread
        // 2º ler o packet do llread, se for um control packet START, criar um ficheiro novo, quando receber o close fecho o ficheiro que estou a escrever e paro de chamar llread, se for 0, prox iteraçao chamr llread de novo
        // 3º escrever os dataPacket no ficheiro que criei
        FILE *fileptr;
        char readBytes = 1;
        
        

        while(readBytes){
        
            unsigned char packet[600] = {0};
            int sizeOfPacket = 0, index = 0;
            
            if(llread(&packet, &sizeOfPacket)==-1){
                continue;
            }
           
            
            if(packet[0] == 0x03){
                printf("\nClosed penguin\n");
                fclose(fileptr);
                readBytes = 0;
            }
            else if(packet[0]==0x02){
                printf("\nOpened penguin\n");
                fileptr = fopen(filename, "wb");   
            }
            else{
                for(int i=4; i<sizeOfPacket; i++){
                    fputc(packet[i], fileptr);
                }
            }
        }
    }
    end = clock();
    float duration = ((float)end - start)/CLOCKS_PER_SEC;

    llclose(&statistics, ll, duration);
    
    
    return;

}

/*Funcao testada e funcional*/
int getControlPacket(char* filename, int start, unsigned char* packet){

    int sizeOfPacket = 0;

	if(strlen(filename) > 255){
        printf("size of filename couldn't fit in one byte: %d\n",2);
        return -1;
    }

	unsigned char hex_string[20];

    struct stat file;
    stat(filename, &file);
    sprintf(hex_string, "%02lX", file.st_size);
	
	int index = 3, fileSizeBytes = strlen(hex_string) / 2, fileSize = file.st_size;

    printf("\nfilesize: %d\n hex_string: %s", file.st_size, hex_string);

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


	for(int i=(fileSizeBytes-1); i>-1; i--){
		packet[index++] = fileSize >> (8*i);
	}
    

    packet[index++] = 0x01;
    packet[index++] = strlen(filename);

	for(int i=0; i<strlen(filename); i++){
		packet[index++] = filename[i];
	}

	sizeOfPacket = index;
    
    return sizeOfPacket;

}


/*Funcao testada e funcional*/
int getDataPacket(unsigned char* bytes, unsigned char* packet, int nSequence, int nBytes){

	int l2 = div(nBytes, 256).quot , l1 = div(nBytes, 256).rem;

    packet[0] = 0x01;
	packet[1] = div(nSequence, 255).rem;
    packet[2] = l2;
    packet[3] = l1;

    for(int i=0; i<nBytes; i++){
        packet[i+4] = bytes[i];
    }

	return (nBytes+4); //tamanho do data packet

}



