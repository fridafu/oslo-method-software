#define  MESSAGE_ADDRESS 	0x201FFFD0 		/* VMEbus Message_Box address  */
#define  SEM_1		 	0x201FFFD8   		/* Databuffer # 1 semaphore */
#define  SEM_2		 	0x201FFFDC		/* Databuffer # 2 semaphore */
#define  VMESTATUS	 	0x201FFFE4		/* Running status for VME system */
#define  ENGSTATUS	 	0x201FFFE8		/* Running status for acq_engine task */
#define  BUFFER_LENGTH   	0x20000			/* Data buffer length 32 kW = 128 kBytes*/
#define  MESSAGE_LENGTH		0x50			/* Message Box length 10 W = 40 bytes �*/
#define  RECORD_LENGTH	 	0x8000			/* Exabyte record length = 32 kBytes */
#define  REC_IN_BUF  		4			/* 4 records on exabyte per databuffer */
#define  NO_OF_TELESCOPES	8			/* Number of particle telescopes 8/64  */
