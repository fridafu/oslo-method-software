/* SIRI-delen implementert, 
endret *pSIRIEVRD=ENABLE til CLEAR
satt inn *pSIRICOST   = 0x81; foran eventloop
*/


#include      <stdio.h>
#include       <time.h>
#include       <smem.h>
#include <ces/uiocmd.h>
#include <ces/vmelib.h>
#include     <signal.h>
#include      <types.h>

#define PHYS_2_PCI(p) ((p)|0x80000000)
#define BOOL(x)   (!(!(x)))
#define bit(a,x)  BOOL((a) & (1L << (x)))
#define swap(x) ((x&0xff000000)>>24)+((x&0x00ff0000)>>8)+((x&0x0000ff00)<<8)+((x&0x000000ff)<<24)
#define swap1(x) (((x&0xff000000)>>24)+((x&0x00ff0000)>>8)+((x&0x0000ff00)<<8)+((x&0x000000ff)<<24)&0x00000001)
#define WAIT(x) {volatile long ii;for(ii=x;ii;ii--);}    /* 1 loop = 0.1647us */
#define WAITupdate(a,b) if(a>b){WAIT(a-b);b=a+2;}

#define usLOOP         0.1647             /*Number of microseconds/loop       */
#define MESSNUM        10                 /*Number of words in message box    */
#define MAXBUF1        32767              /*Upper limit of buffer 1           */
#define MAXBUF2        65535              /*Upper limit of buffer 2           */
#define EVENT_HEADER   0xf000             /*Event_header identification code  */
#define SAFE           200                /*Maximum eventlength with 5 TPU`s  */
#define BUFFER1        1
#define BUFFER2        2
#define EMPTY          0x0                /*Reset semaphore                   */
#define FULL           0x1                /*Set semaphore                     */

#define CPUADDR        0xff010000         /*Phys. addr. for SRAM -> A24 slave */
#define SLV24ADDR      0x00850000         /*Absolute VME A24 slave address    */
                                          /*with hex switch = 5 on CPU card   */
#define CLEAR          0x0
#define ENABLE         0x1
#define DISABLE        0x0
#define NUMBUF         0x1                /*Number of ring buffers (1 - 32)   */
#define START          0x1
#define STOP           0x0
#define AM24           0x39               /*Address modifier for 24 bits AM24 */
#define AM32           0x09               /*Address modifier for 32 bits AM32 */

#define NEXTREG        0xf0ffff09
#define T1STATUS       0xf0ffff00         /*Registers in Master TPU1          */
#define T1NUMREG       0xf0ffff05
#define T1PATTERN      0xf0ffff06
#define T2STATUS       0xf0ffff10         /*Registers in Slave  TPU2          */
#define T2NUMREG       0xf0ffff15
#define T2PATTERN      0xf0ffff16
#define T3STATUS       0xf0ffff20         /*Registers in Slave  TPU3          */
#define T3NUMREG       0xf0ffff25
#define T3PATTERN      0xf0ffff26
#define NIMCSR         0xf0efff9c         /*Control & Status register         */
#define NIMPATTERN     0xf0efff9e         /*Pattern register                  */
#define NIMbaseaddr    0xf0efffa0         /*Baseaddress for NIM ADCs          */

#define SIRINUBUF 0xf0f04007  /*CSR, number of buffers in ring buffer (1 - 32)*/
#define SIRINXBUF 0xf0f04006  /*CSR, next buffer to be read by VME            */
#define SIRIIRQVC 0xf0f04005  /*CSR, interrupt vector register                */
#define SIRIEVRDY 0xf0f04004  /*CSR, event has been read by ROCO              */
#define SIRIBUFAD 0xf0f04003  /*CSR, address of SIRI buffer                   */
#define SIRIEVEAD 0xf0f04002  /*CSR, address of SIRI event                    */
#define SIRICOST  0xf0f04001  /*CSR, main control and status register         */
#define SIRICLR   0xf0f04000  /*CSR, clear command register                   */
#define SIRIDSR   0xf0f00000  /*DSR, start of event words                     */

char tpu_2, tpu_3;
long i, j, f, q, SIRI, TPUS;
int curbuf, curmax, par, para, value, pileup, onerun, bufwait;
u_short patcopy1, patcopy2, patcopy3;

time_t now;
struct tm *date;
char sSTART[80], sSTOP[80], inbuf[130];
int rx, ex, tx, r1, r2, e1, e2, t0, t1, t2;

u_short *pT1STATUS, *pT2STATUS,  *pT3STATUS;
u_char  *pT1NUMREG, *pT2NUMREG,  *pT3NUMREG;
u_short *pT1PATTERN,*pT2PATTERN, *pT3PATTERN;
u_short *pNIMCSR,   *pNIMPATTERN;
u_short *pNIMadc;
u_char  *pNEXTREG;

int SIRIpat1, SIRIpat2, SIRIpat3, SIRIpat4;
int imax, k, chipadi;
int evno[32], chipad[32], energy[32];                 /*Storing 16 dE-E values*/
int   dE[64],      E[64];                             /*64 telescopes         */
int one = 1L;                                         /*for bit-pattern       */

u_char  *pSIRICLR,   *pSIRICOST,  *pSIRIEVEAD, *pSIRIBUFAD;
u_char  *pSIRIEVRDY, *pSIRIIRQVC, *pSIRINXBUF, *pSIRINUBUF;
u_short *pSIRIDSR,   SIRIadc[64];

long *pBUFFER_ADDRESS, *pBUFFER_LENGTH, *pSEMA_1, *pSEMA_2;
long *pVMESTATUS, *pSUNSTATUS;
long header, p1;
long records, events, recordsold, eventsold;

int  mem24, mem32;
long *pmes, *pbuf;
long bufferbytes, messagebytes, err;
u_long ad, vad, vadr;
uio_mem_t cmem_dsc;
struct pdparam_slave sp;

float uPUR, uNIM, uCAMAC, uSIRI, uEVENT;       /*Extra delay until ADCs finish*/
int loopTOT, loopPUR, loopNIM, loopCAMAC, loopSIRI, loopEVENT;
int loop500ns, loop1us;

#define BRANCH      1                      /*Rotary switch on Branch Driver   */
#define CRATE       1                      /*Rotary switch on Crate Controller*/
#define PUR1SLOT    7                      /*Position of PUR1                 */
#define ADC1SLOT   11                      /*Position of ADC1                 */
#define ADC2SLOT   12
#define ADC3SLOT   13
#define ADC4SLOT   14
#define TDC1SLOT   15
#define TDC2SLOT   16
#define TDC3SLOT   17
#define TDC4SLOT   18

u_short tempc, tpp0, tpp1;          /*Temporary storage for CAMAC, PUR */

