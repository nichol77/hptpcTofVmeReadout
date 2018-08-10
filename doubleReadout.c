/////////////////////////////////////////////////////////////////////////////
// Name:        doubleReadout.c
// Purpose:     
// Licence:     Based on CAEN V1190 Demo
/////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////
// File includes
////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#if defined (LINUX)
#include <memory.h>
#include <ctype.h>
#endif
#include "common_defs.h"
#include "cvt_board_commons.h"
#include "cvt_common_defs.h"
#include "cvt_V1190.h"


#include "configLib.h"
#include "keyValuePair.h"


////////////////////////////////////////////
// File local variables declaration
////////////////////////////////////////////

////////////////////////////////////////////
// Global visible variables declaration
////////////////////////////////////////////

////////////////////////////////////////////
// File local methods declaration
////////////////////////////////////////////

#define MAX_CH_ENABLE_WORD 8
#define MAX_V1190_TDC_COUNT 2

FILE*		outputFile1;
FILE*		outputFile2;
int runActive;

static void timerHandler(int signal)
{
  runActive=FALSE;
}


int openOutputs(char*outputPath, int runNumber)
{
  char dname[FILENAME_MAX] ;
  char fname[FILENAME_MAX] ;
  int status ;
  time_t now;
  time(&now);
  status = 0 ;
  struct stat st = {0};
  
  // get rid of the ".dat" of the filename
  snprintf (dname, sizeof(dname), "%s/run%02d", outputPath, runNumber);
  if (stat(dname, &st) == -1) {
    mkdir(dname, 0700);
  }
  
  
  snprintf (fname, sizeof(fname), "%s/raw_1_%lu",dname,now) ;
  outputFile1 = fopen (fname, "w") ;
  if (outputFile1 == NULL)
    {
      perror ("openOutput") ;
      status = 1 ;
    }
  snprintf (fname, sizeof(fname), "%s/raw_2_%lu", dname,now) ;
  outputFile2 = fopen (fname, "w") ;
  if (outputFile2 == NULL)
    {
      perror ("openOutputs") ;
      status = 1 ;
    }
  return(status);
}

void sigUsr2Handler(int sig)
{
  fprintf(stderr,"Caught signal %d\n",sig);
  runActive=FALSE;
} 


void trapSignal(int signal, void (*handler)(int))
{
  struct sigaction action ;
   
  /* Register signal handler */
  action.sa_handler = handler ;
  sigemptyset (&action.sa_mask) ;
  action.sa_flags = 0 ;
  sigaction (signal, &action, NULL) ;
}
 
void setupTimer(int seconds)
{
  struct itimerval interval;
   
  /* Setup the interval timer */
  interval.it_interval.tv_sec=0;
  interval.it_interval.tv_usec=0;
   
  interval.it_value.tv_sec=seconds;
  interval.it_value.tv_usec=0;
   
  setitimer(ITIMER_REAL, &interval, 0);
}

void sleeper(int milliseconds)
{
  struct timespec req = {0};
  int noSec = milliseconds / 1000;
  req.tv_sec = noSec * 1L;
  req.tv_nsec = (milliseconds - noSec * 1000) * 1000000L;
  nanosleep(&req, (struct timespec*)NULL);
}

/**************************************************
 **************************************************/

////////////////////////////////////////////////////////////////////////////////////////////////
/*! \fn      int main(int argc, char **argv) 
 *   \brief   CAENVMETool demo usage for the V1190 board
 *            
 *            Setups a V1190 board, reads some events and stores into output file
 *   \param   argc number of command line arguments
 *   \param   *argv[] command line arguments' list 
 *   \return  = 0 procedure completed correctly: < 0 some error occurred
 */
////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) 

