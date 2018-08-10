/* config.h - <short description of this file/module> */

/* G.J.Crone, University College London */

/*
 * Current CVS Tag:
 * $Header: /afs/rl.ac.uk/user/t/tcn/cvsroot/online/packages/configLib/configLib.h,v 1.2 2001/07/27 12:56:36 gjc Exp $
 */
 
/* 
 * Modification History : DO NOT EDIT - MAINTAINED BY CVS 
 * $Log: configLib.h,v $
 * Revision 1.2  2001/07/27 12:56:36  gjc
 * Added CONFIG_E_UNNAMED error code and configValidate prototype.
 *
 * Revision 1.1  2001/07/24 13:36:43  gjc
 * First check in of new package
 *
 *
 */ 

#ifndef _config_H
#define _config_H
#ifdef __cplusplus 
extern "C" { 
#endif /* __cplusplus */
 
/* includes */

/* defines */
#define CONFIG_VAR "DAQ_CONFIG_DIR"
#define BLOCKNAME_MAX 64
#define MAX_BLOCKS 32
#define RECLEN_MAX 4096

/* typedefs */
   typedef enum {
      CONFIG_E_OK = 0,
      CONFIG_E_NOFILE = 0x100,
      CONFIG_E_NESTING,
      CONFIG_E_EOF,
      CONFIG_E_SYSTEM,
      CONFIG_E_KVP,
      CONFIG_E_SECTION,
      CONFIG_E_UNNAMED
   } ConfigErrorCode ;
/* global variable declarations*/   

/* function prototype declarations */
   char* configFileSpec (char* fileName) ;
   ConfigErrorCode configLoad (char* fleName, char* blockList) ;
   ConfigErrorCode configStore (char* fileName, char* blockName) ;
   char* configErrorString (ConfigErrorCode code) ;
   ConfigErrorCode configValidate (char* fileName) ;
#ifdef __cplusplus 
} 
#endif /* __cplusplus */ 

#endif /* _config_H */
