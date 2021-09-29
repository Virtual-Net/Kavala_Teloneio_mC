/*******************************************************************************************************
PARKING TELONEIOU - KABALA - Termatiko Eisodoy
gmaragkakis
12/02/19
Code 4 OP7200

Printer Custom 150H            Serial C
RFID Reader Mulit ISO 1.2.5    Serial D

Eisodos  IP 192.168.1.36, mnm eisodoy peristasiakou: VIS!!-


V.2: allagi gia na min patietai to button mexri na vgei to eisitirio! (giati vgainan dipla, den arkouse o xronos epanaforas gia to button)
V.3: prostethike metavliti sti diadikasia twn ACK gia na katalavainei pote den pairnei apantisi kai na vgainei ektos sundesis
v.4: Upgrade TCP communication for using php server
********************************************************************************************************/

#define  STDIO_DEBUG_SERIAL   SADR
#define  STDIO_DEBUG_BAUD  57600
#define  STDIO_DEBUG_ADDCR

#class auto
#memmap xmem

//network configuration
#define TCPCONFIG          0
#define USE_ETHERNET       1
#define MAX_TCPBUF         100
#define MY_IP_ADDRESS      "192.168.1.36"
#define MY_NETMASK         "255.255.255.0"
#define MY_GATEWAY         "192.168.1.75"
#define MY_NAMESERVER      "192.168.1.75"
#define LOG_SERVER         "192.168.1.75"
#define SERVER_IP         	"192.168.1.75"
//#define SCREEN_SERVER      "192.168.1.15"
#define SERVERPORT         80
#define  TCP_TIMEOUT       150

#define KEEPALIVE_WAITTIME 75    // 75 second wait
#define KEEPALIVE_NUMRETRYS 3 // retry 8 times

#define     CINBUFSIZE     63
#define     COUTBUFSIZE    8191
#define     DINBUFSIZE     63
#define     DOUTBUFSIZE    63
#define     MAX_SENTENCE   50

//ucos configuration
#define  OS_MAX_EVENTS           1        // Maximum number of events (semaphores, queues, mailboxes)
#define  OS_MAX_TASKS            6        // Maximum number of tasks system can create (less stat and idle tasks)
#define  OS_MAX_QS            	0
#define  OS_Q_EN              	0
#define  OS_MBOX_EN              1        // Enable mailboxes
#define  OS_TICKS_PER_SEC     	128      //32
#define  STACK_CNT_512         	6
#define  STACK_CNT_1K            0        // number of 512 byte stacks (application tasks + stat task + prog stack)
#define  STACK_CNT_2K         	0
#define  STACK_CNT_4K         	1        // number of 2K stacks

#define  MSG_BUF_SIZE            256

#define     OS_TIME_DLY_HMSM_EN  1
#define	HTTP_HEADER1		   "GET /ThemeForParking_Rethymno_2/mytraffic.php?cmd="
#define	HTTP_HEADER2		   "&dev=TermIn&ip=192.168.1.36 HTTP/1.1\r\nHost:192.168.1.75:80\r\n\r\n"


// GENERAL
#define     LOOP_TIMEOUT   35     //20  //25   //30
#define     ACK_Timeout    60     //120    //80


#use "ucos2.lib"
#use "dcrtcp.lib"
#use        "kpcust16key.lib"
#use        "terminal9.lib"
#use        "Arial16.lib"

//#define 		VDly2(a)     		VDly(0,0,0,a);
#define     VDly(j)           OSTimeDly(j*OS_TICKS_PER_SEC/1000);

// FUNCTION PROTOTYPES
void Input_Task (void *data);
void TCP_Task(void *ptr);
void Nothing_Task (void *data);
void Display_Task (void *data);
void RFID_Task (void *pdata);
void Watchdog_Task(void *pdata);

// VARIABLES
unsigned int netstatus,openbar,light,a,b,c,d,tim,tim1,DisplayMSG,stat;
char send[150];
//char sendScreen[100];
char barcode_print[200];
char date[15];
char license[10];
char barcode[10];
char time[15];
//char msg_return[100];    //krata to mnm tou server gia eisodo oximatos, gia na to steilei pisw gia epibebaiwsi

