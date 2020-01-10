/**
* @file private.c
*
* Private functions for libdlog.
*
* This file is directly included so that the functions can be static and
* not show up in the library symbol table
*
* @author Jonathan Cook
* @date 04/20/2014
* @version 0.9c
*/

#include <stdio.h>	 
#include <string.h>	
#include <errno.h>	
#include <sys/socket.h>	 
#include <netinet/in.h> 
#include <arpa/inet.h>	
#include <unistd.h>	
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <limits.h>
#include <syslog.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <libdlog.h>

#define MAXLOGTOFILE   2048  //!< Maximum size of log entry
#define MAXFILEPATH     512  //!< Maximum size of filename
#define MAXHOSTNAME     128  //!< Maximum size of hostname or IP string
#define MAXDIGEST        32  //!< Maximum size of MD5 digest
#define MAXUSER          32  //!< Maximum size of username
#define MAXANNOTATION   128  //!< Maximum annotation size

/** Generic boolean config value */
typedef enum {NO, YES} YesNoFlag;
/** 3-way config values */
typedef enum {M_NO, M_YES, M_MD5} YesNoMD5Flag;  
typedef enum {R_NO, R_YES, R_RAW} YesNoRawFlag;  
/** Where to put log data */
typedef enum {LOGTOFILE, LOGTOSYSLOG} LoggingLocation; 

/** external MD5 implementation */
extern char* dlogMD5(char* data, char *digest); 

/**
 * Flag if logging or not; 1:to log 0:to not
 * It can be changed by modifying the config file
 */
static YesNoFlag logDoLogging = YES;

/**
 * A global integer value which is produced by "dlogBeginTransfer"
 * function and will be passed to "dlogEndTransfer" function, so both
 * the begin and end transfers can be related to each other.
 */
static unsigned long nextTransferID = 0;

/**
 * config file location
 */
static char configFilename[MAXFILEPATH];

/**
 * Flag whether to record the file-name into the logging file or not;
 * 0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag fileNameFormat = M_YES; 

/**
 * Flag whether to record the file-extension into the logging file or not; 
 * 0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag fileExtFormat = M_YES; 

/**
 * Flag whether to record the source path into the logging file or not; 
 * 0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag sourcePathFormat = M_YES; 

/**
 * Flag whether to record the destination path into logging file or not;
 * 0:YES 1:NO 2:MD5. It can be changed by modifying the config file
 */
static YesNoMD5Flag targetPathFormat = M_YES; 

/**
 * Flag whether to record the user ID into the logging file or not; 
 * 0:YES 1:NO 2:MD5. It can be changed by modifying the config file.
 */
static YesNoMD5Flag userIDFormat = M_YES;   

/**
 * Flag whether to record the source IP address into logging file or not;
 * yes/no/raw. Raw means host identifier string passed in will not be
 * processed; flag can be changed by modifying the config file
 */
static YesNoRawFlag sourceIPFormat = YES;    

/**
 * Flag whether to record the target IP address into logging file or not;
 * yes/no/raw. Raw means host identifier string passed in will not be
 * processed; flag can be changed by modifying the config file
 */
static YesNoRawFlag targetIPFormat = YES;   

/**  DJL libdlog 09/2012
 * Logging identifier name (for syslog use)
 * It can be changed by modifying the config file
 */
static char syslogIdent[MAXFILEPATH];

/** DJL libdlog 09/2012
 * Logging option (for syslog use)
 * It can be changed by modifying the config file
 */
static int syslogOption = LOG_PID;

/** DJL libdlog 09/2012
 * Logging facility (for syslog use)
 * It can be changed by modifying the config file
 */
static int syslogFacility = LOG_USER;

/** DJL libdlog 09/2012
* Logging level (for syslog use)
* It can be changed by modifying the config file
*/
static int syslogLevel = LOG_INFO;

/**
* Include an annotation field in the log record 
*/
static YesNoFlag logAnnotation = YES;

/**
* Batching level for log I/O: write when this number of
* records are available.
*/
static unsigned int logBatchSize = 5;

/**
 * Logging file location
 * It can be changed by modifying the config file
 */
static char dlogFilename[MAXFILEPATH];

