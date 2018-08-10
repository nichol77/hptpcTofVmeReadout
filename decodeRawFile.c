/////////////////////////////////////////////////////////////////////////////
// Name:        decodeRawFile
// Purpose:     Decodes raw TDC files into something more useful
// Licence:     Based on CAEN V1190 Demo 
/////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////
// File includes
////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#if defined (LINUX)
	#include <memory.h>
	#include <ctype.h>
#endif
#include "common_defs.h"
#include "cvt_board_commons.h"
#include "cvt_common_defs.h"
#include "cvt_V1190.h"

////////////////////////////////////////////
// File local defines
////////////////////////////////////////////
#define GLB_HDR_STR		"GLB_HDR   - EVT COUNT   : %08x GEO      : %08x \n"
#define GLB_TRL_STR		"GLB_TRL   - STATUS      : %08x WCOUNT   : %08x GEO     : %08x \n"
#define TDC_HDR_STR		" TDC_HDR  - TDC         : %08x EVT ID   : %08x BUNCH ID: %08x \n"
#define TDC_MSR_STR		"  TDC_MSR - TRAILING    : %08x CH       : %08x MEASURE : %08x \n"
#define TDC_ERR_STR		"  TDC_ERR - TDC         : %08x ERR FLAGS: %08x \n"
#define TDC_TRL_STR		" TDC_TRL  - TDC         : %08x EVT ID   : %08x WCOUNT  : %08x \n"
#define TDC_TRG_STR		"  TDC_TRG - TRG TIME TAG: %08x \n"
#define FILLER_STR		"  FILLER  - \n"
#define UNKNOWN_STR		"\n??? UNKNOWN TAG ??? -          READ WORD: %08x \n\n"

#define HPTPC_OUTPUT_STR "%02d, %d\n"


void usage(char **argv) {
  printf("Usage:\n\t%s <raw file> <text file>\n",argv[0]);
}
  


int main(int argc, char **argv) {
  if(argc<3) {
    usage(argv);
    return -1;
  }

  						// raw output file
  UINT8 *data_buff= NULL;	
  const int DATA_BUFF_SIZE= 1024*1024;			// The data buffer size
  UINT32 data_size;
  char rawName[180];
  sprintf(rawName,"%s",argv[1]);
  char parsedName[180];
  sprintf(parsedName,"%s",argv[2]);
  


  FILE* raw_file= NULL;					// parsed output file
  FILE* parsed_file= NULL;
  // Create output files
  if( ( parsed_file= fopen( parsedName, "wt"))== NULL)
    {
      printf( "Error creating parsed first output file '%s'", parsedName);
      return -2;
    }
  
  if( ( raw_file= fopen( rawName, "rb"))== NULL)
    {
      printf( "Error opening raw input file '%s'", rawName);
      return -2;
    }


  data_buff= malloc( DATA_BUFF_SIZE);
  if( data_buff== NULL)
    {
      // Insufficient memory
      printf( "Error allocating events' buffer (%i bytes)", DATA_BUFF_SIZE);
      return -4;
    }

  
  UINT32 timeVal;
  while( ( data_size= ( UINT32)fread( data_buff, 4, DATA_BUFF_SIZE>> 2, raw_file)))
    {
      UINT32 *tmp_buff= (UINT32*)data_buff;
      char line[ 400];
      size_t str_len;
      
      while( data_size-- > 0)
	{
	  UINT32 data= *(tmp_buff++);
	  *line= '\0';
	  //	  if(data& CVT_V1190_DATA_TYPE_MSK && (data&CVT_V1190_DATA_TYPE_MSK)!=CVT_V1190_FILLER)
	    //	    fprintf(stderr,"What have we: bytes left %d -- %#x -- %#x\n",data_size,data & CVT_V1190_DATA_TYPE_MSK,CVT_V1190_DATA_TYPE_MSK);
	  switch( data& CVT_V1190_DATA_TYPE_MSK)
	    {
	    case CVT_V1190_GLOBAL_TRAILER: //RJN overloaded for unixTime
	      {
		timeVal=data&0x7fffffff;
		timeVal+=1000000000;		
		sprintf( line, "unix: %u\n",timeVal);
		//		printf(line);
		fprintf(stderr,"*");
	      }
	      break;
	    case CVT_V1190_TDC_MEASURE:
	      {
		// TDC measure
		//		UINT32 trailing= CVT_V1190_GET_TDC_MSR_TRAILING( data);
		UINT32 channel= CVT_V1190_GET_TDC_MSR_CHANNEL( data);
		UINT32 measure= CVT_V1190_GET_TDC_HDR_MEASURE( data);
		//fix July 2013
		channel= CVT_V1290_GET_TDC_MSR_CHANNEL( data);
		measure= CVT_V1290_GET_TDC_HDR_MEASURE( data);
		sprintf( line, HPTPC_OUTPUT_STR, channel, measure);
	      }
	      break;
	    case CVT_V1190_TDC_ERROR:
	      {
		UINT32 tdc= CVT_V1190_GET_TDC_ERR_TDC( data);
		UINT32 err_flags= CVT_V1190_GET_TDC_ERR_ERR_FLAGS( data);
		
		sprintf( line, TDC_ERR_STR, tdc, err_flags);
	      }
	      break;
	    case CVT_V1190_TDC_TRAILER:
	      {
		UINT32 tdc= CVT_V1190_GET_TDC_TRL_TDC( data);
		UINT32 event_id= CVT_V1190_GET_TDC_TRL_EVENT_ID( data);
		UINT32 wcount= CVT_V1190_GET_TDC_TRL_WCOUNT( data);
		
		sprintf( line, TDC_TRL_STR, tdc, event_id, wcount);
	      }
	      break;
	    case CVT_V1190_GLOBAL_TRIGGER_TIME:
	      {
		UINT32 trg_time_tag= CVT_V1190_GET_GLB_TRG_TIME_TAG( data);
		
		sprintf( line, TDC_TRG_STR, trg_time_tag);
	      }
	      break;
	    case CVT_V1190_FILLER:
	      {
		//		sprintf( line, FILLER_STR);
	      }
	      break;
	    default:
	      {
		//RJN no idea why we end up here and have to mess around again with the time
		//		sprintf( line, UNKNOWN_STR, data);		
		timeVal=data&0x7fffffff;
		if(timeVal<1000000000)
		  timeVal+=1000000000;		
		sprintf( line, "unix: %u\n",timeVal);
		fprintf(stderr,"*");
		///		printf(line);
	      }
	      break;
	    }
	  if( (str_len= strlen( line))> 0)
	    {
	      if( fwrite( line, 1, str_len, parsed_file)!= str_len)
		{
		  // error writing file
		  printf( "\nError writing parsed output file '%s' \n", parsedName);
		  return -3;
		}
	    }
	}
    }
  
  printf(  " Ok\n");
  
  if( parsed_file!= NULL)
    {
      fclose( parsed_file);
    }

  return 0;

}