//DISPLAY
fontInfo fontdesc;

void OpenBar(unsigned int i);
void Light(unsigned int i);
int reboot();
int printcard(char printstr[60]);
void msDelay(unsigned int delay);

//*******************************************************************************************************
void main(){

   char test,error;
   int i;

    msDelay(3000);
   //Initialize serial C for printer
   serCflowcontrolOff();
   serCparity(PARAM_NOPARITY);
   serCdatabits(PARAM_8BIT);
   if(serCopen(115200)){
      printf("Serial C Printer OK\n");
   }else{
      printf("Serial C Printer ERROR\n");
   }

   brdInit();
   digOutConfig(0x00); //7
   digOut(0,0);digOut(0,1);//BARA   1->0
   digOut(1,0);digOut(1,1);//LIGHT  1->0
   //digOut(2,0);digOut(2,1);//Message  1->0

   openbar = 0;
   Light(0); // Na Xekinaei me prasino fanari

   glInit();
   glSetContrast(100);   //14
   keyInit();
   keypadDef();
   glkeypadInit(1);

   glXFontInit ( &fontdesc,22,26,0x20,0xFF,Arial16 );
   buzzer(1);msDelay(50);buzzer(0);msDelay(50);buzzer(1);msDelay(50);buzzer(0);

   OSInit();
   //glkClearRegion(0,0,320,240);

   sprintf(barcode_print,"00-00-0000|00:00:00|Test___|0000000$");printcard(barcode_print);msDelay(2000);
   //sprintf(barcode_print,"00-00-0000|00:00:00|Test___|0000000$");printcard(barcode_print);msDelay(2000);

   error = OSTaskCreate(Watchdog_Task,  NULL, 512,  0);
   error = OSTaskCreate(TCP_Task ,     NULL, 4096, 1);
   error = OSTaskCreate(Input_Task, NULL, 512,  2);
   //error = OSTaskCreate(Nothing_Task,     NULL, 512,  3);
   error = OSTaskCreate(RFID_Task,   NULL, 512 , 4);
   error = OSTaskCreate(Display_Task,  NULL, 512 , 5);

   OSStart();
}
//*****************************************************************************************************
void Watchdog_Task(void *pdata){
int wd;
wd = VdGetFreeWd(33);
  while(1){
    VdHitWd(wd);
    //printf("wd reset!\n");
    VDly(500);
 }
}
//*****************************************************************************************************
void TCP_Task(void *ptr){

//#memmap xmem
static tcp_Socket socket, socket1;
longword host,host1,ping_who;
char  buffer[100], ip[128];
unsigned int temp;
char *token;
char *err,*err2;
char *ack,*vis,*mag,*mge;
unsigned int i,bytes_read,x,x1,sock_test,stat,tmp,conn_counter;
x=0;
tim =0;
netstatus=0; // Connected or NOT
tmp=0;
light = 0;
Light(0);
conn_counter =0;

sock_init();
memset(buffer,0x00,sizeof(buffer));
memset(send,0x00,sizeof(send));
memset(ip,0x00,sizeof(ip));
//memset(msg_return,0x00,sizeof(msg_return));

//VDly(0,0,3,0);

while (ifpending(IF_DEFAULT) == IF_COMING_UP) {
   tcp_tick(NULL);
   VDly(200);
}
ping_who = resolve(LOG_SERVER);

//VDly(1000);
// Print who we are...

//glBackLight(1);
sprintf(ip, "IP address is:%s", inet_ntoa(buffer, gethostid()) );
glkMsgBox(0,0,320,120,&fontdesc,ip,0,0); VDly(100);
glkMsgBox(0,120,318,120,&fontdesc,"Entry 1 \n Code: v.3",0,0); VDly(100);
VDly(2000); glkClearRegion(0,0,320,240);
//glBackLight(0);
printf("%s\n, Code v.3\n",ip);


while(1){
host = resolve(LOG_SERVER);
VDly(200);//Dinw delay 200 giati mplokarei
//host1 = resolve(SCREEN_SERVER);
VDly(200);//Dinw delay 200 giati mplokarei
tcp_tick(&socket);
//tcp_tick(&socket1);
    if (tcp_open(&socket ,0,host,SERVERPORT, NULL)){
         while (!sock_established(&socket)){
            tcp_tick(&socket); err=sockerr(&socket); printf("sockerr: %s\n", err);
            VDly(100); x++;
            if(x>18){
               x=0; netstatus=0;
               // TODO Display message on Screen
               DisplayMSG = 0;
               VDly(10);
               break;
            }
         }
         if (sock_established(&socket)){
            printf("\nsock established\n");
            tcp_keepalive(&socket,4);
            netstatus = 1;
            buzzer(1);VDly(250);buzzer(0);VDly(150);buzzer(1);VDly(250);buzzer(0);
            DisplayMSG = 1;
            while(tcp_tick(&socket)&& pd_havelink(IF_DEFAULT)==1){

            if(strlen(send)){
               sock_puts(&socket, send);
               printf("TCP_Send:%s\n",send);
               send[0]=0x0;
            }
            if(tim>ACK_Timeout){
               sprintf(send,"%s%s%s",HTTP_HEADER1,"ACK",HTTP_HEADER2);
			   //sprintf(send, "ACK@");
               tim =0;
               conn_counter++;
               printf("\nconn_counter:%d\n",conn_counter);
               if (DisplayMSG != 4){
               	DisplayMSG = 1;
               }
            }
            if(conn_counter>1){//Lost connection
					printf("\nLOST CONNECTION\n");
					conn_counter=0;
				break;}

            tim++;

          //Check if there's something to receive.
             if(sock_bytesready(&socket)>1){
            bytes_read=sock_fastread(&socket,buffer,sizeof(buffer)-1);
            if(bytes_read>0) {
                  buffer[bytes_read]=0;
                  printf("\nWe got TCP: %s\n",buffer);
				  		ack = strstr(buffer,"ACK@");
                  vis = strstr(buffer,"VIS@");
                  mag = strstr(buffer,"MAG@");
                  mge = strstr(buffer,"MGE@");
            if(ack){
				  //if(strncmp(buffer,"ACK@",4)==0){
                     printf("Ack packet received\n");
					if(strncmp(ack,"ACK@",4)==0){
                     token=strtok(ack,"@");
                     token=strtok(NULL,"@");
                     temp = atoi(token);

                     switch(temp){
                        case 0:
                           light = 0; Light(0); tim = 0;
                           DisplayMSG = 1;
                           token=strtok(NULL,"$");
                           if (token !=NULL){
                              temp = strlen(token);
                              token[temp]=0x00;
                           }
                           break;
                        case 1:
                           printf("\nParking FULL");
                           DisplayMSG = 4;light = 1;Light(1);tim = 0;
                           break;
                        /*case 20:
                           light = 0; Light(0);tim = 0; //DisplayMSG = 1;
                           break;
                        case 21:
                           printf("\nParking FULL");
                           DisplayMSG = 4;light = 1;Light(1);tim = 0;
                           break;*/
                        case 2:
                           printf("\nOpen Barrier SERVER");
                           DisplayMSG = 3;openbar = 1; tim = 0; OpenBar(1250);
                           //stat=0;
                           break;
                        /*case 27:
                           printf("\nRed Light");
                           DisplayMSG = 4;light = 1;Light(1);tim = 0;
                           break;
                        case 28:
                           printf("\nGreen Light");
                           openbar = 0; light=0; Light(0); tim = 0;    //DisplayMSG = 1;
                           break;*/
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     VDly(100);
                     tim = 0;
				  }
                  }
                 //else if(strncmp(buffer,"VIS@",4)==0){
					 else if(vis){
                     printf("VIS packet received\n");
					if(strncmp(vis,"VIS@",4)==0){
                     token=strtok(vis,"@");  // VIS
                     //sprintf(msg_return, "VISOK%s", buffer + 4);   // gia na to steilw pisw molis tupwthei to ticket
                     //printf("msg_return %s\n",msg_return);
                     token=strtok(NULL,"@");    // 0
                     temp = atoi(token);

                     switch(temp){
                        case 0:
                        	printf("\nEisodos dekti");
                           DisplayMSG = 3; tim = 0; openbar = 1;
                           token=strtok(NULL,"@");    // eisodos dekti
                           //token=strtok(NULL,"@");    // euxaristoyme
                           //token=strtok(NULL,"$");    // date|time|
                           tim = 0;
                           if (token !=NULL){
                              temp = strlen(token);
                              //printf("Token%d %s\n",temp,token);
                              token[temp]=0x00;
                              if (temp >0) printcard(token);
                              tim = 0;
                           }
                           OpenBar(1250);
                           //stat=0;
                           break;

                        /*case 1:  //AKYRH
                           DisplayMSG = 5;tim = 0;
                           break;*/
                        case 2:  //HDH STO PARKING
                           DisplayMSG = 7;tim = 0;      //5
                           break;
                        case 1:   //PLHRES
                           printf("\nParking FULL");
                           DisplayMSG = 4;light = 1;Light(1);tim = 0;
                           break;
                        case 3:  //S?S???? ???S????????
                           //DisplayMSG = 8;tim = 0;      //5
                           printf("\nEisodos dekti w/o ticket\n");
                           DisplayMSG = 3; tim = 0; openbar = 1;
                           token=strtok(NULL,"@");    // eisodos dekti
                           //token=strtok(NULL,"@");    // euxaristoyme
                           //token=strtok(NULL,"$");    // date|time|
                           tim = 0;
                           if (token !=NULL){
                              temp = strlen(token);
                              //printf("Token%d %s\n",temp,token);
                              token[temp]=0x00;
                              //if (temp >0) printcard(token);
                              tim = 0;
                           }
                           OpenBar(1250);
                           //stat=0;
                           break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     VDly(100);
                     tim = 0;
					 }
                  }
                  //else if(strncmp(buffer,"MAG@",4)==0){
					  else if(mag){
                     printf("\nMAG packet received");
					 if(strncmp(mag,"MAG@",4)==0){
                     token=strtok(mag,"@");
                     token=strtok(NULL,"@");
                     temp = atoi(token);

                     switch(temp){
                        case 0:
                        	printf("Mag - Eisodos dekti\n");
                           DisplayMSG = 3; tim = 0; openbar = 1;OpenBar(1250);
                           //stat=0;
                           break;
                        case 1:
                        	printf("Mag - Mi egkuri karta\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 2:
                        	printf("Mag - Mi egkuri karta\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 4:    //????? ??? S?? ??????G?
                        	printf("\nMag - Oxima idi sto parking\n");
                           DisplayMSG = 7; tim = 0;
                           break;
                        /*case 4:    //ле вяомовяеысг паяйимцй пкгяес
                        	printf("Mag - Parking plires\n");
                           DisplayMSG = 4; tim = 0;
                           break;
                        case 6:
                           printf("Mag - Mi egkuri karta\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 8:      //г йаята еимаи ломо циа диамуйтеяеусг
                        	printf("Mag - karta mono gia dianuktereusi\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 9:     //г йаята еимаи ломо циа диамуйтеяеусг
                        	printf("Mag - karta mono gia dianuktereusi\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 10:  //еисодос дейтг
                        	printf("Mag - Eisodos dekti\n");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           //stat=0;
                           break;
                        case 11:   //г йаята йкеиди
                        	printf("Mag - Karta kleidi, eisodos dekti\n");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           //stat=0;
                           break;
                        case 12:
                        	printf("Mag - Mi egkuri karta\n");
                           DisplayMSG = 5; tim = 0;
                           break;
                        case 13: //евете 1 овгла гдг сто паяйимцй
                        	printf("Mag - Oxima idi sto parking\n");
                           DisplayMSG = 7; tim = 0;
                           break;
                        case 14:
                        	printf("Mag - Mi egkuri karta\n");
                           DisplayMSG = 9; tim = 0;
                           break;*/
                        case 6:  //? ????? S?S ??? ???? ????????
                        	printf("\nMag - den uparxei upoloipo stin karta\n");
                           DisplayMSG = 6; tim = 0;
                           break;
                        case 16:  //сустгла апосвокглемо
                        	printf("Mag - systima apasxolimeno\n");
                           DisplayMSG = 8; tim = 0;
                           break;
                        /*case 17:
                        	printf("Mag - Eisodos dekti\n");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           token=strtok(NULL,"$");
                           tim = 0;
                           if (token !=NULL){
                              token=strtok(NULL,"@");
                              token=strtok(NULL,"@");
                              token=strtok(NULL,"$");
                              //token[temp]=0x00;
                              if (temp >0) printcard(token);
                              tim = 0;
                           }
                           //stat=0;
                           break;*/
                        case 18:
                           printf("\nReset PCL");
                           reboot();
                           break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     VDly(100);
                     tim = 0;
					 }
                  }
                  //else if(strncmp(buffer,"MGE@",4)==0){
					else if(mge){
						if(strncmp(mge,"MGE@",4)==0){
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     VDly(100);
                     tim = 0;
						}
                  }
                  /*else if (strcmp(buffer,"VIS@0@")>=0){
                     printf("\nVIS packer received\n");
                     printf("%sEOF\n",buffer);
                     printcard(buffer+6);
                     DisplayMSG = 2;
                     openbar = 1;
                     VDly(0,0,0,200);
                     tim =0;
                  }  */
                  else{
                     printf("\nWe got UNKNOWN TCP packet: %s",buffer);
                  }
                 VDly(200);
            }else
                VDly(200);
                printf("\nNo bytes read");
                conn_counter = 0;
            }
           VDly(100);
            }//while(tcp_tick)

         //If no connection, close the socket
            printf("\nConnection closed...");
         DisplayMSG = 0;
            sock_close(&socket);
            sock_test=sock_alive(&socket);
            printf("\nsock_test:%d",sock_test);
         buzzer(1);VDly(1000);buzzer(0);
            if(sock_alive(&socket)==0){
            printf("\nsock closed");}
            VDly(100);
     }
    }else{//if server tcp not open
        VDly(200); tmp++;
      if(tmp > 20){
         tmp=0;
         tim = 0;
         printf("TCP_Open Failed!\n");
         DisplayMSG = 0;
         VDly(1000);
      }
    }
}//while
}
//*****************************************************************************************************
void  Input_Task (void *data){
unsigned int count;
unsigned char temp[15];

data = data;
stat = 0;
count = LOOP_TIMEOUT;

VDly(500);

for(;;) {
   a = !digIn(0);
   c = digIn(2);     //anastrofo NC button
   printf("\nInput a:%d c:%d light:%d stat:%d",a,c,light,stat);

//ENTRY
   if((a)&&(stat==0)&&(!light)&&(DisplayMSG != 2)){
      DisplayMSG = 1;
      stat=1; count = LOOP_TIMEOUT;
   }//else if((a)&&(stat==0)&&(light)){
    //	DisplayMSG = 7;

   //}

   if((a)&&(stat==1)&&(c)){
	   sprintf(send,"%s%s%s",HTTP_HEADER1,"VIS!!-",HTTP_HEADER2);
      //sprintf(send,"VIS@1");
      DisplayMSG = 2;
      stat = 3;
      tim=0;
      printf("\nsending vis packet..");
      count = LOOP_TIMEOUT;
      VDly(1000);//VDly(0,0,5,0);
   }
   if((a)&&(stat==3)){
      count--;
      printf("\nCounting down at: %d",count);
      if(!count){
         stat=0;
         count=LOOP_TIMEOUT;
         printf("\nLoop Timeout!");
      }
   }

   if(!a){
   	count = 1;
      printf("\nLoop off, count set to 1!");
   }

   VDly(200);
}//for(;;)
}
//*****************************************************************************************************
void Nothing_Task(void *data){

data = data;
  //VDly(0,0,2,0);
  while(1){
   VDly(400);
   if(light){
      Light(1);
   }else{
      Light(0);
   }
   //if(openbar){
   //   OpenBar(1000);
   //   openbar=0;
   //   VDly(0,0,0,200);
   //}
 }//while
}
//*****************************************************************************************************
void RFID_Task(void *pdata){
   char read[25], rbuf[MAX_SENTENCE];
   char temp7[50];
   INT8U cnt, err;
   unsigned int count;

    pdata = pdata;

   serDflowcontrolOff();
   serDparity(PARAM_NOPARITY);
   serDdatabits(PARAM_8BIT);
   if(serDopen(9600))
      printf("Serial D-rfid OK\n");
   else
      printf("Serial D-rfid ERROR\n");

   memset(rbuf,0x00,sizeof(rbuf));
   while(1){
     serDwrFlush(); serDrdFlush();
     sprintf(read,"s"); serDputs(read); //einai gia reader V1.0
      VDly(300);
     cnt = serDread(read, 24, 200); read[cnt]=0x00;
       if ((cnt>=18) && (a)&&(!light)){        //==18
       	buzzer(1);VDly(100);buzzer(0);
         read[16]=0x00; sprintf(rbuf,read);
         printf("rfid: %s\n", rbuf);
         tim =0;
         sprintf(send,"%s%s%s%s",HTTP_HEADER1,"MAG!!MN",rbuf,HTTP_HEADER2);
		 //sprintf(send,"MAG@%s$",rbuf);
         DisplayMSG = 2;
         memset(rbuf,0x00,sizeof(rbuf));
         cnt=0;
         VDly(3000);tim =0;    // 1000
         //VDly(1000);tim =0;
         //VDly(1000);tim =0;
       }else{
   	 	//DisplayMSG = 5;
       	//count = LOOP_TIMEOUT;
         VDly(100);
   	 }
     VDly(300);
    } //while
}
//*****************************************************************************************************
void  Display_Task (void *data){

unsigned int len,timer,DisplayMSG_OLD;
   char FileSizemsg[50];

   data=data;
   VDly(3000);
   DisplayMSG = 10;   // anuparkto munima gia na fanei i IP kai i Version
   DisplayMSG_OLD = 1;

   while(1){

      if(a){      // BACKLIGHT CONTROL
         glBackLight(1); timer = 40;
      }else{
         timer--;
         if(!timer){
            glBackLight(0);
         }
      }
      if(DisplayMSG_OLD!=DisplayMSG){
         DisplayMSG_OLD = DisplayMSG;

         //glBackLight(1);//TEST
         switch(DisplayMSG){
            case 0:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,2,318,120,&fontdesc,  "пяобкгла\nсустглатос\nадумалиа\nсумдесгс",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"SYSTEM ERROR",0,0); VDly(100);
               DisplayMSG = 10;
               break;
            //Eisodos
            case 1:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,2,318,120,&fontdesc,  "йакыс гяхате\nпаяайакы\nпиесте\nто йоулпи",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"WELCOME\nPLEASE PRESS\nTHE BUTTON",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 2:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "паяайакы\nпеяилемете",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"PLEASE\nWAIT",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 3:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "еисодос дейтг\nеуваяистоуле",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"VALID ENTRY\nTHANK YOU",0,0); VDly(100);
               OSTimeDlyHMSM(0,0,1,700);
               //stat=0;
               //DisplayMSG = 10;
               break;
            case 4:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "паяйимцй\nпкгяес",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"PARKING\nFULL",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 5:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "лг ецйуяг\nйаята",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"INVALID\nCARD",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 6:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "кгнг пеяиодоу\nисвуос йаятас",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"CARD TIME\nEXPIRED",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 7:      //OXHMA HDH STO PARKING
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "овгла гдг\nсто паяйимцй",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"VEHICLE\nALREADY IN",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 8:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "сустгла\nапасвокглемо\nдойиласте нама",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"SYSTEM BUSY\nTRY AGAIN",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 9:      //г пимайида дем еимаи йатавыяглемг
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "лг йатавыяглемг\nпимайида",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"PLATE NUMBER\nNOT REGISTERED",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            default:
               break;
         }//switch(DisplayMSG)
      }else{
         VDly(100);
      }
      VDly(200);

   }//while(1)
}
//*****************************************************************************************************
// HELPER FUNCTIONS
void msDelay(unsigned int delay){
   auto unsigned long done_time;

   done_time = MS_TIMER + delay;
   while( (long) (MS_TIMER - done_time) < 0 );
}
//*****************************************************************************************************
void Light(unsigned int i){
   if (i){
      digOut(1,0);
   }else{
      digOut(1,1);
   }
}
//*****************************************************************************************************
void OpenBar(unsigned int time){
   digOut(0,0);  //1
   VDly(time);     // msDelay
   digOut(0,1);  //0

   VDly(500);

   digOut(0,0);  //1
   VDly(time);     // msDelay
   digOut(0,1);  //0
}
//*****************************************************************************************************
int reboot(){
   int wd;               // handle for a virtual watchdog
   unsigned long tm;
   VDly(100);
   tm = SEC_TIMER;
   wd = VdGetFreeWd(33); // wd activated, 9 virtual watchdogs available
                         // wd must be hit at least every 2 seconds
   while(SEC_TIMER - tm < 200)
   { // let it run for a minute

   }
}
//*****************************************************************************************************
int printcard(char printstr[60]){

char *pdate;
char *ptime;
char *plicense;
char *pbarcode;
static char part1[10],part2[10],initiator[60],temp[100];

char msb,lsb,lrc,posit[9],tmp[2];
int sz,i,ind;
int chr;
char lines[49];

pdate = strtok(printstr,"|");
//pdate[10]=0x00;
sprintf(date,"%s",pdate);

ptime = strtok(NULL,"|");
//ptime[8]=0x00;
sprintf(time,"%s",ptime);

plicense = strtok(NULL,"|");
sprintf(license,plicense);
//license[7]=0x00;
//pbarcode[20]=0x00;
pbarcode = strtok(NULL,"|");
sprintf(barcode,"%s",pbarcode);

/**************************/
/*print to the new printer*/
/**************************/
serCwrFlush;
serCrdFlush;

//Init the printer
serCputc(29);serCputc(246);
serCputc(29);serCputc(33);serCputc(33); //character size 50 34
serCputc(27);serCputc(97);serCputc(49); //Justification Center
//SET BARCODE WIDTH
   serCputc(29);serCputc(119);serCputc(2);
//Print the barcode
serCputc(29);serCputc(107);serCputc(4);serCputs(barcode);serCputc(0);      //4:code 39
//serCputc(10);      <--------
serCputc(27);serCputc(86);serCputc(1);
serCputc(174);serCputs(" ");serCputc(174);serCputs(" ");serCputc(174); serCputc(10);
serCputc(27);serCputc(86);serCputc(0);
//Print the "PARKING" header
serCputc(80);serCputc(65);serCputc(82);serCputc(75);serCputc(73);serCputc(78);serCputc(71);   //Parking
serCputc(10);
//Print the Parking name
serCputc(29);serCputc(33);serCputc(17); //27
serCputs("TELONEIOU"); serCputc(10);//"12 char"
//serCputs("DHM.KH A.E.");
serCputc(10);
serCputc(10);    //<------------
//serCputc(29);serCputc(33);serCputc(17);
//serCputs("            ");serCputc(10);

//Print the rest
serCputc(29);serCputc(33);serCputc(0);
serCputs("TEL:");serCputs("6932-383139");serCputc(10);
serCputs("DATE:");serCputs(pdate);serCputc(10);serCputs("TIME:");serCputs(ptime);serCputc(10);
serCputs("PLATE:");serCputs(plicense);serCputc(10);

//serCputc(29);serCputc(33);serCputc(17);
//serCputs("            ");serCputc(10);

//cut
serCputc(29);serCputc(248);serCputc(27);serCputc(105);
//Done printing, do the rest

//serCwrFlush;
//serCrdFlush;

//sprintf(send, "%s" , msg_return);
//memset(msg_return,0x00,sizeof(msg_return));

return(1);
}