/**
* 0: log the data to some local files specified by "dlogFilename" variable,
* 1: log it using the "syslog" capability
*/
static LoggingLocation loggingLocation = LOGTOFILE;

/* TODO: New options to implement
static timeFormat;
static durationFormat;
static annotationFormat;
static transferModeFormat;
static unsigned int optBatchLogging=1;
static unsigned int optMaxOpenTransfers=1000000;
*/

/**
 * A name of an application which uses this library; 
 * the name will be recorded into loggoing file.
 */
static char appName[MAXFILEPATH];

/**
 * Bitmask for logging source IP address.
 */
static unsigned int sourceIPMask = 0xffffffff;
/**
 * Bitmask for logging target IP address.
 */
static unsigned int targetIPMask = 0xffffffff;

/**
 * Session ID
 */
static unsigned long sessionID = 0x00abcdef;

/**
* Struct to hold initial data about active transfers. This will 
* be combined with ending data to populate a log record.
*/
struct dlogLoggingData
{  
   unsigned long id;
   unsigned int xferType;
   char fileName[MAXFILEPATH];
   char fileExt[MAXFILEPATH];
   unsigned long size;
   char sourceDir[MAXFILEPATH];
   char targetDir[MAXFILEPATH];
   char user[MAXUSER];
   char annotation[MAXANNOTATION];
   char sourceIP[MAXHOSTNAME];
   char targetIP[MAXHOSTNAME];
   struct timeval startTval;
   struct timeval endTval;
   int errorFlag;
   struct dlogLoggingData *next;
};

/**
* List holding records of active transfers. Node is created when transfer
* begins and is deleted when transfer ends.
*/
static struct dlogLoggingData *activeXferList=0;

/**
* List holding records of ended-but-not-logged transfers. Will log in batch.
*/
static struct dlogLoggingData *endedXferList=0;

/**
 * General mutex for internal data struct protection
 */
static pthread_mutex_t generalMutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * Mutex for writing to log file
 */
static pthread_mutex_t logfileMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Marks whether dlogInit() has been already called or not.
 */
static YesNoFlag alreadyInitialized = NO;


/**
* Internal function to add initial logging data to a linked list before it
* is recorded into file, so it can be retrieved later by calling 
* the function "dlogRemoveLoggingData".
* @param llist the linked list to add the record to
* @param logRecord the record to add
* @return 0 on success, 1 on error (logRecord is NULL)
*/

static unsigned int dlogAddLoggingData(struct dlogLoggingData **llist,
                                struct dlogLoggingData *logRecord) 
{
   unsigned int stat=1; // assume error (record is null is only error)
   // protect shared list
   pthread_mutex_lock( &generalMutex );
   if (logRecord != NULL) 
   {  
      if (*llist != NULL)
         logRecord->next = *llist;    
      else  // for first node
         logRecord->next = NULL;
      *llist = logRecord;
      stat = 0;
   }
   // unprotect shared list
   pthread_mutex_unlock( &generalMutex );
   return stat;
}


/**
* Internal function to retrieve the logging data related to transferID and
* then remove it from the linked list
*
* @param llist the linked list to remove a record from
* @param transferID  the transfer ID of the record to find and remove
                     (if 0, remove first item)
* @return pointer to found+removed record, or NULL if not found
*/
static struct dlogLoggingData *dlogRemoveLoggingData(
                                   struct dlogLoggingData **llist, 
                                   unsigned long transferID)
{
   struct dlogLoggingData *cur,*prev;

   if (*llist == NULL) 
      return 0;
   // protect shared list
   pthread_mutex_lock( &generalMutex );

   cur = *llist;
   prev = 0;
   while (cur != NULL && transferID) // skip if tid==0
   {
      if (cur->id == transferID) // found
      {  
         break;
      } else {
         prev = cur;
         cur = cur->next;
      }
   }
   if (cur) // then found
   {  
      if (!prev)
         *llist = cur->next; // cur is head of list
      else
         prev->next = cur->next; // ax it from middle
      cur->next = 0;
   }
   // unprotect shared list
   pthread_mutex_unlock( &generalMutex );
   return cur; // null if not found
}


