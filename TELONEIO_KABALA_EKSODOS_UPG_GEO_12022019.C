/*******************************************************************************************************
PARKING TELONEIOU - KABALA - Termatiko Eksodoy
gmaragkakis
12/02/19
Code 4 OP7200

ddm920 motorized ticket dispenser		Serial C
CCD barcode reader Z5130 w/ converter	RS485
RFID Reader Mulit ISO 1.2.5            Serial D

Eksodos: IP 192.168.1.37
V.4: Code change for HTTP Protocol communication
********************************************************************************************************/

#define	STDIO_DEBUG_SERIAL	SADR
#define	STDIO_DEBUG_BAUD	57600
#define	STDIO_DEBUG_ADDCR

#class auto
#memmap xmem


// Net Configuration
#define TCPCONFIG 					0 //STATIC IP
#define USE_ETHERNET 				1 //STATIC IP
#define 	MY_IP_ADDRESS    			"192.168.1.37"
#define 	MY_NETMASK       			"255.255.255.0"
#define 	MY_GATEWAY   	   		"192.168.1.165"
#define  MY_NAMESERVER		      "192.168.1.165"
#define 	LOG_SERVER              "192.168.1.165"

#define 	SERVERPORT					80
#define 	TCP_TIMEOUT 				150
#define 	MAX_TCPBUF					255
#define KEEPALIVE_WAITTIME			75
#define KEEPALIVE_NUMRETRYS 		3
#define MAX_SENTENCE    			50

// Redefine uC/OS-II configuration constants as necessary
#define 	OS_MAX_EVENTS         	1       	// Maximum number of events (semaphores, queues, mailboxes)
#define 	OS_MAX_TASKS           	6  		// Maximum number of tasks system can create (less stat and idle tasks)
#define 	OS_MAX_QS					0
#define 	OS_Q_EN						0
#define 	OS_MBOX_EN             	1       	// Enable mailboxes
#define 	OS_TICKS_PER_SEC			128		//32
#define  STACK_CNT_512	       	6
#define 	STACK_CNT_1K          	0       	// number of 512 byte stacks (application tasks + stat task + prog stack)
#define 	STACK_CNT_2K 				0
#define  STACK_CNT_4K 			   1 			// number of 2K stacks

#define 	MSG_BUF_SIZE 				256


#define 		OS_TIME_DLY_HMSM_EN 	1
#define	HTTP_HEADER1		   "GET /themeForParking/mytrafficout.php?cmd="
#define	HTTP_HEADER2		   "&dev=TermOut&ip=192.168.1.37 HTTP/1.1\r\nHost:192.168.1.165:80\r\n\r\n"
#use 			"ucos2.lib"
#use 			"dcrtcp.lib" 			//Used for TCP/IP
#use 			"kpcust16key.lib"
#use 			"terminal9.lib"

#use 			"Arial16.lib"

//SERIALS
#define 		BINBUFSIZE     		255
#define 		BOUTBUFSIZE    		255
#define     CINBUFSIZE           255
#define     COUTBUFSIZE          255
#define 		DINBUFSIZE 				255
#define 		DOUTBUFSIZE 			255

#define 		TIMEOUT 20UL
#define 		MAXSIZE  256

#define 		VDly2(a)     		OSTimeDlyHMSM(0,0,0,a);
#define 		VDly(j)  			OSTimeDly(j*OS_TICKS_PER_SEC/1000);

#define 		LOOP_TIMEOUT		2 //15
#define 		ACK_Timeout	   	120 //40

// VARIABLES
OS_EVENT       *AckMbox;
OS_EVENT       *TxMbox;
OS_EVENT       RandomSem;

int offset;
char Tcp_State;
char send[150];
char buffer2[100];
unsigned int netstatus,openbar,a,b,DisplayMSG,tim,stat,keepTicket;
char strbuf[100];

//DISPLAY
fontInfo fontdesc;

// FUNCTION PROTOTYPES
void Display_Task (void *data);
void TCP_Task(void *ptr);
void BARCODE_Task (void *pdata);
void RFID_Task(void *pdata);
void Input_Task (void *pdata);
//void Watchdog_Task (void *pdata);

// HELPER FUNCTIONS
void msDelay(unsigned int delay);
int  reboot();
void OpenBar(unsigned int time);
void RsClick(unsigned int s);

// MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
// MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
// MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
void main (void){
   unsigned int error,x;
   unsigned char temp[100];
   unsigned char tmp[100];

   error = 0;
   keepTicket = 0;

   brdInit();
   digOutConfig(0x00);  //7
   digOut(0,0);   digOut(0,1);  //Bara     1->0
   digOut(1,1);

   //Initialize serial C
  // serCflowcontrolOff();
  //	serCparity(PARAM_EPARITY);
 //	serCdatabits(PARAM_7BIT);
  //	serCwrFlush; serCrdFlush;

 //	if(serCopen(19600))
  // 	printf("Serial C OK\n");
 //	else
 //  	printf("Serial C ERROR\n");

	//serCflowcontrolOff();
	//serCparity(PARAM_EPARITY);
	//serCdatabits(PARAM_7BIT);
	//serCwrFlush; serCrdFlush;

  	glInit();
	glSetContrast(14);
	keyInit();
	keypadDef();
	glkeypadInit(1);

	glXFontInit ( &fontdesc,22,26,0x20,0xFF,Arial16 );
	buzzer(1);msDelay(50);buzzer(0);msDelay(50);buzzer(1);msDelay(50);buzzer(0);

	OSInit();
	glkClearRegion(0,0,320,240);

   //error = OSTaskCreate(Watchdog_Task,  NULL, 512,  0);
	error = OSTaskCreate(TCP_Task ,     NULL, 4096, 1);
	error = OSTaskCreate(Input_Task,		NULL, 512,	3);
	error = OSTaskCreate(RFID_Task,   	NULL, 512 , 4);
   error = OSTaskCreate(BARCODE_Task,  NULL, 512 , 2);
	error = OSTaskCreate(Display_Task,  NULL, 512 , 5);
	printf("\nSTART OS\n");
	OSStart();

}
/*******************************************************************************************************/
void RFID_Task(void *pdata){
	char read[25], rbuf[MAX_SENTENCE];
	char temp7[50];
	INT8U cnt, err;

    pdata = pdata;

	serDflowcontrolOff();
	serDparity(PARAM_NOPARITY);
	serDdatabits(PARAM_8BIT);
	if(serDopen(9600))
		printf("\nSerial D-rfid OK\n");
	else
		printf("\nSerial D-rfid ERROR\n");

   memset(rbuf,0x00,sizeof(rbuf));
	while(1){
	  serDwrFlush(); serDrdFlush();
	  sprintf(read,"s"); serDputs(read); //einai gia reader V1.0
     OSTimeDlyHMSM(0,0,0,300);
	  cnt = serDread(read, 24, 200); read[cnt]=0x00;
     if (cnt>0) printf("\nRFID %d\n",cnt);
       if ((cnt>=18) && (a)){        //==18
       	buzzer(1);VDly(100);buzzer(0);
   	   read[16]=0x00; sprintf(rbuf,read);
	      printf("\nrfid: %s\n", rbuf);
         tim =0;
		 sprintf(send,"%s%s%s%s",HTTP_HEADER1,"MAG!!",rbuf,HTTP_HEADER2);
		   //sprintf(send,"MGE@%s$",rbuf);
         DisplayMSG = 2;
         memset(rbuf,0x00,sizeof(rbuf));
	      cnt=0;
         OSTimeDlyHMSM(0, 0, 1, 0);tim =0;
         OSTimeDlyHMSM(0, 0, 1, 0);tim =0;
         OSTimeDlyHMSM(0, 0, 1, 0);tim =0;
       }
     OSTimeDlyHMSM(0,0,0,300);
    } //while
}
/*******************************************************************************************************/
void  BARCODE_Task (void *pdata){

char MyBuff[50],tempx[50],CBuff[50],MyBuff_1[50];
  	int n,n1,n2,intbuff,mode,trials,timEnter,loopcrash,index;

  //Initialize serial C
   serCflowcontrolOff();
	serCparity(PARAM_NOPARITY);
	serCdatabits(PARAM_8BIT);
   if(serCopen(38400))
      printf("\nSerial C-Dispenser OK\n");
   else
      printf("\nSerial C-Dispenser ERROR\n");
   serCwrFlush; serCrdFlush;
    OSTimeDlyHMSM(0,0,1,0);
   //tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0x33; tempx[5]= 0x32;
   //serCwrite(tempx,6);
   //OSTimeDlyHMSM(0,0,1,0);

   //Initialize serial B
   serBflowcontrolOff();
	serBparity(PARAM_NOPARITY);
	serBdatabits(PARAM_8BIT);
   if(serBopen(9600))
      printf("\nSerial B-Barcode OK\n");
   else
      printf("\nSerial B-Barcode ERROR\n");
   serBwrFlush; serBrdFlush;
    OSTimeDlyHMSM(0,0,1,0);

   //////////////////////////////////////////

   mode = 0;
   trials = 0;
   timEnter = 0;
   //GoRead = 0;
   loopcrash = 0;
	while (1){

		if(netstatus)
      {
         n=0;
        	n = serCread(CBuff,50,300);
	      //CBuff[n]=0x00;

         switch(mode){
         	case 0:   //Boot Device
            	//serBwrFlush; serBrdFlush;
               printf("\nddm intake command\n");
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x02; tempx[4]= 0xCA; tempx[5]= 0x00; tempx[6]= 0xC8;
   				serCwrite(tempx,7);
               serCread(CBuff,6,100); //200
               OSTimeDlyHMSM(0,0,0,10);//100
               for (intbuff=0;intbuff<n;intbuff++)
            		{
                     if (CBuff[intbuff] == 0x35)
               			{
                           mode = 1;
               			}
                  }
               timEnter =0;
            	break;
            case 1:   //Idle
                //if ((n>0 && CBuff[n-2]==0x02 && CBuff[n-1]==0x35) || GoRead==1){  //Read Ticket
                  //GoRead = 0;
                  if(a)
                  {
                    	DisplayMSG = 2;
                     memset(CBuff,0x00,sizeof(CBuff));
                  	printf("\nRead Ticket\n");
                  	//memset(MyBuff,0x00,sizeof(MyBuff));
                     trials=0;
                     //serBwrFlush; serBrdFlush;
                  	//while (trials<400){     //200
                     n1 = 0;
                     //OSTimeDlyHMSM(0,0,0,500);
                  	n1=serBread(MyBuff,9,300);
                     if (MyBuff[0]==0xfc || MyBuff[0]==0xf8)
                     {
                     	memmove(MyBuff,MyBuff+1,strlen(MyBuff));
                        printf("\n\n\nDetecting and removing barcode header...\n\n\n");
                     }

                     //MyBuff[n1] = 0x00;
                     if (n1>0)
                     {
                     for (index=0;index<n1;index++)
                     	{
                     		printf("\nMyBuff content: %x\n",MyBuff[index]);
                        	//printf("\nIndex: %d\n",index);
                        }
                        if (MyBuff[n1-1]!= 0x0A)
                        {
                            serBclose();
                            serBflowcontrolOff();
									 serBparity(PARAM_NOPARITY);
									 serBdatabits(PARAM_8BIT);
   								 serBopen(9600);
                            serBwrFlush; serBrdFlush;
      									printf("\nSerial B-Barcode OK\n");
   									/*else
      									printf("\nSerial B-Barcode ERROR\n");*/
   								 serBwrFlush; serBrdFlush;
                           }
                     }

                     	printf("\nRead Barcode: %s\n",MyBuff);
                     	//trials++;
                     	//if (n1>4){serBwrFlush; serBrdFlush;break;}
                     	sprintf(send,"%s%s%s%s",HTTP_HEADER1,"VIS!!",MyBuff,HTTP_HEADER2);
						//sprintf(send,"BAR@%s@",MyBuff);
                        memset(MyBuff,0x00,sizeof(MyBuff));
                        		serBclose();
                           	serBflowcontrolOff();
										serBparity(PARAM_NOPARITY);
										serBdatabits(PARAM_8BIT);
   									serBopen(9600);
                           	serBwrFlush; serBrdFlush;
      									printf("\nSerial B-Barcode OK\n");
   									/*else
      									printf("\nSerial B-Barcode ERROR\n");*/
   									//serBwrFlush; serBrdFlush;
                        //printf("MYBUFF CONTENT AFTER SERVER: %s",MyBuff);
                        //serBwrFlush; serBrdFlush;
                     	//memset(MyBuff,0x00,sizeof(MyBuff));
                  		OSTimeDlyHMSM(0,0,0,10);
                     	//OSTimeDlyHMSM(0,0,0,10);
                     	loopcrash = 1;
                  		} else if (!a){
                     		serBclose();
                           serBflowcontrolOff();
									serBparity(PARAM_NOPARITY);
									serBdatabits(PARAM_8BIT);
   								serBopen(9600);
                           serBwrFlush; serBrdFlush;
      									printf("\nSerial B-Barcode OK\n");
   									/*else
      									printf("\nSerial B-Barcode ERROR\n");*/
   								serBwrFlush; serBrdFlush;
                           //serBclose();
                     		//OSTimeDlyHMSM(0,0,1,0);*/
                     		mode = 3;
                  			}

                  	if(loopcrash){
                     keepTicket = 3;
                 		mode = 2;
                 		trials = 0;
                  	timEnter = 0;
               				}
                           	else if (timEnter >60)
                              {   //30
	               					timEnter =0;
               						memset(MyBuff,0x00,sizeof(MyBuff));
                  					/*n = serCread(CBuff,20,300);
	      								CBuff[n]=0x00;
                  					memset(CBuff,0x00,sizeof(CBuff));
                  					tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x02;
                                 tempx[4]= 0xCA; tempx[5]= 0x00; tempx[6]= 0xC8;
   										serCwrite(tempx,7);
   										OSTimeDlyHMSM(0,0,0,100);
                  					n = serCread(CBuff,20,300);
	      								memset(CBuff,0x00,sizeof(CBuff));*/
                  					//printf("Idle: %d\n",n);
                  					//if (n < 4){
                   					//	mode=5;
                  					//}
               						}
                                 	else
                                    	{
               									timEnter++;
                  								tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x02;
                                          tempx[4]= 0xCA; tempx[5]= 0x00; tempx[6]= 0xC8;
   													serCwrite(tempx,7);
                  								OSTimeDlyHMSM(0,0,0,50);
                  								serCread(CBuff,6,200);
               								}
               memset(CBuff,0x00,sizeof(CBuff));
               memset(MyBuff,0x00,sizeof(MyBuff));
            	break;
            case 2:   //Wait  Server
            	printf("\nWait Server \n");

            	if (trials>30){
               	mode = 3;
                  trials = 0;
                  DisplayMSG = 14;
               	}else if(!loopcrash){
                  	mode = 3;
                  	trials = 0;
               	}
               if (keepTicket==1){
                  mode=4;
               	keepTicket=0;
               }else if(keepTicket==2){
                  mode=3;
               	keepTicket=0;
               }
               trials++;
            	break;
            case 3:   //Return Ticket
            	for (intbuff=0;intbuff<n;intbuff++)
               {
               	if (CBuff[intbuff]==0x37)
                  {
                  		n2=0;
                  		memset(MyBuff_1,0x00,sizeof(MyBuff_1));
               			n2=serBread(MyBuff_1,10,300);
                           printf("\nMYBUFF CONTENT AFTER DOUBLE READING: %s\n",MyBuff_1);
                           if (n2>0)
                           {
                              serBclose();
                           	serBflowcontrolOff();
										serBparity(PARAM_NOPARITY);
										serBdatabits(PARAM_8BIT);
   									serBopen(9600);
                           	serBwrFlush; serBrdFlush;
      									printf("\nSerial B-Barcode OK\n");
   									/*else
      									printf("\nSerial B-Barcode ERROR\n");*/
   									serBwrFlush; serBrdFlush;
                              }
               	mode = 0;
               	}
                }
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0xC6; tempx[5]= 0xC7;
		      	serCwrite(tempx,6);
               OSTimeDlyHMSM(0,0,0,50);
               serCread(CBuff,6,200);
            	break;
            case 4:   //Keep Ticket
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0xC4; tempx[5]= 0xC5;
		      	serCwrite(tempx,6);
               OSTimeDlyHMSM(0,0,0,50);
               serCread(CBuff,6,200);
               OSTimeDlyHMSM(0,0,1,0);	//02/02/2017
               loopcrash = 0;
               mode = 0;
            	break;
            /*case 5:   //Reset Device
            	printf("\nDispenser RESET!!!\n");
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0x33; tempx[5]= 0x32;
		      	serCwrite(tempx,6);
      			serCread(CBuff,6,200);
               serCwrFlush;serCrdFlush;
               serCclose();
               OSTimeDlyHMSM(0,0,0,200);
               if(serCopen(38400))
                  printf("\nSerial C-Dispenser OK\n");
               else
                  printf("\nSerial C-Dispenser ERROR\n");
               serCwrFlush; serCrdFlush;
               //tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0xC6; tempx[5]= 0xC7;
		      	//serCwrite(tempx,6);
               //OSTimeDlyHMSM(0,0,0,200);
               OSTimeDlyHMSM(0,0,0,500);//100
               n = serCread(CBuff,50,300);
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0x28; tempx[5]= 0x29;
		      	serCwrite(tempx,6);
               OSTimeDlyHMSM(0,0,0,100);
               n = serCread(CBuff,50,300);
               OSTimeDlyHMSM(0,0,0,200);//100
               n = serCread(CBuff,50,300);
               tempx[0]= 0x01; tempx[1]= 0x01; tempx[2]= 0x00; tempx[3]= 0x01; tempx[4]= 0x28; tempx[5]= 0x29;
		      	serCwrite(tempx,6);
               OSTimeDlyHMSM(0,0,0,100);
               n = serCread(CBuff,50,300);
         	   CBuff[n]=0x00;
	            if (n>0) printf("\n");
	            for (intbuff=0;intbuff<n;intbuff++){
	             printf("Status %d",CBuff[intbuff]);
	            }
               OSTimeDlyHMSM(0,0,0,200);
               if (CBuff[5]==0x12){
                  mode = 1;
               	timEnter = 0;
                  //GoRead = 1;
               }else{
               	mode = 0;
               }
            	break;*/
         }
         /*memset(MyBuff,0x00,sizeof(MyBuff));
         memset(CBuff,0x00,sizeof(CBuff));
			memset(tempx,0x00,sizeof(tempx));*/

		}//if netstatus
      	memset(MyBuff,0x00,sizeof(MyBuff));
         memset(CBuff,0x00,sizeof(CBuff));
			memset(tempx,0x00,sizeof(tempx));
         memset(MyBuff_1,0x00,sizeof(MyBuff_1));
      serCwrFlush;serCrdFlush;
      serBwrFlush; serBrdFlush;
		OSTimeDlyHMSM(0,0,0,100); //200ms
	}//while
}