int cadc1[8];                              /*ADC1 data registers              */
int cadc2[8];                              /*ADC2 data registers              */
int cadc3[8];                              /*ADC3 data registers              */
int cadc4[8];                              /*ADC4 data registers              */
int ctdc1[8];                              /*TDC1 data registers              */
int ctdc2[8];                              /*TDC2 data registers              */
int ctdc3[8];                              /*TDC3 data registers              */
int ctdc4[8];                              /*TDC4 data registers              */
int cpur1[4];                              /*PUR1 data registers              */

int cmd;
char leaveloop;                            /*Break loop interrupt (Ctrl_C)    */

void keyb_int(int sig_num);
void Menu();
void Loop();
void Dumpbuf();
void Status();
void Parameters();
void DisEnAble();
void Microsecond();
void SLAVEinitiate();
void CAMACopen();
void CAMACconfig();
void CAMACclose();
void SIRIopen();
void SIRIconfig();
void SIRIclose();
void TPUopen();
void TPUconfig();
void TPUclose();
void MESSAGEBOXopen();
void MESSAGEBOXconfig();
void MESSAGEBOXclose();
void BUFFERopen();
void BUFFERclear();
void BUFFERclose();
void eventmonitor();

int main() {                                      /*Interrupt handler (Ctrl_C)*/
   if(signal(SIGINT, SIG_IGN) != SIG_IGN){ 
      signal(SIGINT, keyb_int);
   }

   printf(" ____________________________________________________________ \r\n");
   printf("|                                                            |\r\n");
   printf("|                          Crash 0.9                         |\r\n");
   printf("|                                                            |\r\n");
   printf("|          A VME-based data acquisition system for           |\r\n");
   printf("|            the CACTUS/SIRI multidetector system            |\r\n");
   printf("|     Written for the CES RTPC8067 single board processor    |\r\n");
   printf("|       with a PowerPC 603 @ 66 MHz CPU running LynxOS       |\r\n");
   printf("|                                                            |\r\n");
   printf("| E-mail  : magne.guttormsen@fys.uio.no                      |\r\n");
   printf("| Created : 29-06-1999                                       |\r\n");
   printf("| Modified: 29-06-1999                                       |\r\n");
   printf("|____________________________________________________________|\r\n");
   printf("                                                              \r\n");

   SLAVEinitiate();
   MESSAGEBOXopen();
   BUFFERopen();
   BUFFERclear();
   CAMACopen();
   CAMACconfig();
   SIRIopen();                                      /*comment away if not SIRI*/
   SIRIconfig();                                    /*comment away if not SIRI*/
   TPUopen();
   TPUconfig();
   MESSAGEBOXconfig();

   events    =  0;
   records   =  0;

   uPUR      =  8.0;
   uNIM      = 45.0;
   uCAMAC    = 25.0;
   uSIRI     =  8.0;
   uEVENT    =  0.0;
   loopPUR   = (int)((uPUR   / usLOOP) + 0.5);
   loopNIM   = (int)((uNIM   / usLOOP) + 0.5);
   loopCAMAC = (int)((uCAMAC / usLOOP) + 0.5);
   loopSIRI  = (int)((uSIRI  / usLOOP) + 0.5);
   loopEVENT = (int)((uEVENT / usLOOP) + 0.5);
   loop500ns = (int)((0.500  / usLOOP) + 0.5);
   loop1us   = (int)((1.000  / usLOOP) + 0.5);

   Menu();

   do{
      eventsold  = events;                                 /*Making statistics*/
      recordsold = records;
      if(cmd == 'r'|cmd == 'o'){
         *pVMESTATUS = swap(STOP);                 /*Set VME status to stopped*/
         time( &now );
         date = localtime( &now );
         strftime( sSTOP, 80, "%c", date );
         printf("\nVME data acquisition stopped at %s\n", sSTOP );
         printf("Records = %ld    Events = %ld \n\n", recordsold, eventsold);
      }

      leaveloop ='n';                          /*Do not jump out of event-loop*/
      printf("eventbuilder+>");
      cmd = readchar();
      switch (cmd) {
         case 'r':onerun=0;Loop();  break;
         case 'o':onerun=1;Loop();  break;
         case 'd':Dumpbuf();        break;
         case 's':Status();         break;
         case 'e':DisEnAble();      break;
         case 'm':Microsecond();    break;
         case 'p':Parameters();     break;
         case 'h':Menu();           break;
         case '\n':                 break;
         case '*':                  break;
         default :printf(" Illegal command, try again\n");
      }
   }while (cmd != '*');

   TPUclose();
   SIRIclose();                                     /*comment away if not SIRI*/
   CAMACclose();
   MESSAGEBOXclose();
   BUFFERclose();
   return 0;
}                                                           /*End main program*/


void Loop(){
   events  = 0;                                    /*Reset numbers for new run*/
   records = 0;

   time( &now );
   date = localtime( &now );
   strftime( sSTART, 80, "%c", date );
   printf( "\nVME data acquisition started at %s\n", sSTART );
   printf( "To jump out of event-loop, press Ctrl_C\n");
  
   waitstate:
   *pT1STATUS    = CLEAR;
   *pVMESTATUS   = swap(START);           /*Set VME status to started         */
   *pNIMPATTERN  = CLEAR;                 /*Reset NIM ADCs for event          */
   *pSEMA_1      = swap(EMPTY);           /*Buffer 1 is empty                 */
   *pSEMA_2      = swap(EMPTY);           /*Buffer 2 is empty                 */
   cccc(cadc1[0]);                        /*Reset all CAMAC devices for event */

   p1            = 0;
   curbuf        = BUFFER1;
   curmax        = MAXBUF1;
                        t0 = time(&now);
                        t1 = 0;
                        r1 = 0;
                        e1 = 0;

   if(onerun    == 0){
      par = swap1(*pSUNSTATUS);
      if(par == 1){
         printf( "SIRIUS is running: Push the STOP button for SIRIUS!!!\n");
         return;
      }
      printf( "Push the START button for SIRIUS!!!\n");

      while(par != 1){
         par = swap1(*pSUNSTATUS);
         if(leaveloop == 'y') return;            /*Jump out of event-loop     */
      }
      printf("\n Eventbuilder monitor (update every minute) \n");
      printf("-------------------------------------------   \n");
      printf("     Running    Events/s    Records/s         \n");
      t0 = time(&now);
      t1 = 0;
      r1 = 0;
      e1 = 0;
   }


   /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
   /*     Infinite main readout loop starts here       */
   /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
   for(;;){
      nextevent:
      /*:::::::::::::::::::::::::::::::::::::::::::::::*/
      /*           Buffer handling section             */
      /*:::::::::::::::::::::::::::::::::::::::::::::::*/
      if(p1 >= curmax-SAFE){                /*Not space enough, change buffer */         
         if(curbuf == BUFFER1){
          /*  *pSEMA_1     = swap(FULL); Set buffer 1 semaphore to FULL  */
            records      = records + 1;
            p1 = MAXBUF1+1;        /*Set pointer to first location of buffer 2*/
            curbuf = BUFFER2;
            curmax = MAXBUF2;
            if(onerun == 1)return; 
         }else{                                         /*Buffer 2 is current */
       /*     *pSEMA_2     = swap(FULL); Set buffer 2 semaphore to FULL  */
            records      = records + 1;
            p1 = 0;                /*Set pointer to first location of buffer 1*/
            curbuf = BUFFER1;
            curmax = MAXBUF1;
         }   
         eventmonitor();
      }
      header = p1;                 /*Start of event pointer is saved in header*/
      p1=p1+10;
      p1     = p1 + 1;             /*Update buffer pointer                    */
                                                       /*End of TPUS*/
      *(pbuf+header) = (p1-header)+EVENT_HEADER;
      events         = events + 1;
   }                                                     /*End of infinit loop*/
   return;
}                                                        /*End function loop  */