/*
* Internal function that does actual recording of the logging data
* into a file or syslog
* @param rec the string containing the formatted logging record, 
*            null terminated
* @return  0 if data is recorded into a file successfully, other if not.
*
* NOT USED ANYMORE
static int dlogRecordtoFile(const char * rec)
{ 

   if (lres)
   {
      // No lock acquired!
      // JEC: What to do here? certainly not perror()
      // perror("Error locking libdlog log file");
   } else {
      // write log entry to file
      fprintf(logFileHandle, "%s", rec);
   }
   // do unlock whether or not there was a lock
   // error just in case of any system wierdness
   if (lres)
      return 2; // was a lockf() error
   else
      return 0;
}
*/

/**
* Process all finished transfer records and write them out to log file or
* syslog. This processes the endedXferList and logs all entries on the
* list, leaving it empty.
* @return 0 on sucess, other if error
*/
static unsigned int writeLogData()
{
   struct dlogLoggingData *data=0;
   char buff[MAXLOGTOFILE];
   int stat=0;
   double duration;
   struct timespec sleepTime;
   int i=0,lres=0; // lockf result
   FILE* logFileHandle = 0;

   // a pthread lock is used for local thread mutex,
   // then lockf() is used for system-wide file locking;
   // perhaps only lockf() is not needed, but it should be safe
   pthread_mutex_lock( &logfileMutex );

   // if 'syslog' capability is used for logging data,
   // then write the syslog entry and return
   if (loggingLocation == LOGTOSYSLOG) 
   {
      openlog(syslogIdent, syslogOption, syslogFacility);
   } else if (loggingLocation == LOGTOFILE) 
   {
      // file logging
      logFileHandle = fopen(dlogFilename,"a"); 
      if (! logFileHandle)
      {
         pthread_mutex_unlock( &logfileMutex );
         return 1;
      }
      // else file was opened ok
      // now lock file for writing (but control max time 
      // spent trying, for now, 2 seconds)
      for (i=0; i < 200; i++)
      {
         lres = lockf(fileno(logFileHandle), F_TLOCK, 0);
         if (!lres) break; // acquired lock!
         if (lres != EACCES && lres != EAGAIN)
            break; // some other bad error!
         sleepTime.tv_sec = 0;
         sleepTime.tv_nsec = 10000000; // (1/100 second)
         nanosleep(&sleepTime,0); // wait 1/100 second
      }
      if (lres) // file lock error!
      {
         pthread_mutex_unlock( &logfileMutex );
         return 1;
      }
   } else // incorrect logging location
   {
      pthread_mutex_unlock( &logfileMutex );
      return 1;
   }

   // Now we have our logging connection, so log some records
   
   // TODO: Create timestamp formats

   // grab finished records until there are no more
   while ((data=dlogRemoveLoggingData(&endedXferList,0))!=NULL)
   {
      duration =  (data->endTval.tv_sec - data->startTval.tv_sec)*1.0e6 + 
                  (data->endTval.tv_usec - data->startTval.tv_usec); 
      duration = duration / 1.0e3; // create milliseconds
      // format record
      sprintf(buff, "%s %s name='%s' fileExt='%s' size=%lu sourceDir='%s' "
                 "targetDir='%s' session=%lu user='%s' startTime=%lu "
                 "duration=%.3f success='%s' "
                 "sourceIP='%s' targetIP='%s' "
                 "note='%s'\n",
            appName,
            (data->xferType==DLOG_RECEIVE) ? "RECEIVE":"SEND", 
            data->fileName, data->fileExt, data->size, data->sourceDir,
            data->targetDir, sessionID, data->user, data->startTval.tv_sec,  
            /*(data->endTval.tv_sec - data->startTval.tv_sec), 
            (data->endTval.tv_usec - data->startTval.tv_usec),*/
            duration,
            (data->errorFlag) ? "no":"yes", 
            data->sourceIP, data->targetIP, data->annotation);
      // now log the record      
      if (loggingLocation == LOGTOSYSLOG) 
      {
         syslog(syslogFacility | syslogLevel,"%s",buff);
      } else if (loggingLocation == LOGTOFILE) 
      {
         fprintf(logFileHandle, "%s", buff);
      }
   }

   // now close off the logging facility
   if (loggingLocation == LOGTOSYSLOG) 
   {
      closelog();
   } else if (loggingLocation == LOGTOFILE) 
   {
      lockf(fileno(logFileHandle), F_ULOCK, 0);
      fclose(logFileHandle);
   }
   
   // release the logfile mutex
   pthread_mutex_unlock( &logfileMutex );
   
   if (!stat)
      return 0;
   return 2+stat; // send unique errors on up
}