{
  int ret_val= 0;									// procedure exit value
  cvt_V1190_data board_data;						// board data
  cvt_V1190_data board_data2;						// board data
  //  FILE* raw_out_file= NULL;						// raw output file
  //  FILE* raw_out_file2= NULL;						// raw output file
  UINT8 *data_buff= NULL;							// read data buffer
  UINT8 *data_buff2= NULL;							// read data buffer
  UINT32 data_size;
  UINT32 fakeTime;      
  UINT32 data_size2;
  int32_t vme_handle= -1;							// The CAENVMELib handle
  //  long read_events= 0;
  const int DATA_BUFF_SIZE= 1024*1024;			// The data buffer size
  const int NUM_TDCS_PER_V1290=2;

  UINT16 board1_base_address=0xee00;
  UINT16 board2_base_address=0x1111;


  CVT_V1190_EDGE_DETECTION_ENUM m_edge_detection=2;		/*!< The edge detection type. */
  CVT_V1190_PAIR_RES_WIDTH_ENUM m_res_width=0;			/*!< The resolution width. */
  UINT16 m_enable_msk[ MAX_CH_ENABLE_WORD];			/*!< The channel enable pattern buffer. */
  {
    int i=0;
    for(i=0;i<MAX_CH_ENABLE_WORD;i++) {
      m_enable_msk[i]=0xffff;
    }
  }


  
  //  time_t startTime;
  time_t now;
  time_t lastTime1=0;
  time_t lastTime2=0;  
  int runTime;
  int maxEvents;
  int runNumber;
  char* outputPath;
  FILE* rnFile;

	
  /////////////////////////////////////////
  // Demo application specific
  /////////////////////////////////////////

  memset( &board_data, 0, sizeof( board_data));
  memset( &board_data2, 0, sizeof( board_data2));

  signal(SIGTERM, sigUsr2Handler);
  signal(SIGINT, sigUsr2Handler);




	
  ///////////////////////////////////////////////////
  // Auto-increment runNumber
  ///////////////////////////////////////////////////
  configLoad("runNumber.config","run");
  runNumber=kvpGetInt("runNumber", 0);
  runNumber++;
  rnFile=fopen("runNumber.config", "w");
  fprintf(rnFile,"<run>\nrunNumber#S=%d;\n</run>\n", runNumber);
  fclose(rnFile);


  ///////////////////////////////////////////////////
  //  open a file to write the current event number
  //  to so you can check remotely how much data
  //  has been taken so far.
  ///////////////////////////////////////////////////
  FILE * statFile;
  statFile = fopen ("Status.txt","w");
  fprintf(statFile,"%d\n",runNumber);
  fclose(statFile);

  
	
  ///////////////////////////////////////////////////
  // Get configuration from file into kvp buffer
  ///////////////////////////////////////////////////
  configLoad("hptpctof.config","global");


	
  // -----------------------------------------------------------------------------
  // Open output file if requested
  // -----------------------------------------------------------------------------
  printf("Starting run %d\n",runNumber);
  
  runTime=kvpGetInt("runTime", 0);
  printf(" Runtime is %d\n", runTime);
  
  maxEvents=kvpGetInt("maxEvents",0);
  printf(" Max events is %d\n", maxEvents);
  
  outputPath=kvpGetString("outputPath");
  if (outputPath != NULL)
    {
      openOutputs(outputPath, runNumber);
    }
  else
    {
      openOutputs(".",runNumber);
    }
  	
  // Vme handle initialization
  if( !CAENVME_Init( cvV1718, 0, 0, &vme_handle)== cvSuccess)
    {
      fprintf(stderr,"VME INIT ERROR -- Can't find v1718\n");
      exit(0);
    }


  /////////////////////////////////////////
  // Library specific
  /////////////////////////////////////////

  //
  // init V1190 board data
  printf(  " Initializing V1190 board data ... ");
  if( !cvt_V1190_open( &board_data, board1_base_address, vme_handle, CVT_V1290_TYPE_N))
    {	
      printf( "\nError executing cvt_V1190_open \n");
      ret_val= -4;
      goto exit_point;
    }
  printf(  " Ok \n");
  printf(  " Initializing V1190 board data ... "); 
  if( !cvt_V1190_open( &board_data2, board2_base_address, vme_handle, CVT_V1290_TYPE_N)) 
    {	 
      printf( "\nError executing cvt_V1190_open \n"); 
      ret_val= -4; 
      goto exit_point; 
    }
  printf(  " Ok \n"); 
	
  //
  // Get system informations
  {
    UINT32 tdc_id_buff[ MAX_V1190_TDC_COUNT];
    UINT16 firmware_rev;
    UINT16 micro_firmware_rev;
    UINT16 serial_number;
    int i; 

    printf(  " Getting system informations ... ");
    if( !cvt_V1190_get_system_info( &board_data, &firmware_rev, tdc_id_buff, &micro_firmware_rev, &serial_number))
      {
	printf( "\nError executing cvt_V1190_get_system_info \n");
	ret_val= -5;
	goto exit_point;
      }
    printf(  " Ok \n\n");

    // Show system infos
    printf( "   Firmware Rev.       : %04x\n", firmware_rev);
    printf( "   Micro Firmware Rev. : %04x\n", micro_firmware_rev);
    printf( "   Serial Number       : %04x\n", serial_number);
    for( i= 0; i< NUM_TDCS_PER_V1290; i++)
      {
	printf( "   TDC %d               : %08x\n", i, tdc_id_buff[ i]);
      }


    printf(  " Getting system informations ... ");
    if( !cvt_V1190_get_system_info( &board_data2, &firmware_rev, tdc_id_buff, &micro_firmware_rev, &serial_number))
      {
	printf( "\nError executing cvt_V1190_get_system_info \n");
	ret_val= -5;
	goto exit_point;
      }
    printf(  " Ok \n\n");

    // Show system infos
    printf( "   Firmware Rev.       : %04x\n", firmware_rev);
    printf( "   Micro Firmware Rev. : %04x\n", micro_firmware_rev);
    printf( "   Serial Number       : %04x\n", serial_number);
    for( i= 0; i< NUM_TDCS_PER_V1290; i++)
      {
	printf( "   TDC %d               : %08x\n", i, tdc_id_buff[ i]);
      }
  }
  //
  // data clear
  printf(  " Sending data clear ... ");
  if( !cvt_V1190_data_clear( &board_data))	
    {	
      printf( "\nError executing cvt_V1190_data_clear \n");
      ret_val= -5;
      goto exit_point;
    }
  printf(  " Ok \n");

  // data clear
  printf(  " Sending data clear ... ");
  if( !cvt_V1190_data_clear( &board_data2))	
    {	
      printf( "\nError executing cvt_V1190_data_clear \n");
      ret_val= -5;
      goto exit_point;
    }
  printf(  " Ok \n");

  //
  // Acquisition mode
  printf(  " Setting acquisition mode ... ");
  printf("MAX_CH_ENABLE_WORD: %d\n",MAX_CH_ENABLE_WORD);
	
	
	
  if( !cvt_V1190_set_continuous_acquisition_mode( &board_data, 
						  m_edge_detection,
						  m_res_width,
						  &m_enable_msk[0]))
    {	
      printf( "Error executing cvt_V1190_set_continuous_acquisition_mode \n");
      ret_val= -5;
      goto exit_point;
    }
	
  if( !cvt_V1190_set_continuous_acquisition_mode( &board_data2, 
						  m_edge_detection,
						  m_res_width,
						  &m_enable_msk[0]))
    {	
      printf( "Error executing cvt_V1190_set_continuous_acquisition_mode \n");
      ret_val= -5;
      goto exit_point;
    }
  printf(  " Ok\n");

  //
  // readout mode
  printf(  " Setting readout mode ...");
  if( !cvt_V1190_set_readout_mode( &board_data, TRUE, TRUE, 1))
    {	
      printf( "Error executing cvt_V1190_set_readout_mode \n");
      ret_val= -5;
      goto exit_point;
    }
  printf(  " Ok\n");
  printf(  " Setting readout mode ...");
  if( !cvt_V1190_set_readout_mode( &board_data2, TRUE, TRUE, 1))
    {	
      printf( "Error executing cvt_V1190_set_readout_mode \n");
      ret_val= -5;
      goto exit_point;
    }
  printf(  " Ok\n");

  // Allocate buffer storage
  data_buff= malloc( DATA_BUFF_SIZE);
  data_buff2= malloc( DATA_BUFF_SIZE);
  if( data_buff== NULL)
    {
      // Insufficient memory
      printf( "Error allocating events' buffer (%i bytes)", DATA_BUFF_SIZE);
      ret_val= -5;
      goto exit_point;
    }
  if( data_buff2== NULL)
    {
      // Insufficient memory
      printf( "Error allocating events' buffer (%i bytes)", DATA_BUFF_SIZE);
      ret_val= -5;
      goto exit_point;
    }


  //
  // Start acquisition 
  printf(  " Getting events: ctrl-c to end run ...\n");
  printf(  "\n");

  runActive=TRUE;

  time(&now);
  //  printf("Size of time_t is %lu bytes.\n", sizeof(time_t));
  //  printf("Time in hex -- %#x\n",now-1000000000);


  if (runTime != 0)
    {
      printf("Setting run length to %d seconds\n",runTime);
      trapSignal(SIGALRM, timerHandler);
      setupTimer(runTime);
    }
  trapSignal(SIGINT, timerHandler);
  
  
  while(runActive)
    {
      static long last_read_bytes= 0;
      static long last_read_bytes2= 0;
      static long read_bytes= 0;
      static long read_bytes2= 0;
      data_size= DATA_BUFF_SIZE;
      data_size2= DATA_BUFF_SIZE;
      //
      // Read from MEB .....
      time(&now);
      if( !cvt_V1190_read_MEB( &board_data, data_buff, &data_size))
	{
	  printf( " \nError executing cvt_V1190_read_MEB \n");
	  ret_val= -5;
	  goto exit_point;
	}
      // Read from MEB .....
      if( !cvt_V1190_read_MEB( &board_data2, data_buff2, &data_size2))
	{
	  printf( " \nError executing cvt_V1190_read_MEB \n");
	  ret_val= -5;
	  goto exit_point;
	}

      if( !data_size && !data_size2)
	continue;       

      
      fakeTime=(now-1000000000)|CVT_V1190_GLOBAL_TRAILER;
      
      //
      // .... and store raw data into file
      if(data_size>0) {
	if(now>lastTime1) {
	  fwrite((char*)&fakeTime,1,4,outputFile1);
	  lastTime1=now;
	}
	if( fwrite( data_buff, 1, data_size, outputFile1)!= data_size)
	  {
	    printf( " \nError writing raw data file \n");
	    ret_val= -5;
	    goto exit_point;
	  }
	read_bytes+= data_size;
	if(  read_bytes> last_read_bytes+200000)
	  {
	    // 
	    // Give user a feedback every 200KB data
	    fprintf(stderr, ".");
	    //fprintf(stderr,"read_bytes: %d\n",read_bytes);
	    last_read_bytes= read_bytes;
	  }

      }
      if(data_size2>0) {
	if(now>lastTime2) {
	  fwrite((char*)&fakeTime,1,4,outputFile2);
	  lastTime2=now;
	}
	if( fwrite( data_buff2, 1, data_size2, outputFile2)!= data_size2)
	  {
	    printf( " \nError writing raw data file \n");
	    ret_val= -5;
	    goto exit_point;
	  }	
	read_bytes2+= data_size2;
	if(  read_bytes2> last_read_bytes2+200000)
	  {
	    // 
	    // Give user a feedback every 200KB data
	    fprintf(stderr, "*");
	    //fprintf(stderr,"read_bytes: %d\n",read_bytes);
	    last_read_bytes2= read_bytes2;
	  }
      }      
    }
  printf(  "\n Done \n");


  FILE *rawOutArray[2]={outputFile1,outputFile2};
  int filecount;
  for(filecount=0;filecount<2;filecount++) {

    if( fflush( rawOutArray[filecount]))
      {
	printf( "\nError flushing raw output file  \n");
	ret_val= -5;
	goto exit_point;
      }
  }


	

 exit_point:	
  /////////////////////////////////////////
  // Library specific
  /////////////////////////////////////////
	
  //
  // Release board resources
  if( !cvt_V1190_close( &board_data))
    {	
      printf( "\nError executing cvt_V1190_close\n");
    }

  /////////////////////////////////////////
  // Demo application specific
  /////////////////////////////////////////

  if( outputFile1!= NULL)
    {
      fclose( outputFile1);
    }
  if( outputFile2!= NULL)
    {
      fclose( outputFile2);
    }
  if( data_buff!= NULL)
    {
      free( data_buff);
    }

 

  return ret_val;
}

