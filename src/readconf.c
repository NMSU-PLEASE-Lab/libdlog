
#include <stdio.h>
#include <string.h>

typedef enum {NO, YES} YesNoFlag;
typedef enum {M_NO, M_YES, M_MD5} YesNoMD5Flag;
typedef enum {LOGTOFILE, LOGTOSYSLOG} LoggingLocation;

/* external MD5 implementation */
extern char* logMD5(char* data); 

/**
 *  Flag if logging or not; 1:to log 0:to not
 *  It can be changed by modifying the config file
 */
static YesNoFlag logDoLogging = YES;

/**
 *  A global integer value which is produced by "dlogBeginTransfer"
 *  function and will be passed to "dlogEndTransfer" function, so both
 *  the begin and end transfers can be related to each other.
 */
static unsigned long int nextTransferID = 0;

/**
 *  config file location
 */
static char configFilename[500];

/**
 *  Flag whether to recored the file-name into the logging file or not; 0:YES
 *  1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag fileNameFormat = M_YES; 

/**
 *  Flag whether to recored the file-extension into the logging file or not; 
 *  0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag fileExtFormat = M_YES; 

/**
 *  Flag whether to recored the source path into the logging file or not; 
 *  0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag sourcePathFormat = M_YES; 

/**
 *  Flag whether to recored the destination path into the logging file or not;
 *  0:YES 1:NO 2:MD5. It can be changed by modifying the config file
 */
static YesNoMD5Flag targetPathFormat = M_YES; 

/**
 *  Flag whether to recored the user ID into the logging file or not; 
 *  0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag userIDFormat = M_YES;   

/**
 *  Flag whether to recored the source IP address into the logging file or not;
 *  0:YES 1:NO 2:bitmask. It can be changed by modifying the config file
 */
static YesNoFlag sourceIPFormat = YES;    

/**
 *  Flag whether to recored the destination IP address into 
 *  the logging file or not; 0:YES 1:NO 2:bitmask.
 *  It can be changed by modifying the config file
 */
static YesNoFlag targetIPFormat = YES;   

/**
 *  Logging file location
 *  It can be changed by modifying the config file
 */
static char dlogFilename[500];

/**
 *  0: log the data to some local files specified by "dlogFilename" variable,
 *  1: log it using the "syslog" capability
 *  
 */
static LoggingLocation loggingLocation = LOGTOFILE;

/**
 *  A name of an application which uses this library; 
 *  the name will be recorded into loggoing file.
 */
static char appName[100];

/**
 *  Bitmasks to mask IPs before recording into logging file
 *  
 */
static unsigned int sourceIPMask = 0xffff, targetIPMask = 0xffff;

static YesNoFlag alreadyInitialized = NO;