/**
* Internal function to find and return the location of the config file.
* Locations in order: environment variable "DLOG_CONFIG"; file ".dlog.rc"
* in directory of environment variable "HOME"; the absolute file
* "/etc/dlog/dlog.conf"
* @return 1 if a config file was found, otherwise 0
*/
static int setConfigFile()
{
   char *confFile;
   FILE * checkFile;
   
   confFile = getenv("DLOG_CONFIG");
   if (confFile != NULL)
   {  
      checkFile = fopen(confFile, "rf");
      if (checkFile)
      {
         fclose(checkFile);
         strncpy(configFilename, confFile, MAXFILEPATH);
         return 1;
      }
   }

   confFile = getenv("HOME");
   strcat(confFile,"/.dlog.rc");
   if (confFile != NULL)
   {  
      checkFile = fopen(confFile, "rf"); 
      if (checkFile)
      {
         fclose(checkFile);
         strncpy(configFilename, confFile, MAXFILEPATH);
         return 1;
      }
   }

   char tmp[MAXFILEPATH] = "/etc/dlog/dlog.rc";
   checkFile = fopen(tmp, "rf"); 
   if (checkFile)
   {
      fclose(checkFile);
      strncpy(configFilename, tmp, MAXFILEPATH);
      return 1;
   }
   else
      return 0;
}

/**
* Internal function to mask an IP address string with netmask and leave
* the masked network address in the same string
* @param ip  A string containing an IP address (in/out)
* @param mask  An int containing a netmask,(e.g. 8, 16, 24, ...)
* @param size is the (max) size of the ip string
* @return  nothing, the input string is changed
*/
static void doIPBitmask(char *ip, unsigned int mask, unsigned int size)
{
   struct in_addr addr,netmask;
   unsigned long network;

   if (! inet_aton(ip,&addr) ) 
   {
      // not a valid IP address
      return;
   }

   netmask.s_addr = htonl(mask);
   
   network = ntohl(addr.s_addr) & ntohl(netmask.s_addr);
   struct in_addr addr2;
   addr2.s_addr = htonl(network);

   strncpy(ip,inet_ntoa(addr2),size);
   ip[size-1] = '\0';

   return;
}

/**
*  Get a local IP address. It is called if a 
*  source or destination is local
* @return  The local IP adress (dotted decimal), 0 if error
*/
static char *getLocalIP()
{
   int i=0;
   char *retString = (char *) malloc(MAXFILEPATH * sizeof (char));
   struct hostent * host; 
      //?? = (struct hostent * ) malloc ( sizeof ( struct hostent ));

   gethostname ( retString , MAXFILEPATH ) ;

   if ((host = ( struct hostent * ) gethostbyname ( retString )) )
   {
      // JEC: May need to iterate here; danger of retrieving 127.0.?.?
      do {
         strcpy(retString , 
                inet_ntoa(*((struct in_addr *)host->h_addr_list[i++])));
      } while (strstr(retString,"127") == retString && host->h_addr_list[i]);
      return retString;
   }
   else {
      free(retString);
      return 0;
   }
}


/**
* Get a remote host IP address from a host-name. Note that this
* is potentially a DNS operation and thus should really be avoided.
* @param hostName  A string containing a host-name 
* @return  IP address of the remote host (dotted decimal string), 
*          NULL if error
*/
static char *getRemoteIP(char * hostName) 
{       
   char *retString = 0;
   char *tmp=0;

   struct hostent *he;
   struct in_addr **addr_list;
   int i;

   if ((he = gethostbyname( hostName ) ) == NULL) 
   {
      // error in gethostbyname?
      return 0;
   }

   retString = (char *) malloc(MAXHOSTNAME * sizeof (char));

   addr_list = (struct in_addr **) he->h_addr_list;

   for (i = 0; addr_list[i] != NULL; i++)
   {
      tmp = inet_ntoa(*addr_list[i]);
      if (strstr(tmp,"127") != tmp)
         break;
   }

   strcpy(retString,tmp);
   return retString;
} 