/*******************************************************************************************************/
void  Display_Task (void *data){

unsigned int len,timer,DisplayMSG_OLD;
char FileSizemsg[50];

data=data;
//VDly(100);
DisplayMSG = 20;
DisplayMSG_OLD = 1;

   while(1){

      if(a){		// BACKLIGHT CONTROL
			glBackLight(1); timer = 40;
		}else{
			timer--;
			if(!timer){
         	glBackLight(0);
         }
		}
      //printf("\nDisplay Task\n");

      if(DisplayMSG_OLD != DisplayMSG){
			DisplayMSG_OLD = DisplayMSG;

      	//glBackLight(1);//TEST
	      switch(DisplayMSG){
	         case 0:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,2,318,120,&fontdesc,  "пяобкгла\nсустглатос\nадумалиа\nсумдесгс",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"NETWORK\nERROR",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
	         case 1:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "еисацете то\nеиситгяио сас",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"INSERT YOUR\nTICKET",0,0); VDly(100);
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
	            glkMsgBox(0,0,318,120,&fontdesc,  "енодос дейтг\nеуваяистоуле",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"VALID EXIT\nTHANK YOU",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
	         case 4:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "AKYPO\nеиситгяио",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"INVALID\nTICKET",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
	         case 5:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "BARCODE ERROR\nдойиласте нама",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"BARCODE ERROR\nTRY AGAIN",0,0); VDly(100);
	            OSTimeDlyHMSM(0,0,3,0);
	            DisplayMSG = 1;
	            break;
	         case 6:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "кгнг пеяиодоу\nисвуос йаятас",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"CARD TIME\nEXPIRED",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
            case 7:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "паяайакы\nепийоимымгсте\nле то\nталеио",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"PLEASE\nCONTACT CASHIER",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
            case 8:
	            glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "лг ецйуяг\nйаята",0,0); VDly(100);
				   glkMsgBox(0,120,318,120,&fontdesc,"INVALID\nCARD",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
            case 9:
	            glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "пяепеи ма\nпкгяысете\nпяим тгм\nенодо сас",0,0); VDly(100);
				   glkMsgBox(0,120,318,120,&fontdesc,"YOU HAVE\nTO PAY\nBEFORE EXIT",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
            case 10:
	            glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "то овгла\nде бяисйетаи\nсто PARKING",0,0); VDly(100);
				   glkMsgBox(0,120,318,120,&fontdesc,"VEHICLE NOT\nIN PARKING LOT",0,0); VDly(100);
	            //DisplayMSG = 10;
	            break;
            case 11:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "йахустеягсате\n15', пкгяысте\nтг диажояа",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"YOU ARE 15'\nLATE. PAY THE\nDIFFERENCE",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 12:
               glkClearRegion(0,0,320,240);
               glkMsgBox(0,0,318,120,&fontdesc,  "сустгла\nапасвокглемо\nдойиласте нама",0,0); VDly(100);
               glkMsgBox(0,120,318,120,&fontdesc,"SYSTEM BUSY\nTRY AGAIN",0,0); VDly(100);
               //DisplayMSG = 10;
               break;
            case 13:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "DATABASE ERROR\nдойилате нама",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"DATABASE ERROR\nTRY AGAIN",0,0); VDly(100);
	            OSTimeDlyHMSM(0,0,3,0);
            case 14:
	            glkClearRegion(0,0,320,240);
	            glkMsgBox(0,0,318,120,&fontdesc,  "то овгла\nде бяисйетаи\nстгм енодо",0,0); VDly(100);
	            glkMsgBox(0,120,318,120,&fontdesc,"VEHICLE NOT\nAT EXIT SPOT",0,0); VDly(100);
	            OSTimeDlyHMSM(0,0,3,0);
	         default:
	            break;
	      }//switch(DisplayMSG)
      }else{
      	OSTimeDlyHMSM(0,0,0,100);
      }
		OSTimeDlyHMSM(0,0,0,200);
   }//while(1)
}
/*******************************************************************************************************/
void  Input_Task (void *data){

unsigned int count,conn_counter;
unsigned char msg[80],temp[15];

data = data;
stat = 0;
count = LOOP_TIMEOUT;
conn_counter =0;


OSTimeDlyHMSM(0,0,0,500);

for(;;) {
	a = !digIn(0);
	//printf("Input a:%d\n",a);   // b:%d .... b

//EXIT
	if((a)&&(stat==0)){
		stat=1; count = LOOP_TIMEOUT;
	}

////////////
	if(!a){
		count--;
      printf("\nCounting down at: %d",count);
		if(!count){
			stat=0;
			count=LOOP_TIMEOUT; //printf(" R ");
         printf("\nLoop Timeout!");
		}
	}
   OSTimeDlyHMSM(0,0,0,200);

}//for(;;)
}
/*******************************************************************************************************/
void TCP_Task(void *ptr){

//#memmap xmem
static tcp_Socket socket;
longword host,ping_who;
char	buffer[100], ip[128];
unsigned int temp;
char *token;
char *err,*err2;
char *ack,*vis,*mag,*mge;
unsigned int i,bytes_read,x,sock_test,tmp,conn_counter;
x=0;
tim =0;
netstatus=0; // Connected or NOT
tmp=0;


sock_init();
//sock_mode(&socket, TCP_MODE_ASCII);
memset(buffer,0x00,sizeof(buffer));
memset(send,0x00,sizeof(send));
memset(ip,0x00,sizeof(ip));

while (ifpending(IF_DEFAULT) == IF_COMING_UP) {
   tcp_tick(NULL);
   OSTimeDlyHMSM(0,0,0,200);
}
ping_who = resolve(LOG_SERVER);

// Print who we are...
//glBackLight(1);
sprintf(ip, "IP address is:%s", inet_ntoa(buffer, gethostid()) );
glkMsgBox(0,0,320,120,&fontdesc,ip,0,0); VDly(100);
glkMsgBox(0,120,318,120,&fontdesc,"Exit 1 \n Code: v.4",0,0); VDly(100);
VDly(2000); glkClearRegion(0,0,320,240);
//glBackLight(0);
printf("%s\n, Code v.4\n",ip);

while(1){
host = resolve(LOG_SERVER);
OSTimeDlyHMSM(0,0,0,200);//Dinw delay giati mplokarei
tcp_tick(&socket);

    if (tcp_open(&socket ,0,host,SERVERPORT, NULL)){
         while (!sock_established(&socket)){
           	tcp_tick(&socket); err=sockerr(&socket); printf("sockerr: %s\n", err);
		   	OSTimeDlyHMSM(0,0,0,100); x++;
            if(x>18){
					x=0; netstatus=0;
            	DisplayMSG = 0;
               OSTimeDlyHMSM(0,0,1,0);
					break;
				}
		 	}
         if (sock_established(&socket)){
		    	printf("\nsock established");
				tcp_keepalive(&socket,4);
				netstatus = 1;
				buzzer(1);VDly(250);buzzer(0);VDly(150);buzzer(1);VDly(250);buzzer(0);
				DisplayMSG = 1;
            while( tcp_tick(&socket)&& pd_havelink(IF_DEFAULT)==1){

					if(strlen(send)){
						sock_puts(&socket, send);
						printf("\nTCP_Send:%s",send);
				  		send[0]=0x0;
                  //DisplayMSG = 0;
            	}
	            if(tim>ACK_Timeout){
					sprintf(send,"%s%s%s",HTTP_HEADER1,"ACK",HTTP_HEADER2);
	                 //sprintf(send, "ACK@");     //sprintf(send, "ACK@ID:2$");
                    DisplayMSG = 1;
                    tim =0;
                    conn_counter++;
               	  printf("\nconn_counter:%d\n",conn_counter);
	            }
               if(conn_counter>1){	//Lost connection
						printf("\nLOST CONNECTION\n");
						conn_counter=0;
						break;
               }

               tim++;

			 		//Check if there's something to receive.
             	if(sock_bytesready(&socket)>1){
						bytes_read=sock_fastread(&socket,buffer,sizeof(buffer)-1);
						if(bytes_read>0) {
				 	   	buffer[bytes_read]=0;
				 	   	printf("\nWe got TCP: %s",buffer);
					ack = strstr(buffer,"ACK@");
                  	vis = strstr(buffer,"VIS@");
                  	mag = strstr(buffer,"MAG@");
                  	mge = strstr(buffer,"MGE@");
                     if(ack){
					 //if(strncmp(buffer,"ACK@",4)==0){
                     printf("\nAck packet received");
					if(strncmp(ack,"ACK@",4)==0){
                     token=strtok(ack,"@");
							token=strtok(NULL,"@");
							temp = atoi(token);

                     switch(temp){
                        case 0:
                           tim = 0;
                           DisplayMSG = 1;
                        	break;
                        case 1:
                           DisplayMSG = 1;tim = 0;
                        	break;
                        case 20:
                        	printf("\nACK - Parking FULL");
                           DisplayMSG = 1;tim = 0;
                        	break;
                        case 21:
                           printf("\nACK - Parking FULL");
                           DisplayMSG = 1;tim = 0;
                        	break;
                        case 2:
                           printf("\nOpen Barrier SERVER");
									DisplayMSG = 3;openbar = 1; OpenBar(1250);tim = 0;
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     OSTimeDlyHMSM(0,0,0,100);
                     tim = 0;
					}
						}
                  //else if(strncmp(buffer,"BAR@",4)==0){
							else if(vis){
							printf("\nBAR packet received");
					if(strncmp(vis,"VIS@",4)==0){
                     token=strtok(vis,"@");
							token=strtok(NULL,"@");
							temp = atoi(token);

                     switch(temp){
                     	case 0:  //Eksodos dekth
                           DisplayMSG = 3; openbar = 1; OpenBar(1250);tim = 0;
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=1;
                           }
                        	break;
                        case 31:  //AKYRH karta epik me tameio
                           DisplayMSG = 8;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                        case 1:  //akyro eishthrio
                           DisplayMSG = 4;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                        case 2:  //prepei na plhrosete prin thn eksodo sas
                           DisplayMSG = 9;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                        case 36: //database error
                           DisplayMSG = 5;tim = 0;
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                        case 3: // plhroste th diafora
                           DisplayMSG = 7;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                        case 38:  //dokimaste ksana
                           DisplayMSG = 5;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                           if(keepTicket==3){
                           	keepTicket=2;
                           }
                        	break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     OSTimeDlyHMSM(0,0,0,100);
                     tim = 0;
					}
						}
						else if(mag){
                  //else if(strncmp(buffer,"MAG@",4)==0){
                     printf("\nMAG packet received");
					 if(strncmp(mag,"MAG@",4)==0){
                     token=strtok(mag,"@");
							token=strtok(NULL,"@");
							temp = atoi(token);

                     switch(temp){
                     	case 0:     //EKSODOS DEKTH
	                        printf("\nMAG - Eksodos dekti");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                        case 11:    //H KARTA KLEIDI
                        	printf("\nMAG - Eksodos dekti - karta kleidi");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                        case 12:
                        	printf("\nMAG - Epikoinwniste me to tameio");
                           DisplayMSG = 7; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 1:
                        	printf("\nMAG - To oxima einai ektos");
                           DisplayMSG = 10; tim = 0;//8
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 14:
                        	printf("\nMAG - Mi egkuri karta");
                           DisplayMSG = 8; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 2:
                        	printf("\nMAG - Liksi periodou isxuos kartas");
                           DisplayMSG = 6; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 16:
                        	printf("\nMAG - Dokimaste ksana");
                           DisplayMSG = 12; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 18:
                           printf("\nReset PCL");
									reboot();
                        	break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     OSTimeDlyHMSM(0,0,0,100);
                     tim = 0;
					 }
						}
						else if(mge){
                  //else if(strncmp(buffer,"MGE@",4)==0){
                     printf("\nMGE packet received");
					 if(strncmp(mge,"MGE@",4)==0){
                     token=strtok(mge,"@");
							token=strtok(NULL,"@");
							temp = atoi(token);

                     switch(temp){

                        case 1: //akyrh karta
                        	printf("\nMGE - Akuri karta");
                        	DisplayMSG = 8; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        break;
                        case 5: //prepei na plhrosete  prhn thn eksodo sas
                        	printf("\nMGE - Prepei na plirwsete prin tin eksodo sas");
                        	DisplayMSG = 9;tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        break;
                        case 6: //database error
                        	printf("\nMGE - Database error - try again");
                        	DisplayMSG = 13;tim = 0;
                        break;
                        case 7: //kathysterhsate na kanete eksodo
                           printf("\nMGE - Kathusterisate 15 lepta");
                        	DisplayMSG = 11;tim = 0; //7
                        	buzzer(1);VDly(1000);buzzer(0);
                        break;
                     	case 40:    //eksodos dekth
                        	printf("\nMGE - EKSODOS DEKTI");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                        case 43:   //TO OXHMA DEN BRISKETAI STO PARKING
                        	printf("\nMGE - To oxima de vrisketai sto parking");
                           DisplayMSG = 10; tim = 0;  //8
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 10:
                        	printf("\nMGE - Eksodos dekti");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                        case 11:   //KARTA KLEIDI PASSPARTUT
                        	printf("\nMGE - Passpartut");
                           DisplayMSG = 3; tim = 0; openbar = 1; OpenBar(1250);
                           buzzer(1);VDly(100);buzzer(0);VDly(200);buzzer(1);VDly(300);buzzer(0);
                        	break;
                        case 12:   //H KARTA EINAI MONO DIANYKTEREUSHS
                        	printf("\nMGE - Karta dianuktereusis");
                           DisplayMSG = 7; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 13:
                           printf("\nMGE - Akuri karta");
                           DisplayMSG = 8; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 14:  //h karta den einia kataxorhmenh
                        	printf("\nMGE - Mi kataxwrimeni karta");
                           DisplayMSG = 8; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 15:    //h karta sas den exei ypoloipo
                        	printf("\nMGE - Karta xwris upoloipo");
                           DisplayMSG = 6; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 16:
                           printf("\nMGE - Barcode error, dokimaste ksana");
                           DisplayMSG = 5; tim = 0;
                           buzzer(1);VDly(1000);buzzer(0);
                        	break;
                        case 18:
                           printf("\nReset PCL");
									reboot();
                        	break;
                     }
                     memset(buffer,0x00,sizeof(buffer));
                     memset(token,0x00,sizeof(token));
                     OSTimeDlyHMSM(0,0,0,100);
                     tim = 0;
					 }
						}
						else{
							printf("\nWe got UNKNOWN TCP packet: %s",buffer);
						}
	                 OSTimeDlyHMSM(0,0,0,200);
            	}else
                	printf("\nNo bytes read");
                	conn_counter = 0;
	               OSTimeDlyHMSM(0,0,0,200);
	            }
     		  		OSTimeDlyHMSM(0,0,0,100);
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
            OSTimeDlyHMSM(0,0,0,100);
     		}
    	}else{//if server tcp not open
        OSTimeDlyHMSM(0,0,0,200); tmp++;
    	if(tmp > 20){
         tmp=0;
         tim = 0;
         printf("TCP_Open Failed!\n");
         DisplayMSG = 0;
         OSTimeDlyHMSM(0,0,1,0);
      }
    }
	}//while
}
/*******************************************************************************************************/
/*void Watchdog_Task(void *pdata){
int wd;
wd = VdGetFreeWd(255);
  while(1){
    VdHitWd(wd);
    //printf("wd reset!\n");
    OSTimeDlyHMSM(0,0,0,500);
 }
}*/
/*******************************************************************************************************/
int reboot(){
	int wd;               // handle for a virtual watchdog
	unsigned long tm;
	OSTimeDlyHMSM(0,0,0,100);//VDly(100);
	tm = SEC_TIMER;
	wd = VdGetFreeWd(33); // wd activated, 9 virtual watchdogs available
	                      // wd must be hit at least every 2 seconds
	while(SEC_TIMER - tm < 200)
	{ // let it run for a minute

	}
}
/*******************************************************************************************************/
// HELPER FUNCTIONS
void msDelay(unsigned int delay){
   auto unsigned long done_time;

   done_time = MS_TIMER + delay;
   while( (long) (MS_TIMER - done_time) < 0 );
}
void OpenBar(unsigned int time){
   digOut(0,0);  //1
   VDly(time);     // msDelay
   digOut(0,1);  //0

   VDly(500);

   digOut(0,0);  //1
   VDly(time);     // msDelay
   digOut(0,1);  //0
}
/*void RsClick(unsigned int s){
   //printf("Open Bar");
   if (s==1){
   	digOut(1,1);
   }else{
      digOut(1,0);
   }
}*/
/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/