int main(int argc, char *argv[])
{
   FILE* f; char *sp;
   char buf[500], option[500], value[500];

   if (! (f = fopen (argv[1],"rf")))
      return 0;

   // read conf file
   while (!feof(f)) 
   {
      *buf = '\0';  
      fgets(buf, 500-1, f);
      if (buf[0] == '#' || buf[0] == '\n') // skip comment line or blank line
         continue;
      //buf[strlen(buf)-1] = '\0'; // remove newline (doesn't matter, really)
      //printf("(%s)\n",buf); // debugging
      sp = strchr(buf,'=');
      if (sp == 0)
         continue;
      *sp = '\0'; // split line at equals sign
      sscanf(buf,"%s",option);  // get no-whitespace strings
      sscanf(sp+1,"%s",value);
      //printf("  (%s) = (%s)\n",option,value); // debugging
      *sp = '='; // restore line in case we print an error message

      if (strcmp(option,"DoLogging") == 0)
      {
         if (!strcmp("yes",value))
            logDoLogging = YES;
         else if (!strcmp("no",value))
            logDoLogging = NO;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option, "LogFilename") == 0)
      {
         strcpy(dlogFilename, value);
      }
      else if (strcmp(option, "LoggingLocation") == 0)
      {  
         if (!strcmp("file",value))
            loggingLocation = LOGTOFILE;
         else if (!strcmp("syslog",value))
            loggingLocation = LOGTOSYSLOG;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogFilename") == 0)
      {  
         if (!strcmp("yes",value))
            fileNameFormat = M_YES;
         else if (!strcmp("no",value))
            fileNameFormat = M_NO;
         else if (!strcmp("md5",value))
            fileNameFormat = M_MD5;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogExtension") == 0)
      {  
         if (!strcmp("yes",value))
            fileExtFormat = M_YES;
         else if (!strcmp("no",value))
            fileExtFormat = M_NO;
         else if (!strcmp("md5",value))
            fileExtFormat = M_MD5;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogSourcePath") == 0)
      {  
         if (!strcmp("yes",value))
            sourcePathFormat = M_YES;
         else if (!strcmp("no",value))
            sourcePathFormat = M_NO;
         else if (!strcmp("md5",value))
            sourcePathFormat = M_MD5;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogTargetPath") == 0)
      {  
         if (!strcmp("yes",value))
            targetPathFormat = M_YES;
         else if (!strcmp("no",value))
            targetPathFormat = M_NO;
         else if (!strcmp("md5",value))
            targetPathFormat = M_MD5;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogUserID") == 0)
      {  
         if (!strcmp("yes",value))
            userIDFormat = M_YES;
         else if (!strcmp("no",value))
            userIDFormat = M_NO;
         else if (!strcmp("md5",value))
            userIDFormat = M_MD5;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"DoLogSourceIP") == 0)
      {
         if (!strcmp("yes",value)) {
            sourceIPFormat = M_YES;
            sourceIPMask = 0xffff;
         } else if (!strcmp("no",value)) {
            sourceIPFormat = M_NO;
         } else {
            unsigned int d1,d2,d3,d4;
            if (sscanf(value,"%u.%u.%u.%u",&d1,&d2,&d3,&d4) != 4)
               goto FORMATERROR; //raise error
            sourceIPFormat = M_MD5;
            sourceIPMask = (d1<<24) | (d2<<16) | (d3<<8) | (d4);
         }
      }
      else if (strcmp(option,"DoLogTargetIP") == 0)
      {  
         if (!strcmp("yes",value)) {
            targetIPFormat = YES;
            targetIPMask = 0xffff;
         } else if (!strcmp("no",value)) {
            targetIPFormat = NO;
         } else {
            unsigned int d1,d2,d3,d4;
            if (sscanf(value,"%u.%u.%u.%u",&d1,&d2,&d3,&d4) != 4)
               goto FORMATERROR; //raise error
            targetIPFormat = YES;
            targetIPMask = (d1<<24) | (d2<<16) | (d3<<8) | (d4);
         }
      }
   } 
   fclose(f);  
   alreadyInitialized = YES;
   printf("alreadyInitialized = %d\n",alreadyInitialized);
   printf("logDoLogging = %d\n",logDoLogging);
   printf("dlogFilename = (%s)\n",dlogFilename);
   printf("loggingLocation = %d\n",loggingLocation);
   printf("nextTransferID = %u\n",(unsigned int)nextTransferID);
   printf("fileNameFormat = %d\n",fileNameFormat);
   printf("fileExtFormat = %d\n",fileExtFormat);
   printf("sourcePathFormat = %d\n",sourcePathFormat);
   printf("targetPathFormat = %d\n",targetPathFormat);
   printf("userIDFormat = %d\n",userIDFormat);
   printf("sourceIPFormat = %d\n",sourceIPFormat);
   printf("sourceIPMask = 0x%x\n",sourceIPMask);
   printf("targetIPFormat = %d\n",targetIPFormat);
   printf("targetIPMask = 0x%x\n",targetIPMask);
/*
   printf(" = %d\n",);
   printf(" = %d\n",);
   printf(" = %d\n",);
   printf(" = %d\n",);
*/
   return 1;

FORMATERROR:
   fclose(f);  
   alreadyInitialized = YES;
   logDoLogging = NO;
   fprintf(stderr,"Error in DLOG configuration file format, line (%s)", buf);
   fprintf(stderr," -- no logging will be done.\n");
   return 1;
}