void Menu() {
   printf("\n");
   printf("      R : Run infinite event-loop          \r\n");
   printf("      O : One-buffer run                   \r\n");
   printf("      D : Dump data buffer (buffer one)    \r\n");
   printf("      S : Status                           \r\n");
   printf("      E : Enable/Disable TPUs              \r\n");
   printf("      M : Microsecond delay of events      \r\n");
   printf("      P : Parameters in SIRI/TPU locations \r\n");
   printf("      H : Help, listing of this menu       \r\n");
   printf(" Ctrl_C : Jump out of infinite event-loop  \r\n");
   printf("      * : Exit                             \r\n");
   printf("\n");
   return;
}


int readchar(){
   int c1;
   int c2 = '\n';
   while((c1 = getchar()) != '\n')
	c2 = tolower(c1);
   return c2;
}


void keyb_int(int sig_num){                     /* Keyboard interrupt routine */
   if (sig_num == SIGINT) {
      printf("\n\nLeaving infinite event-loop...\n");
      printf("Type r if you want to restart VME-acquisition\n");
      leaveloop = 'y';
   }
}


void Dumpbuf(){
   int nettowords;
   float nzro = 0.;
   float nabc = 0.;
   float npur = 0.;
   float nsir = 0.;
   float nxxx = 0.;
   float nozr = 0.;
   float pzro = 100.;
   int evnr = 0;
   int i1    = 500;
   int i2    = MAXBUF1 - 500;

   printf("Dump of 500 first words and 500 last words in buffer 1"); 

   for(i = 0; i < MAXBUF1 + 1; i++){
      par=*(pbuf+i);
      if(par==0) nzro = nzro + 1.;
      if(par==0x800a|par==0x800b|par==0x800c) nabc = nabc + 1.;
      if(par==0x800d) npur = npur + 1.;
      if(par==0x800e) nsir = nsir + 1.;
      if(par==0x800f) nxxx = nxxx + 1.;
      if((par&0xffffff00) == 0xf000)evnr = evnr + 1.;/*Masking out eventheader*/
      if(par != 0) nozr = nozr + 1.;
         
      if(par == 0x800a){if(i<i1 | i>i2)printf("\n");}
      if(par == 0x800b){if(i<i1 | i>i2)printf("\n");}
      if(par == 0x800c){if(i<i1 | i>i2)printf("\n");}
      if(par == 0x800d){if(i<i1 | i>i2)printf("\n");}
      if(par == 0x800e){if(i<i1 | i>i2)printf("\n");}
      if(par == 0x800f){if(i<i1 | i>i2)printf("\n");}           /*Up to 6 TPUs*/
      if((par&0xffffff00) == 0xf000){                           /*Eventheader */
      if(i<i1 | i>i2)printf("\n\n");
      if(i<i1 | i>i2)printf(" %x    (Event  %d    Word  %d)",par,evnr,i+1);
      }else{
   	   if(i<i1|i>i2)printf(" %x",par); 
      }
   }
   nozr = (nozr-evnr-2.*nabc-3.*npur-3.*nsir-2.*nxxx);        /*True energies */
   nzro = nzro-(float)SAFE; 
   if (nozr > 0){
      pzro = 100.*nzro/(nzro + nozr);
   }else{
      pzro = 100.;
   }
   nettowords = MAXBUF1 + 1 - SAFE;
   printf("\nScanning buffer 1 and making estimates:\n"); 
   printf("Netto words ( -200 zeros at end)   %6d \n", nettowords);
   printf("Eventheader words                  %6d \n", (int)evnr);
   printf("Patterns from TPU 1, TPU 2 or TPU3 %6d \n", (int)nabc);
   printf("Patterns from TPU 4 (pile-up)      %6d \n", (int)npur);
   printf("Patterns from TPU 5 (Siri)         %6d \n", (int)nsir);
   printf("Patterns from TPU 6 (not used)     %6d \n", (int)nxxx);
   printf("Data (energy, time...) equal 0     %6d \n", (int)nzro);
   printf("Data (energy, time...) not equal 0 %6d \n", (int)nozr);
   printf("Percent data with content 0 is %6.1f\n", pzro);
   printf("\nWaiting for pile-up                %4.1f us\n", uPUR); 
   printf("Conversion time for NIM ADCs       %4.1f us\n", uNIM); 
   printf("Conversion time for CAMAC ADCs     %4.1f us\n", uCAMAC); 
   printf("Conversion time for SIRI ADCs      %4.1f us\n", uSIRI); 
   printf("General delay before reading event %4.1f us\n", uEVENT);
   printf("\n"); 
   return;
}                                                       /*End function Dumpbuf*/
      

