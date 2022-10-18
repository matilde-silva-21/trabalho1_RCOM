// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

extern int alarmEnabled, alarmCount;
int senderNumber = 0, receiverNumber = 1;
int nTries, timeout, fd;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);

    alarmCount = 0;

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    

    printf("\n---------------LLOPEN---------------\n\n");

    volatile int STOP = FALSE;

    nTries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;


    if(connectionParameters.role == LlRx){

        unsigned char buf[5] = {0}, parcels[5] = {0};

        buf[0] = 0x7E;
        buf[1] = 0x03;
        buf[2] = 0x03;
        buf[3] = 0x00;
        buf[4] = 0x7E;


        while(alarmCount <= nTries){

            if(!alarmEnabled){
                int bytes = write(fd, buf, sizeof(buf));
                printf("\nSET message sent, %d bytes written\n", bytes);
                startAlarm(timeout);
            }
            
            int result = read(fd, parcels, 5);
            if(result != -1 && parcels != 0 && parcels[0]==0x7E){
                //se o UA estiver errado 
                if(parcels[2] != 0x07 || (parcels[3] != (parcels[1]^parcels[2]))){
                    printf("\nUA not correct: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    alarmEnabled = FALSE;
                    continue;
                }
                
                else{   
                    printf("\nUA correctly received: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    break;
                }
            }

        }

        if(alarmCount > nTries){
            printf("\nAlarm limit reached, SET message not sent\n");
            return -1;
        }
    }

    else
    {
        unsigned char buf[1] = {0}, parcels[5] = {0}; // +1: Save space for the final '\0' char

        STATE st = STATE0;
        unsigned char readByte = TRUE;
        
        // Loop for input
        while (STOP == FALSE)
        { 
            if(readByte){
                int bytes = read(fd, buf, 1); //ler byte a byte
                if(bytes==0) continue;
            }
        
            
            switch (st)
            {
            case STATE0:
                if(buf[0] == 0x7E){
                    st = STATE1;
                    parcels[0] = buf[0];
                }
                break;

            case STATE1:
                if(buf[0] != 0x7E){
                    st = STATE2;
                    parcels[1] = buf[0];
                }
                else {
                    st = STATE0;
                    memset(parcels, 0, 5);
                }
                break;

            case STATE2:
                if(buf[0] != 0x7E){
                    st = STATE3;
                    parcels[2] = buf[0];
                }
                else {
                    st = STATE0;
                    memset(parcels, 0, 5);
                }
                break;

            case STATE3:
                if(buf[0] != 0x7E){
                    parcels[3] = buf[0];
                    st = STATE4;
                }
                else {
                    st = STATE0;
                    memset(parcels, 0, 5);
                }
                break;

            case STATE4:
                if(buf[0] == 0x7E){
                    parcels[4] = buf[0];
                    st = STATE5;
                    readByte = FALSE;
                }

                else {
                    st = STATE0;
                    memset(parcels, 0, 5);
                }
                break;
            case STATE5:
                if(((parcels[1])^(parcels[2]))==(parcels[3])){
                    printf("\nGreat success! SET message received without errors\n\n");
                    STOP = TRUE;
                }
                else {
                    st = STATE0;
                    memset(parcels, 0, 5);
                    readByte = TRUE;
                }
                break;
            default:
                break;
            }
        }

        parcels[2] = 0x07;
        parcels[3] = parcels[1]^parcels[2];

        //preciso de estar dentro da state machine ate receber um sinal a dizer que o UA foi corretamente recebido

        int bytes = write(fd, parcels, sizeof(parcels));
        printf("UA message sent, %d bytes written\n", bytes);
    }

    alarmEnabled = FALSE;

    /*if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }*/

    //close(fd);

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{

    //1º criar o BCC para o dataPacket
    //2º fazer byte stuffing
    //3º criar a nova infoFrame com o dataPacket (ja stuffed) la dentro
    //4º enviar a infoFrame e contar o alarme
    //5º factCheck a frame recebida do llread (ver se tem erros ou assim)
    //6º llwrite so termina quando recebe mensagem de sucesso ou quando o limite de tentativas é excedido

    printf("\n---------------LLWRITE---------------\n\n");

    alarmCount = 0;

    unsigned char BCC = 0x00, infoFrame[6+bufSize*2], parcels[5] = {0};
    int index = 4, STOP = 0, controlReceiver = (receiverNumber << 7) | 0x05;


    for(int i=0; i<bufSize; i++){
        BCC = (BCC ^ buf[i]);
    }

    infoFrame[0]=0x7E; //Flag
    infoFrame[1]=0x03; //Address
    infoFrame[2]=(senderNumber << 6); //Control
    infoFrame[3]=infoFrame[1]^infoFrame[2];


    for(int i=0; i<bufSize; i++){
        if(buf[i]==0x7E){
            infoFrame[index++]=0x7D;
            infoFrame[index++]=0x5e;
            continue;
        }
        infoFrame[index++]=buf[i];
    }

    infoFrame[index++]=BCC;
    infoFrame[index++]=0x7E;

    while(!STOP){
        if(!alarmEnabled){
            write(fd, infoFrame, index);
            printf("\nInfoFrame sent NS=%d\n", senderNumber);
            startAlarm(timeout);
        }
        
        int result = read(fd, parcels, 5);

        if(result != -1 && parcels != 0 /*&& parcels[0]==0x7E && parcels[4]==0x7E*/){
            if(parcels[2] != (controlReceiver) || (parcels[3] != (parcels[1]^parcels[2]))){
                    printf("\nRR not correct: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    alarmEnabled = FALSE;
                    continue;
            }
            
            else{
                printf("\nRR correctly received\n");
                alarmEnabled = FALSE;
                STOP = 1;
            }
        }

        else if(alarmCount > nTries){
            printf("\nllwrite error: Exceeded number of tries when sending frame\n");
            STOP = 1;
            return -1;
        }

    }


    if(senderNumber){
        senderNumber = 0;
    }
    else {senderNumber = 1;}

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet, int sizeOfPacket)
{   

    unsigned char infoFrame[1200]={0}, buffer[1000]={0}, supFrame[5]={0}, BCC2=0x00;

    int result = read(fd, infoFrame, 1200), index = 0;

    if(result == -1){
        return -1;
    }


    printf("\n---------------LLREAD---------------\n\n");
    //1º ler o pipe
    //2º fazer de-stuff aos bytes lidos
    //3º verificar que os BCCs estao certos
    //4º enviar a mensagem de confirmacao de receçao, positiva se correu tudo bem, negativa se BCC ou assim esta mal
    
    //fazer as contas para confirmar o valor max do buffer
    
    supFrame[0] = 0x7E;
    supFrame[1] = 0x03;
    supFrame[4] = 0x7E;

    if(infoFrame[0]!=0x7E || (infoFrame[1]^infoFrame[2]) != infoFrame[3]){
        printf("\nInfoFrame not received correctly. Sending REJ.\n");
        supFrame[2] = (receiverNumber << 7) | 0x01;
        supFrame[3] = supFrame[1] ^ supFrame[2];
        write(fd, supFrame, 5);
        return -1;
    }




    for(int i=4; i<1200-1; i++){
        if(infoFrame[i] == 0x7D && infoFrame[i+1]==0x5e){
            buffer[index++] = 0x7E;
            i++;
        }
        else {buffer[index++] = infoFrame[i];}
    }




    int size = 0; //tamanho da secçao de dados

    if(buffer[4]==0x01){
        size = 256*buffer[6]+buffer[7]+4; //+4 para contar com os bytes de controlo, numero de seq e tamanho
        for(int i=4; i<size; i++){
            BCC2 = BCC2 ^ buffer[i];
        }
    }
    
    else{
        size += buffer[6]+5; //+5 para contar com os bytes de controlo, parametro e tamanho
        size += buffer[6+size+2];

        for(int i=4; i<size; i++){
            BCC2 = BCC2 ^ buffer[i];
        }
    }

    

    if(buffer[size+4] == BCC2){
        printf("\nInfoFrame received correctly. Sending RR.\n");
        supFrame[2] = (receiverNumber << 7) | 0x05;
        supFrame[3] = supFrame[1] ^ supFrame[2];
        write(fd, supFrame, 5);
    }

    sizeOfPacket = size+6;

    for(int i=0; i<sizeOfPacket; i++){
        packet[i] = buffer[i];
    }


    if(receiverNumber){
        receiverNumber = 0;
    }
    else {receiverNumber = 1;}


    return 1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}

