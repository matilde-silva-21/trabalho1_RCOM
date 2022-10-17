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
    volatile int STOP = FALSE;

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);

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

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

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

    unsigned char BCC = 0x00, infoFrame[6+bufSize*2], parcels[5] = {0};
    int index = 4, STOP = 0, control = (receiverNumber << 7) | 0x05;


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

        if(result != -1 && parcels != 0 && parcels[0]==0x7E && parcels[4]==0x7E){
            if(parcels[2] != (control) || (parcels[3] != (parcels[1]^parcels[2]))){
                    printf("\nRR not correct: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    alarmEnabled = FALSE;
                    continue;
            }
            else{
                printf("\nRR correctly received\n");
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
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
   {
    signal(SIGALRM,alarmHandler);

    if(connection.role==LlTx) {   //Transmitter

        int receivedDISC_tx=0;
     
        tries=0;
        
        while(tries<connection.nRetransmissions && !receivedDISC_tx){
            alarm(connection.timeout);
            alarmEnabled=1;
            
            if(tries>0)
                printf("Break.\n");
            
            int size = buildFrame(buf,NULL,0,ADR_TX,CTRL_DISC,0);
            printf("llclose: DISC send.\n");
            write(fd,buf,size);
            
            while(alarmEnabled && !receivedDISC_tx){
                int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
                
                if(bytes_read<0)
                    return -1;
                
                for(int i=0; i<bytes_read && !receivedDISC_tx; ++i){
                    state_machine(buf[i],&state);
                    
                    if(state.state==SMEND && state.adr==ADR_TX && state.ctrl == CTRL_DISC)
                        receivedDISC_tx=1;
                }
            }
        }
        
        if(receivedDISC_tx) 
            printf("llclose: DISC received.\n");
        
        int frame_size=buildFrame(buf,0,0,ADR_TX,CTRL_UA,0);
        
        write(fd,buf,frame_size); //sends reply to UA.
        
        printf("llclose: UA sended.\n");

    } else { //Receiver
//..................... missing
        
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 1;
}