void Status(){ 
   int messlow, messhigh;
   int buf1low, buf1high;
   int buf2low, buf2high;

   messlow  = SLV24ADDR;  
   messhigh = SLV24ADDR + 4*(MESSNUM-1);
   buf1low  = vad;
   buf1high = vad + 4*MAXBUF1;
   buf2low  = vad + 4*(MAXBUF1+1);
   buf2high = vad + 4*MAXBUF2;

   printf("Memory locations for remote access: Sun -> Bit3 -> RTPC8067: \n");
   printf("Message box   (VME A24 slave): 0x%x  - 0x%x\n", messlow, messhigh);
   printf("Eventbuffer 1 (VME A32 slave): 0x%x - 0x%x\n", buf1low, buf1high);
   printf("Eventbuffer 2 (VME A32 slave): 0x%x - 0x%x\n", buf2low, buf2high);
   par=*(pmes+0);
   printf("Messagebox 0: Address VME  0x%x \n",swap(par));
   par=*(pmes+1);
   printf("Messagebox 1: Bufferlength 0x%x \n",swap(par));
   par=*(pmes+2);
   printf("Messagebox 2: Semaphore 1  0x%x \n",swap(par));
   par=*(pmes+3);
   printf("Messagebox 3: Semaphore 2  0x%x \n",swap(par));
   par=*(pmes+4);
   printf("Messagebox 4: Not used     0x%x \n",swap(par));
   par=*(pmes+5);
   printf("Messagebox 5: VME status   0x%x \n",swap(par));
   par=*(pmes+6);
   printf("Messagebox 6: SUN status   0x%x \n",swap(par));
   par=*(pmes+7);
   printf("Messagebox 7: Not used     0x%x \n",swap(par));
   par=*(pmes+8);
   printf("Messagebox 8: Not used     0x%x \n",swap(par));
   par=*(pmes+9);
   printf("Messagebox 9: Not used     0x%x \n",swap(par));

   printf("TPU1 is always enabled \n");
   if( tpu_2 == 'e' | tpu_2 == 'E' )
      printf("TPU2 is enabled \n");
   else
      printf("TPU2 is disabled \n");
   if( tpu_3 == 'e' | tpu_3 == 'E' )
      printf("TPU3 is enabled \n");
   else
      printf("TPU2 is disabled \n");

   printf("Waiting for pile-up                %4.1f us\n", uPUR); 
   printf("Conversion time for NIM ADCs       %4.1f us\n", uNIM); 
   printf("Conversion time for CAMAC ADCs     %4.1f us\n", uCAMAC); 
   printf("Conversion time for SIRI ADCs      %4.1f us\n", uSIRI); 
   printf("General delay before reading event %4.1f us\n", uEVENT);

   if(eventsold != 0){
      printf("VME data acquisition started at %s\n", sSTART );
      printf("VME data acquisition stopped at %s\n", sSTOP );
      printf("Records = %ld    Events = %ld \n", recordsold, eventsold);
   }
   printf("\n");
   return;
}                                                        /*End function Status*/
      