/**
* Find and create an IP address string from a given machine name
* @param name is the machine name (might be an IP address already)
* @param ipString is a char array to hold output IP address string
* @param size is the size of ipString array
* @return nothing
*/
static void findIPAddress(char *name, char *ipString, unsigned int size)
{
   struct in_addr ipaddr;
   // check if given machine name is already a valid IP address
   if (inet_aton(name,&ipaddr))
   {
      strncpy(ipString,name,size);
      ipString[size-1] = '\0';
      return;
   }
   // else process name as a machine name
   if (!strcmp(name,"localhost") || name[0]=='\0') 
   {
      // doesn't really work -- should not do this
      char *tmp = getLocalIP();
      if (tmp != NULL)
      {
         strncpy(ipString,tmp,size);
         free(tmp);
         tmp=NULL;
      }
      else
         strcpy(ipString,"?no-IP?");
   }
   else
   {  
      char *tmp = getRemoteIP(name);
      if (tmp != NULL)
      {
         strncpy(ipString,tmp,size);
         free(tmp);
         tmp=NULL;
      }
      else
         strcpy(ipString,"?no-IP?");
   }
   ipString[size-1] = '\0';
}


/**
* To check and convert a number string to an integer
* return the converted integer, and -1 if there is an error.
* @param numstr the string to be check if it's a number.
* @return an integer of the number string or -1 if not. (JEC: -1 is valid??)
*/
static int stringToNumber(char *numstr) 
{
  int numVal;
  char *endPtr;

  numVal = (int)strtol(numstr, &endPtr, 10);

  if ((endPtr == numstr) || (*endPtr != '\0'))
  {
      numVal = -1;
  }

  return numVal;
}


/** 
* Extract a base filename from a full path.
* @param name  A string containing a source full path of a file including
*              its extension.
* @param root  Character array to hold the root file name
* @param size  The number of characters in root;
               root name will be at most 1 less
* @return Nothing
*/
static void getBaseFilename(char *name, char *root, unsigned int size)
{  
   char *tmp;
   if (!name || !root)
      return;
   if ((tmp = strrchr(name,'/')) )
      strncpy(root,++tmp,size);
   else
      strncpy(root,name,size);
   root[size-1] = '\0';
}


/**
* Extract a directory path of a filename that has 
* a full path (e.g. to remove the file-name and its extension)
* @param name  string containing the full path of a file
* @param path  character array to hold the path name
* @param size  size of path[]; path will be at most 1 less + \0
* @return nothing
*/
static void getPathFromFilename(char *name, char *path, unsigned int size)
{  
   char *fileNameOnly = NULL;
   if (!name || !path) 
      return;
   // find last / in filename
   if ((fileNameOnly = strrchr(name,'/')) )
   {
      ++fileNameOnly;
      if ((fileNameOnly-name) < size)
         size = (fileNameOnly-name)+1;
      strncpy(path, name, size);
      path[size-1] = '\0'; 
   } else {
      path[0] = '\0';
   }
   return;
}

/**
* Extract a file extension from a file-name
* @param name  string containing the full filename.
* @param ext   character array to hold the file name extension
* @param size  size of ext[]; extension will be at most 1 less + \0
* @return nothing
*/
static void getFilenameExtension(char *name, char *ext, unsigned int size) 
{
   char *tmp;
   if (!name || !ext)
      return;
   // find last dot in filename
   if ((tmp = strrchr(name,'.')) )
   {
      strncpy(ext,++tmp, size);
      ext[size-1] = '\0';
   } else {
      ext[0] = '\0';
   }
   return;
}

/**
* Clean a string of 'bad' characters -- simply replace with '-'.
* Characters replaced are single and double quotes (maybe more to follow).
* @param str is the string to be cleaned (modified)
* @param max is maximum length of string (for safety, should not be needed)
* @return nothing, the string is modified if necessary
*/
static void cleanString(char* str, int max)
{
   int i;
   if (max==0) max = 9999999;
   for (i=0; str[i]!='\0' && i < max; i++) {
      if (str[i] == '\'' || str[i]=='\"')
         str[i] = '-';
   }
}