void Parameters(){ 
   u_short dts;
   u_char  dtc;
   printf("SIRI CSR registers:\n");

   dtc = *pSIRICOST;
   printf("SIRICOST    AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRICOST,AM32,dtc);
   dtc = *pSIRIEVEAD;
   printf("SIRIEVEAD   AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRIEVEAD,AM32,dtc); 
   dtc = *pSIRIBUFAD;
   printf("SIRIBUFAD   AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRIBUFAD,AM32,dtc); 

   printf("\n");
   printf("SIRI DSR, patterns and ADC values:\n");
   for(i = 0; i < 32; i++){
      SIRIadc[2*i] = *(pSIRIDSR+2*i); 
   printf("Pattern %2d, AD=0x%08x, AM=0x%02x: data read: %8x\n", i, SIRIDSR+2*(2*i), AM32, SIRIadc[2*i]); 
      SIRIadc[2*i+1] = *(pSIRIDSR+2*i+1); 
   printf("ADC  no %2d, AD=0x%08x, AM=0x%02x: data read: %8x\n", i, SIRIDSR+2*(2*i+1),AM32, SIRIadc[2*i+1]);
   }
 
   printf("\nTPU registers:\n");
   printf("TPU1:\n");
   dtc = *pNEXTREG;
   printf("NEXTREG     AD=0x%08x, AM=0x%02x: data read: %8x\n",NEXTREG,AM24,dtc);
   dts = *pT1STATUS;
   printf("T1STATUS    AD=0x%08x, AM=0x%02x: data read: %8x\n",T1STATUS,AM24,dts);
   dtc = *pT1NUMREG;
   printf("T1NUMREG    AD=0x%08x, AM=0x%02x: data read: %8x\n",T1NUMREG,AM24,dtc);
   dts = *pT1PATTERN;
   printf("T1PATTERN   AD=0x%08x, AM=0x%02x: data read: %8x\n",T1PATTERN,AM24,dts);

   printf("TPU2:\n");
   dts = *pT2STATUS;
   printf("T2STATUS    AD=0x%08x, AM=0x%02x: data read: %8x\n",T2STATUS,AM24,dts);
   dtc = *pT2NUMREG;
   printf("T2NUMREG    AD=0x%08x, AM=0x%02x: data read: %8x\n",T2NUMREG,AM24,dtc);
   dts = *pT2PATTERN;
   printf("T2PATTERN   AD=0x%08x, AM=0x%02x: data read: %8x\n",T2PATTERN,AM24,dts);

   printf("TPU3:\n");
   dts = *pT3STATUS;
   printf("T3STATUS    AD=0x%08x, AM=0x%02x: data read: %8x\n",T3STATUS,AM24,dts);
   dtc = *pT3NUMREG;
   printf("T3NUMREG    AD=0x%08x, AM=0x%02x: data read: %8x\n",T3NUMREG,AM24,dtc);
   dts = *pT3PATTERN;
   printf("T3PATTERN   AD=0x%08x, AM=0x%02x: data read: %8x\n",T3PATTERN,AM24,dts);

   printf("NIM registers:\n");
   dts = *pNIMCSR;
   printf("NIMCSR      AD=0x%08x, AM=0x%02x: data read: %8x\n",NIMCSR,AM24,dts);
   dts = *pNIMPATTERN;
   printf("NIMPATTERN  AD=0x%08x, AM=0x%02x: data read: %8x\n",NIMPATTERN, AM24,dts);

   printf("NIM ADCs:\n");
   for(i=0; i<16; i++){
       printf("ADC no %2d,  AD=0x%08x, AM=0x%02x: data read: %8x\n", i, NIMbaseaddr+2*i, AM24, *(pNIMadc+i)); 


   }
   printf("\n");
   return;
}                                                    /*End function Parameters*/

      
void DisEnAble(){
   printf("Enable(E) / Disable(D) TPU2 :");
   cmd = readchar();
   if( cmd == 'e') {
      tpu_2 = 'e';
      *pT2STATUS = ENABLE;
      printf("TPU2 enabled \n");
   }
   else if( cmd == 'd') {
      tpu_2 = 'd';
      *pT2STATUS = DISABLE;
      printf( "TPU2 disabled \n");
   }
   else
      printf("Illegal command \n");

   printf("Enable(E) / Disable(D) TPU3 :");
   cmd = readchar();
   if( cmd == 'e') {
      tpu_3 = 'e';
      *pT3STATUS = ENABLE;
      printf("TPU3 enabled \n");
   }
   else if(cmd == 'd') {
      tpu_3 = 'd';
      *pT3STATUS = DISABLE;
      printf( "TPU3 disabled \n");
   }
   else
      printf("Illegal command \n");
   return;   
}                                                     /*End function DisEnAble*/


void Microsecond(){

   printf("\n");
   printf("With this command you can change delay in front of the read-   \n");
   printf("out of various detectors. This is important for ADCs with long \n");
   printf("conversion times. (The delays for a specific detector will be  \n");
   printf("reduced according to the delays of previous ADCs)              \n");
   printf("Suggested values:                                              \n");
   printf("Pile-up rejection      :  8 us                                 \n");
   printf("NIM ADC Silena 7411    : 22 us at 2K, 44 us at 4K, 88 us at 8K \n");
   printf("NIM ADC Silena 7420    :  6 us at 2K, 12 us at 4K, 28 us at 8K \n");
   printf("CAMAC ADCs and TDCs    : 25 us\n");
   printf("SIRI telescopes        :  2 us x detectors fired = 8 us\n");
   printf("In front of event loop :  0 us\n");
   printf("\n");
   printf("Type your values (defaults by CR):\n");

   loopPUR   = (int)((uPUR   / usLOOP) + 0.5);
   loopNIM   = (int)((uNIM   / usLOOP) + 0.5);
   loopCAMAC = (int)((uCAMAC / usLOOP) + 0.5);
   loopSIRI  = (int)((uSIRI  / usLOOP) + 0.5);
   loopEVENT = (int)((uEVENT / usLOOP) + 0.5);

   uPUR   = (float)loopPUR   * usLOOP;
   uNIM   = (float)loopNIM   * usLOOP;
   uCAMAC = (float)loopCAMAC * usLOOP;
   uSIRI  = (float)loopSIRI  * usLOOP;
   uEVENT = (float)loopEVENT * usLOOP;
   
   printf("Waiting for pile-up                (%4.1f us) : ", uPUR); 
   gets(inbuf);
   sscanf(inbuf,"%f",&uPUR);
   printf("Conversion time for NIM ADCs       (%4.1f us) : ", uNIM); 
   gets(inbuf);
   sscanf(inbuf,"%f",&uNIM);
   printf("Conversion time for CAMAC ADCs     (%4.1f us) : ", uCAMAC); 
   gets(inbuf);
   sscanf(inbuf,"%f",&uCAMAC);
   printf("Conversion time for SIRI ADCs      (%4.1f us) : ", uSIRI); 
   gets(inbuf);
   sscanf(inbuf,"%f",&uSIRI);
   printf("General delay before reading event (%4.1f us) : ", uEVENT); 
   gets(inbuf);
   sscanf(inbuf,"%f",&uEVENT);

   loopPUR   = (int)((uPUR   / usLOOP) + 0.5);
   loopNIM   = (int)((uNIM   / usLOOP) + 0.5);
   loopCAMAC = (int)((uCAMAC / usLOOP) + 0.5);
   loopSIRI  = (int)((uSIRI  / usLOOP) + 0.5);
   loopEVENT = (int)((uEVENT / usLOOP) + 0.5);

   uPUR   = (float)loopPUR   * usLOOP;
   uNIM   = (float)loopNIM   * usLOOP;
   uCAMAC = (float)loopCAMAC * usLOOP;
   uSIRI  = (float)loopSIRI  * usLOOP;
   uEVENT = (float)loopEVENT * usLOOP;

   printf("\nWaiting for pile-up                %4.1f us", uPUR); 
   printf("\nConversion time for NIM ADCs       %4.1f us", uNIM); 
   printf("\nConversion time for CAMAC ADCs     %4.1f us", uCAMAC); 
   printf("\nConversion time for SIRI ADCs      %4.1f us", uSIRI); 
   printf("\nGeneral delay before reading event %4.1f us", uEVENT); 
   printf("\n(The values are multiples of %8.4f us)", usLOOP); 

   printf("\n");
   return;   
}                                                   /*End function Microsecond*/


void SLAVEinitiate(){
   #define PCI_CTL0 0xa0f50000
   #define PCI_CTL1 0xa0f50004
   #define PCI_CTL2 0xa0f50008
   #define PCI_CTL3 0xa0f5000c

   long adsl, dtsl, memsl, *psl;

   printf("Enable slave A24 memory (writting to PCI_CTL registers)...");

   adsl = PCI_CTL2;
   dtsl = 0x01400000;
   if (!(memsl=(int)smem_create("MEMsl",(char *)(adsl&~0xFFF), 0x4,SM_WRITE)))
   {
      fprintf(stderr,"Unable to allocate MEMsl window\n");
      exit(0);
   }
   psl = (long *)(memsl | (adsl&0xFFF));
   *psl = dtsl;
   /*printf("CTL2 at phys AD=%x,log AD=0x%08x:set to: %8x\n",adsl,psl,dtsl);*/
   smem_create("MEMsl",(char *)memsl,0x4,SM_DETACH);
   smem_remove("MEMsl");

   adsl = PCI_CTL3;
   dtsl = 0x5f000000;
   if (!(memsl=(int)smem_create("MEMsl",(char *)(adsl&~0xFFF), 0x4,SM_WRITE)))
   {
      fprintf(stderr,"Unable to allocate MEMsl window\n");
      exit(0);
   }
   psl = (long *)(memsl | (adsl&0xFFF));
   *psl = dtsl;
   /*printf("CTL3 at phys AD=%x,log AD=0x%08x:set to: %8x\n",adsl,psl,dtsl);*/
   smem_create("MEMsl",(char *)memsl,0x4,SM_DETACH);
   smem_remove("MEMsl");

   adsl = PCI_CTL2;
   dtsl = 0x00400000;
   if (!(memsl=(int)smem_create("MEMsl",(char *)(adsl&~0xFFF), 0x4,SM_WRITE)))
   {
      fprintf(stderr,"Unable to allocate MEMsl window\n");
      exit(0);
   }
   psl = (long *)(memsl | (adsl&0xFFF));
   *psl = dtsl;
  /*printf("CTL2 at phys AD=%x,log AD=0x%08x:set to: %8x\n",adsl,psl,dtsl);*/
   smem_create("MEMsl",(char *)memsl,0x4,SM_DETACH);
   smem_remove("MEMsl");

   adsl = PCI_CTL3;
   dtsl = 0x00800000;
   if (!(memsl=(int)smem_create("MEMsl",(char *)(adsl&~0xFFF), 0x4,SM_WRITE)))
   {
      fprintf(stderr,"Unable to allocate MEMsl window\n");
      exit(0);
   }
   psl = (long *)(memsl | (adsl&0xFFF));
   *psl = dtsl;
   /*printf("CTL3 at phys AD=%x,log AD=0x%08x: et to: %8x\n",adsl,psl,dtsl);*/
   smem_create("MEMsl",(char *)memsl,0x4,SM_DETACH);
   smem_remove("MEMsl");

   printf("DONE \n");
   return;
}


void BUFFERopen(){        /*Allocation of physically contiguous memory        */
                          /*mapped for static VME slave access                */
                          /*The 2 eventbuffers are stored in A32 slave memory */
                          /*of 2*32 kword = 262144 bytes = 0x40000 bytes      */
   bufferbytes=0x40000;
   if(err=uio_open()) {
	uio_perror("uio_open",err);
	exit(0);
   }
   
   /*
   * prepare page descriptor options
   */
   sp.rdpref = 0;			/* read prefetch (before = 3, magne)*/
   sp.wrpost = 0;			/* write posting (before = 1, magne)*/
   sp.wrprotect = 0;		/* enable writing */
   sp.swap = SINGLE_AUTO_SWAP; /* auto-swapping */
   sp.pcispace = PCI_MEM_CES;	/* PCI memory space */

   printf("Allocating DRAM A32 slave memory (buffers) of 0x%x bytes...", bufferbytes);
   if (err=uio_calloc(&cmem_dsc, bufferbytes)) {
	uio_perror("uio_calloc",err);
	exit(0);
   }else{
	/*printf("Kernel virtual address:          0x%08x\n",cmem_dsc.kaddr); */
	/*printf("User virtual address:            0x%08x\n",cmem_dsc.uaddr); */
	/*printf("Physical address:                0x%08x\n",cmem_dsc.paddr); */
	/*printf("Total A32 buffersize (bytes):    0x%08x\n",cmem_dsc.size);  */
   }
   ad  = cmem_dsc.paddr;
   vad = (long)ad + 0x08000000;
   /*printf("VME address of second CPU (Sun): 0x%08x\n",vad); */

   /*
   * map to VME
   */
   vadr = vme_slave_map(PHYS_2_PCI(ad), bufferbytes,&sp);
   if (vadr == -1) {
      fprintf(stderr,"mapping to VME failed -terminating\n");
      uio_cfree(&cmem_dsc);
      exit(0);
   }else{
      printf("Mapped 0x%x (%d) bytes at PCI 0x%08x to VME 0x%08x\n",
 bufferbytes, bufferbytes,PHYS_2_PCI(ad),vadr);
   }

   pbuf = (u_long *)cmem_dsc.uaddr;
  /* printf("Pointer in program:              0x%08x...DONE\n",pbuf); */
   printf("DONE \n");
   return;
}


void BUFFERclear(){
   printf("Zeroing eventbuffer 1 and 2...");
   for( i = 0; i < MAXBUF2+1; i++){                    /*Clear data buffers*/
      *(pbuf+i) = 0;
   }  
   p1         = 0;
   *pSEMA_1   = swap(EMPTY);         /*Set semaphore for buffer to empty state*/
   *pSEMA_2   = swap(EMPTY); 
   curbuf     = BUFFER1;             /*Start with buffer one                  */
   curmax     = MAXBUF1;             /*Initialise the roof of buffer one      */
   printf("DONE \n");
   return;
}


void BUFFERclose(){
   printf("Closing A32 slave memory...");
   if (err=uio_cfree(&cmem_dsc)) {
	uio_perror("uio_cfree",err);
	exit(0);
   }

   err=vme_slave_unmap(vadr, bufferbytes);
   if (err == -1) {
      fprintf(stderr,"unmapping from VME failed\n");
     /* exit(0)*/;
   }
   uio_close();
   printf("DONE \n");
   return;
}


void MESSAGEBOXopen(){
   messagebytes = 0x28;
   printf("Allocating SRAM A24 slave memory (message box) of 0x%x bytes...", messagebytes);
   if (!(mem24=(int)smem_create("MEM24",(char *)(CPUADDR&~0xFFF), messagebytes,
      SM_WRITE | SM_READ))){
      fprintf(stderr,"Unable to allocate MEM window for slave A24 memory");
      exit(0);
   }
   pmes = (long *)(mem24 | (CPUADDR&0xFFF));         /*First 10 message-words*/
   pBUFFER_ADDRESS = pmes + 0;
   pBUFFER_LENGTH  = pmes + 1;
   pSEMA_1         = pmes + 2;
   pSEMA_2         = pmes + 3;
   pVMESTATUS      = pmes + 5;
   pSUNSTATUS      = pmes + 6;
   printf("DONE \n");
   return;
}


void MESSAGEBOXconfig(){
   printf("Writing status, buffer address and length to SRAM...");
   *(pmes+4) = 0;
   *(pmes+7) = 0;                          /*Zeroing unused locations         */
   *(pmes+8) = 0;
   *(pmes+9) = 0;
   *pBUFFER_ADDRESS = swap(vad);           /*Start address of databuffer      */
   *pBUFFER_LENGTH  = swap(MAXBUF1+1);     /*Store buffer length              */
   *pVMESTATUS      = swap(STOP);          /*Eventbuiler is not yet running   */
   *pSUNSTATUS      = swap(STOP);          /*Sirius has to be restarted to run*/
   printf("DONE \n");
   return;
}


void MESSAGEBOXclose(){
   *pVMESTATUS      = swap(STOP);
   printf("Closing A24 slave memory...");
   smem_create("MEM24",(char *)mem24, messagebytes,SM_DETACH);
   smem_remove("MEM24");
   printf("DONE \n");
   return;
}


void CAMACopen(){
   printf("Opening CAMAC, ");
   copen();
   printf("...DONE \n");
   return;
}      
  

void CAMACclose(){
   printf("Closing CAMAC...");
   cclose();
   printf("DONE \n");
   return;
}      


void CAMACconfig(){                      /*Subroutine to configure the setup  */
   u_short statw = 0xA00;         /*LAM disable,OVF disable,SUB disable*/
   u_short puren = 0;
   u_short dummy;   
   int astat1,astat2,astat3,astat4;
   int tstat1,tstat2,tstat3,tstat4;
   int pcont1;
   printf("Configuring CAMAC registers...");

                       /* Define the the symbolic addresses of CAMAC devices: */
   for ( i = 0; i < 8; i++)                                 /* Camac ADC1     */
      cdreg(&cadc1[i], BRANCH, CRATE, ADC1SLOT, i);         /* Data reg. 0-7  */
   cdreg(&astat1, BRANCH, CRATE, ADC1SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac ADC2     */
      cdreg(&cadc2[i], BRANCH, CRATE, ADC2SLOT, i);         /* Data reg. 0-7  */
   cdreg(&astat2, BRANCH, CRATE, ADC2SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac ADC3     */
      cdreg(&cadc3[i], BRANCH, CRATE, ADC3SLOT, i);         /* Data reg. 0-7  */
   cdreg(&astat3, BRANCH, CRATE, ADC3SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac ADC4     */
      cdreg(&cadc4[i], BRANCH, CRATE, ADC4SLOT, i);         /* Data reg. 0-7  */
   cdreg(&astat4, BRANCH, CRATE, ADC4SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac TDC1     */
      cdreg(&ctdc1[i], BRANCH, CRATE, TDC1SLOT, i);         /* Data reg. 0-7  */
   cdreg(&tstat1, BRANCH, CRATE, TDC1SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac TDC2     */
      cdreg(&ctdc2[i], BRANCH, CRATE, TDC2SLOT, i);         /* Data reg. 0-7  */
   cdreg(&tstat2, BRANCH, CRATE, TDC2SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac TDC3     */
      cdreg(&ctdc3[i], BRANCH, CRATE, TDC3SLOT, i);         /* Data reg. 0-7  */
   cdreg(&tstat3, BRANCH, CRATE, TDC3SLOT, 14);             /* Status register*/
   
   for ( i = 0; i < 8; i++)                                 /* Camac TDC4     */
      cdreg(&ctdc4[i], BRANCH, CRATE, TDC4SLOT, i);         /* Data reg. 0-7  */
   cdreg(&tstat4, BRANCH, CRATE, TDC4SLOT, 14);             /* Status register*/

   for ( i= 0; i< 4; i++)                                   /* Camac PUR1     */
      cdreg(&cpur1[i],BRANCH,CRATE,PUR1SLOT,i);             /* Data reg. 0-3  */
   cdreg(&pcont1,BRANCH,CRATE,PUR1SLOT,0);                  /* Controlregister*/
                                                    /*Enable CAMAC ADC/TDC    */
   cccz(cadc1[0]);                                  /*Reset crate             */
   ccci(cadc1[0], 0);                               /*Clear crate inhibit flag*/
   
   f = 20;                                          /*Write to status register*/
   cssa(f, astat1, &statw, &q);
   if(q == 0) printf("Missing Q-response from ADC1\n");
   cssa(f, astat2, &statw, &q);
   if(q == 0) printf("Missing Q-response from ADC2\n");
   cssa(f, astat3, &statw, &q);
   if(q == 0) printf("Missing Q-response from ADC3\n");
   cssa(f, astat4, &statw, &q);
   if(q == 0) printf("Missing Q-response from ADC4\n");
   cssa(f, tstat1, &statw, &q);
   if(q == 0) printf("Missing Q-response from TDC1\n");
   cssa(f, tstat2, &statw, &q);
   if(q == 0) printf("Missing Q-response from TDC2\n");
   cssa(f, tstat3, &statw, &q);
   if(q == 0) printf("Missing Q-response from TDC3\n");
   cssa(f, tstat4, &statw, &q);
   if(q == 0) printf("Missing Q-response from TDC4\n");
                                                    /*Enable PUR              */
   cssa(16, pcont1, &puren, &q);
   cssa( 0, pcont1, &dummy, &q);
   if(q == 0) printf("Missing Q-response from PUR1\n");  
   printf("DONE \n");
   return;   
}                                                   /*End function CAMACconfig*/


void SIRIopen(){
   /************************************************/
   /* Set-up of address pointers for CSR and DSR   */
   /************************************************/
   u_short dts;
   u_char  dtc;
   printf("Opening SIRI VME interface...");

    /*Pointers to CSR register (8 kb x 16 bits)       */
    /*Only a few locations of CSR are used            */
    /*8 words, each 8 bits long, starts at 0xf0f0'4000*/

   pSIRICLR = (u_char *)vme_map(SIRICLR,sizeof(u_char),AM32);
   if (pSIRICLR == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRICLR register\n");
      exit(0);
   }

   pSIRICOST = (u_char *)vme_map(SIRICOST,sizeof(u_char),AM32);
   if (pSIRICOST == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRICOST register\n");
      exit(0);
   }
   dtc = *pSIRICOST;
/*   printf("SIRICOST   AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRICOST,AM32,dtc); */

   pSIRIEVEAD = (u_char *)vme_map(SIRIEVEAD,sizeof(u_char),AM32);
   if (pSIRIEVEAD == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRIEVEAD register\n");
      exit(0);
   }
   dtc = *pSIRIEVEAD;
/*   printf("SIRIEVEAD  AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRIEVEAD,AM32, dtc); */

   pSIRIBUFAD = (u_char *)vme_map(SIRIBUFAD,sizeof(u_char),AM32);
   if (pSIRIBUFAD == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRIBUFAD register\n");
      exit(0);
   }
   dtc = *pSIRIBUFAD;
/*   printf("SIRIBUFAD  AD=0x%08x, AM=0x%02x: data read: %8x\n",SIRIBUFAD,AM32, dtc); */

   pSIRIEVRDY = (u_char *)vme_map(SIRIEVRDY,sizeof(u_char),AM32);
   if (pSIRIEVRDY == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRIEVRDY register\n");
      exit(0);
   }

   pSIRIIRQVC = (u_char *)vme_map(SIRIIRQVC,sizeof(u_char),AM32);
   if (pSIRIIRQVC == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRIIRQVC register\n");
      exit(0);
   }

   pSIRINXBUF = (u_char *)vme_map(SIRINXBUF,sizeof(u_char),AM32);
   if (pSIRINXBUF == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRINXBUF register\n");
      exit(0);
   }

   pSIRINUBUF = (u_char *)vme_map(SIRINUBUF,sizeof(u_char),AM32);
   if (pSIRINUBUF == (u_char *)0){
      fprintf(stderr,"Unable to map VME address for SIRINUBUF register\n");
      exit(0);
   }

/*Pointers to DSR register (8 kb x 16 bits)               */
/*Only first buffer of interrest (no ring buffers assumed)*/
/*256 words, each 16 bit long, starts at 0xf0f0'0000      */

/*   printf("\n");
   printf("SIRI patterns and ADCs:\n");*/
   pSIRIDSR = (u_short *)vme_map(SIRIDSR,64*sizeof(u_short),AM32);
   if (pSIRIDSR == (u_short *)0){
      fprintf(stderr,"Unable to map VME address for SIRIs DSR\n");
      exit(0);
   }
   for(i = 0; i < 32; i++){
      SIRIadc[2*i] = *(pSIRIDSR+2*i); 
/*      printf("Pattern %2d, AD=0x%08x, AM=0x%02x: data read: %8x\n", i, SIRIDSR+2*(2*i), AM32, SIRIadc[2*i]); */
      SIRIadc[2*i+1] = *(pSIRIDSR+2*i+1); 
/*      printf("ADC  no %2d, AD=0x%08x, AM=0x%02x: data read: %8x\n", i, SIRIDSR+2*(2*i+1),AM32, SIRIadc[2*i+1]); */
   }
   printf("DONE \n");
   return;
}                                                      /*End function SIRIopen*/


void SIRIconfig(){              /*One ring buffer - without interrupt handling*/
   printf("Configuring and initialisation of SIRI VME interface...");
   *pSIRINXBUF = CLEAR;         /*Next buffer to be read by VME               */
   *pSIRINUBUF = NUMBUF;        /*Number of buffers in ring buffer (1 - 32)   */
   *pSIRIEVRDY = CLEAR;         /*Event has been read by ROCO                 */
   *pSIRIEVEAD = CLEAR;         /*Address of SIRI event                       */
   *pSIRIBUFAD = CLEAR;         /*Address of SIRI buffer                      */
   *pSIRICOST  = ENABLE;        /*Main control and status register COST       */
   for(i = 0; i < 64; i++){
      *(pSIRIDSR+i) = 0;
      dE[i]         = 0;
      E[i]          = 0;
   }
   printf("DONE \n");
   return;
}


void SIRIclose (){
   printf("Closing SIRI...");
   *pSIRICOST = DISABLE;
   vme_rel(pSIRICLR,     sizeof(u_char));
   vme_rel(pSIRICOST,    sizeof(u_char));
   vme_rel(pSIRIEVEAD,   sizeof(u_char));
   vme_rel(pSIRIBUFAD,   sizeof(u_char));
   vme_rel(pSIRIEVRDY,   sizeof(u_char));
   vme_rel(pSIRIIRQVC,   sizeof(u_char));
   vme_rel(pSIRINXBUF,   sizeof(u_char));
   vme_rel(pSIRINUBUF,   sizeof(u_char));
   vme_rel(pSIRIDSR, 64*sizeof(u_short));
   printf("DONE \n");
   return;
}


void TPUopen(){
   u_short dts;
   u_char dtc;

   printf("Opening TPUs...");
 /*printf("\n TPU1:\n");*/
   pNEXTREG = (u_char*)vme_map(NEXTREG,sizeof(u_char),AM24);
   if (pNEXTREG == (u_char*)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dtc = *pNEXTREG;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",NEXTREG,AM24,dtc); */
  
   pT1STATUS = (u_short *)vme_map(T1STATUS,sizeof(u_short),AM24);
   if (pT1STATUS == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT1STATUS;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T1STATUS,AM24,dts); */

   pT1NUMREG = (u_char *)vme_map(T1NUMREG,sizeof(u_char),AM24);
   if (pT1NUMREG == (u_char *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dtc = *pT1NUMREG;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T1NUMREG,AM24,dtc); */

   pT1PATTERN = (u_short *)vme_map(T1PATTERN,sizeof(u_short),AM24);
   if (pT1PATTERN == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT1PATTERN;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T1PATTERN,AM24,dts); */

 /*printf("TPU2:\n");*/
   pT2STATUS = (u_short *)vme_map(T2STATUS,sizeof(u_short),AM24);
   if (pT2STATUS == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT2STATUS;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T2STATUS,AM24,dts); */

   pT2NUMREG = (u_char *)vme_map(T2NUMREG,sizeof(u_char),AM24);
   if (pT2NUMREG == (u_char *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dtc = *pT2NUMREG;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T2NUMREG,AM24,dtc); */

   pT2PATTERN = (u_short *)vme_map(T2PATTERN,sizeof(u_short),AM24);
   if (pT2PATTERN == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT2PATTERN;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T2PATTERN,AM24,dts); */

 /*printf("TPU3:\n");*/
   pT3STATUS = (u_short *)vme_map(T3STATUS,sizeof(u_short),AM24);
   if (pT3STATUS == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT3STATUS;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T3STATUS,AM24,dts); */

   pT3NUMREG = (u_char *)vme_map(T3NUMREG,sizeof(u_char),AM24);
   if (pT3NUMREG == (u_char *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dtc = *pT3NUMREG;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T3NUMREG,AM24,dtc); */

   pT3PATTERN = (u_short *)vme_map(T3PATTERN,sizeof(u_short),AM24);
   if (pT3PATTERN == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pT3PATTERN;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",T3PATTERN,AM24,dts); */

 /*printf("NIM REGISTER:\n");*/
   pNIMCSR = (u_short *)vme_map(NIMCSR,sizeof(u_short),AM24);
   if (pNIMCSR == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pNIMCSR;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",NIMCSR,AM24,dts); */

   pNIMPATTERN = (u_short *)vme_map(NIMPATTERN,sizeof(u_short),AM24);
   if (pNIMPATTERN == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   dts = *pNIMPATTERN;
 /*printf("AD=0x%08x, AM=0x%02x: data read: %8x\n",NIMPATTERN,AM24,dts); */

 /*printf("NIM ADCs:\n");*/
   pNIMadc = (u_short *)vme_map(NIMbaseaddr,16*sizeof(u_short),AM24);
   if (pNIMadc == (u_short *)0){
      fprintf(stderr,"Unable to map VME address\n");
      exit(0);
   }
   for(i=0; i<16; i++){
    /*printf("ADC no %2d, AD=0x%08x, AM=0x%02x: data read: %8x\n", i, NIMbaseaddr+2*i, AM24, *(pNIMadc+i)); */
   }
   printf("DONE \n");
   return;
}                                                       /*End function TPUopen*/


void TPUconfig(){                             /*Enable NIM Controller as slave*/
    printf("Configuring and initialising TPUs...");
   *pNIMCSR     = CLEAR;                      /*Reset CSR                     */
   *pNIMPATTERN = CLEAR;                      /*Reset Pattern & ADC`s         */
   *pNIMCSR     = ENABLE;                     /*Enable as Slave               */
   *pT1STATUS   = CLEAR;                      /*Reset TPU before running      */
   *pT2STATUS   = ENABLE;                     /*Enable TPU2                   */
   *pT3STATUS   = ENABLE;                     /*Enable TPU3                   */
   tpu_2        = 'e';
   tpu_3        = 'e';
   printf("DONE \n");
   return;
}


void TPUclose(){
   printf("Closing TPUs...");

   vme_rel(pNEXTREG,sizeof(u_char));
   vme_rel(pT1STATUS,sizeof(u_short));
   vme_rel(pT1NUMREG,sizeof(u_char));
   vme_rel(pT1PATTERN,sizeof(u_short));

   vme_rel(pT2STATUS,sizeof(u_short));
   vme_rel(pT2NUMREG,sizeof(u_char));
   vme_rel(pT2PATTERN,sizeof(u_short));

   vme_rel(pT3STATUS,sizeof(u_short));
   vme_rel(pT3NUMREG,sizeof(u_char));
   vme_rel(pT3PATTERN,sizeof(u_short));

   vme_rel(pNIMCSR,sizeof(u_short));
   vme_rel(pNIMPATTERN,sizeof(u_short));
   vme_rel(pNIMadc,16*sizeof(u_short));

   printf("DONE \n");
   return;
}


void eventmonitor(){
   int th, tm;
   float rs, es;
   t2 = time(&now) - t0;
   tx = t2 - t1;
   if(tx >= 60){
      r2 = records;
      e2 = events;
      rx = r2 - r1;
      ex = e2 - e1;
      rs = (float)rx/(float)tx;
      es = (float)ex/(float)tx;
      th = t2/3600;
      tm = ((((float)t2-th*3600.)/60.) + 0.5); 
      printf("%7dh %2dm    %8.2f    %7.2f \n",th,tm,es,rs);
      r1 = r2;
      e1 = e2;
      t1 = t2;
   }
   return;
}
