/**
* @file publicapi.c
*
* This file provides the public API routines in libdlog. These are 
* dlogInit(), dlogFinalize(), dlogBeginTransfer(), and dlogEndTransfer().
* It also includes the library internal initializer/finalizer routines
* libDlogInitialize() and libDlogFinalize(); these are Gnu 
* constructor/destructor-attributed functions -- with other compilers
* they may need renamed to "_init" and "_fini".
*
* \mainpage
* Libdlog is designed to be used by multiple file transfer tools to log
* data about the file transfers that are executed using the tools. Simply
* include libdlog.h in your source files (with an include path), use the
* API functions, and link against the libdlog library.
*
* The API provides four simple functions to do this:
*  - dlogInit(): called once to initialize logging
*  - dlogBeginTransfer(): called when a file transfer begins
*  - dlogEndTransfer(): called when a file transfer ends
*  - dlogFinalize(): called once to complete the execution
* The pairs of dlogBeginTransfer() / dlogEndTransfer() calls are 
* associated with a unique transfer ID returned from dlogBeginTransfer().
* With this mechanism, parallel multi-threaded transfers are safe
* to log with libdlog.
*
* Libdlog has extensive configuration control. See the sample
* dlog.rc file provided in the distribution. Most logged values can
* be included, excluded, or hashed to anonymize information. Logging
* can be directed to a file, or to syslog. When libDlog initializes
* it searches for a config file in the following places in sequence:
* a file named by $DLOG_CONFIG, if the environment variable exists; a
* file named $HOME/.dlog.rc if the variable and file exist; or a file
* named /etc/dlog/dlog.rc. If all three are not available, libdlog will
* not be activated and will silently do nothing.
*
* Note that the transfer times calculated internally by libdlog are the
* time intervals between calls to the begin/end transfer routines. This
* is not the true transfer time because local buffering can allow the 
* transfer tool's I/O routines to finish before the network transfer is
* complete. To get actual transfer times would be VERY complicated
* internally, but one could enable logging both on the client and server
* sides, and match send and receive operations to get a more accurate
* network transfer time for each file.
*
* @author Mohamed Aturban, David Lerma, Jonathan Cook
* @date 04/20/2014
* @version 0.9c
*/

#include "private.c" // private functions are static and so need included

/**
* Called before each file transfer is started. It creates a record for
* this now-active transfer and keeps the initial information until
* completion. All string parameters should be given a non-null string 
* pointer, but these can be constant strings, empty or otherwise indicative
* (such as "not-provided"). An application does not need to provide all 
* data. All data is copied into private places, so parameter strings do not
* need kept unique for the library.
* @param filename Non-null string containing the full path of a file
*                 including its extension.
* @param size File size in bytes; if not known, use 0 and then specify size 
*             in dlogEndTransfer() call.
* @param userID User ID associated with the transfer
* @param sourceHostname Non-null string, source hostname or IP adress
* @param targetPath Non-null Destination full path filename
* @param targetHostname Non-null string, destination hostname or IP address
* @param xferType Type of transfer: DLOG_SEND is a send,
*                 DLOG_RECEIVE is a receive
* @param annotation is a user defined annotation/comment string (<128ch)
* @return A transfer ID > 0 to be used in the call to 
*         dlogEndTransfer(), or 0 if some error occurred
*/
unsigned long dlogBeginTransfer(char* filename, unsigned long size ,
                                unsigned long userID, char* sourceHostname,
                                char* targetPath, char* targetHostname,
                                unsigned int xferType, char* annotation)
{ 
   // logging record
   struct dlogLoggingData *logRecord=0;
   // JEC: get time of file transfer start
   //struct timeval startTval;
   // transfer ID and error flag
   unsigned long tid;
   int errorFlag = 0; 
   char md5buffer[24];
   
   // if logging is disabled, return
   if (logDoLogging == NO)
      return 0;

   // invalid xferType, so return (TODO: allow others)
   if ((xferType != DLOG_RECEIVE) && (xferType != DLOG_SEND))
      return 0;

   // invalid string pointer argument(s), so return
   if (!filename || !sourceHostname || !targetPath || !targetHostname)
      return 0;
   
   // Generate a new transfer-ID (must lock since a global var)
   pthread_mutex_lock( &generalMutex );
   tid = ++nextTransferID; 
   pthread_mutex_unlock( &generalMutex );

   // create new logging record -- make sure all zeroed w/ calloc()
   logRecord = (struct dlogLoggingData *) 
               calloc(1,sizeof(struct dlogLoggingData));
   if (!logRecord)
   {
      // memory allocation error! Skip everything else!
      return 0;
   }

   logRecord->id = tid; // assign transfer ID

   //
   // Get source file rootname, path, and extension
   //

   if (fileNameFormat != M_NO)
      getBaseFilename(filename, logRecord->fileName,
                      sizeof(logRecord->fileName));

   if (fileExtFormat != M_NO)
      getFilenameExtension(filename, logRecord->fileExt,
                     sizeof(logRecord->fileExt));

   if (sourcePathFormat != M_NO)
   {
      getPathFromFilename(filename, logRecord->sourceDir,
                      sizeof(logRecord->sourceDir));
      // if source dir is empty (not in filename), use CWD
      if (logRecord->sourceDir[0] == '\0')
      {
         getcwd(logRecord->sourceDir, sizeof(logRecord->sourceDir));
      }
   }

   /* md5/bitmask operations on source file components */
   if (fileNameFormat == M_MD5)
   {
      dlogMD5(logRecord->fileName, md5buffer);
      strncpy(logRecord->fileName, md5buffer,
              sizeof(logRecord->fileName));
      logRecord->fileName[sizeof(logRecord->fileName)-1] = '\0';
   }
   if (fileExtFormat == M_MD5)
   {
      dlogMD5(logRecord->fileExt, md5buffer);
      strncpy(logRecord->fileExt, md5buffer,
              sizeof(logRecord->fileExt));
      logRecord->fileExt[sizeof(logRecord->fileExt)-1] = '\0';
   }
   if (sourcePathFormat == M_MD5) 
   {
      dlogMD5(logRecord->sourceDir, md5buffer);
      strncpy(logRecord->sourceDir, md5buffer,
              sizeof(logRecord->sourceDir));
      logRecord->sourceDir[sizeof(logRecord->sourceDir)-1] = '\0';
   }

   // Clean source strings of any quote chars
   cleanString(logRecord->fileName, sizeof(logRecord->fileName));
   cleanString(logRecord->fileExt, sizeof(logRecord->fileExt));
   cleanString(logRecord->sourceDir, sizeof(logRecord->sourceDir));

   //
   // Create the target path data
   //

   if (targetPathFormat != M_NO)
   {
      strncpy(logRecord->targetDir, targetPath, 
              sizeof(logRecord->targetDir));
      logRecord->targetDir[sizeof(logRecord->targetDir)-1] = '\0';
   }

   if (targetPathFormat == M_MD5)
   {
      dlogMD5(logRecord->targetDir, md5buffer);
      strncpy(logRecord->targetDir, md5buffer, 
              sizeof(logRecord->targetDir));
      logRecord->targetDir[sizeof(logRecord->targetDir)-1] = '\0';
   }
   // Clean target string of any quote chars
   cleanString(logRecord->targetDir, sizeof(logRecord->targetDir));

   //
   // Get IP address data TODO: simplify with less processing
   //

   if (sourceIPFormat == R_YES)
   {
      findIPAddress(sourceHostname, logRecord->sourceIP,
                    sizeof(logRecord->sourceIP));
      doIPBitmask(logRecord->sourceIP, sourceIPMask,
                  sizeof(logRecord->sourceIP));
   } else if (sourceIPFormat == R_RAW)
   {
      strncpy(logRecord->sourceIP, sourceHostname, 
              sizeof(logRecord->sourceIP));
      logRecord->sourceIP[sizeof(logRecord->sourceIP)-1] = '\0';
   }
   cleanString(logRecord->sourceIP, sizeof(logRecord->sourceIP));

   if (targetIPFormat == R_YES)
   {
      findIPAddress(targetHostname, logRecord->targetIP,
                    sizeof(logRecord->targetIP));
      doIPBitmask(logRecord->targetIP, targetIPMask,
                  sizeof(logRecord->targetIP));
   } else if (targetIPFormat == R_RAW)
   {
      strncpy(logRecord->targetIP, targetHostname, 
              sizeof(logRecord->targetIP));
      logRecord->targetIP[sizeof(logRecord->targetIP)-1] = '\0';
   }
   cleanString(logRecord->targetIP, sizeof(logRecord->targetIP));

   // Annotation 
   if (logAnnotation == YES) {
      strncpy(logRecord->annotation, annotation,
              sizeof(logRecord->annotation));
      logRecord->annotation[sizeof(logRecord->annotation)-1] = '\0';
      // Clean annotation of any quote chars
      cleanString(logRecord->annotation, sizeof(logRecord->annotation));
   }
//------------------------------------------------------------

   logRecord->xferType = xferType;

   /* Now is time of file transfer start */
   gettimeofday(&(logRecord->startTval),0);

   if (userIDFormat != M_NO)
   {
      sprintf(logRecord->user,"%lu",userID);
      if (userIDFormat == M_MD5)
      {
         dlogMD5(logRecord->user, md5buffer);
         strncpy(logRecord->user, md5buffer, sizeof(logRecord->user));
         logRecord->user[sizeof(logRecord->user)-1] = '\0';
      }
      // JEC: impossible?!?
      //if ((userID >= ULONG_MAX) || (userID < 0))
      //   errorFlag = 1;
   }

   logRecord->size = size;
   // JEC: impossible?!?
   //if ((size >= ULONG_MAX) || (size < 0))
   //   errorFlag = 1;

   // record any errors if they happened (JEC: really?)
   logRecord->errorFlag = errorFlag;

   // Add transfer info to outstanding tranfers list (is mutexed internally)
   if (dlogAddLoggingData(&activeXferList, logRecord) != 0)
   {
      free(logRecord); // didn't get added for some reason?
      return 0;
   }

   // return transfer ID
   return tid;
}


/**
* Called when each file transfer completes. 
* This function calculates the transfer duration and then calls
* internal functions to record the logging data into the
* proper place (file or syslog).
* @param transferID  The integer ID that was produced by
* dlogBeginTransfer() function, so the begin and end of transfers
* can be related to each other.
* @param fileSize is size if not given in dlogBeginTransfer(), 0 otherwise
* @param transError  An integer value where 0 indicates transfer success
*   and any nonzero value indicates an error in the transfer; the library
*   will log the error code without interpretation
* @return  0 if data is recorded successfully or logging is disabled,
*          otherwise nonzero
*/
unsigned int dlogEndTransfer(unsigned long transferID,
                             unsigned long fileSize,
                             unsigned int transError)
{  
   static int numCalled=0;
   if (logDoLogging == NO)
      return 0;

   if (transferID == 0) // no transfer to end??
      return 1;

   /* JEC: get time of file transfer start */
   struct timeval logEndTval;
   struct dlogLoggingData *data;
 
   if ((data = dlogRemoveLoggingData(&activeXferList,transferID)) == 0)
   {
      // error in retrieving logging data
      return 2; 
   }

   // Get ending time of this transfer
   gettimeofday(&logEndTval, 0);
   data->endTval = logEndTval;

   // if size is given here, use it
   if (fileSize > data->size)
      data->size = fileSize;
   
   data->errorFlag += transError;

   // queue record for later logging
   dlogAddLoggingData(&endedXferList,data);
   numCalled++;
   if (numCalled % logBatchSize)
      writeLogData();
   return 0;
}

/**
* Called to initialize LibDLOG. It reads the config file which contains
* config variables that determine in which format data fields will 
* be recorded in the logging files. If this function returns a nonzero
* (error code), then logging will be disabled but the tool can still
* execute and call all of the library functions without generating more
* errors; the rest will silently do nothing.
* @param nameOfApp The name of the application using library; it should be 
*        a short string (e.g., "SCP") and will be included as the first
*        token in log records.
* @return  0 if libdlog initializes successfully, nonzero otherwise
*/
unsigned int dlogInit(char * nameOfApp)
{   

   pthread_mutex_lock( &generalMutex );

   if (alreadyInitialized == YES)
   {
      pthread_mutex_unlock( &generalMutex );
      return 1;
   }

   strncpy(appName, nameOfApp, MAXFILEPATH);
   appName[MAXFILEPATH-1] = '\0';
   
   activeXferList = endedXferList = NULL; 
   
   // reset nextTransferID
   nextTransferID = 0;
            
   if (setConfigFile() != 1)
   {
      logDoLogging = NO;
      return 2;
   }

   /* Create a unique session ID */
   time_t curtime = time(0);
   sessionID = ((curtime << 20) | getpid()) & 0xfffffffff;

   /* JEC: These are initialized upon creation
   fileNameFormat = 0; 
   fileExtFormat = 0; 
   sourcePathFormat = 0; 
   targetPathFormat = 0; 
   userIDFormat = 0;   
   sourceIPFormat = 0;    
   targetIPFormat = 0;
   // log the data using 'syslog' capability 1 yes 0 no
   loggingLocation = 0; 
   */
   strcpy(syslogIdent,"DLOG");
   
   FILE* configFilenamehandle;
   if (! (configFilenamehandle = fopen (configFilename,"rf")))
   {
      // Error : can't open file
      logDoLogging = NO;
      pthread_mutex_unlock( &generalMutex );
      return 3;
   }
   strcpy(dlogFilename, "/var/log/datalog.log"); // default value

   char buf[MAXLOGTOFILE], option[MAXFILEPATH],
        value[MAXFILEPATH];
   char *sp;
   // read conf file
   while (!feof(configFilenamehandle)) 
   {
      *buf = '\0';  
      fgets(buf, MAXLOGTOFILE-1, configFilenamehandle);
      if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
         // skip comment line or blank line
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
      else if (strcmp(option, "LogIdent") == 0)  //DJL libdlog 09/2012
      {
         strncpy(syslogIdent, value, MAXFILEPATH);
         syslogIdent[MAXFILEPATH-1] = '\0';
      }
      else if (strcmp(option, "LogOption") == 0)  //DJL libdlog 09/2012
      {
         char *tmpOption;
         int tmpInt = 0;
         tmpOption = strtok(sp+1," |\n");

         tmpInt = stringToNumber(tmpOption);
         if (tmpInt > 0 && tmpInt < 64)
         {
            tmpOption = NULL; // skip rest
         }

         while (tmpOption != NULL)
         {
             if (!strcmp("LOG_CONS",tmpOption))
                 tmpInt |= LOG_CONS;
             else if (!strcmp("LOG_NDELAY",tmpOption))
                 tmpInt |= LOG_NDELAY;
             else if (!strcmp("LOG_NOWAIT",tmpOption))
                 tmpInt |= LOG_NOWAIT;
             else if (!strcmp("LOG_ODELAY",tmpOption))
                 tmpInt |= LOG_ODELAY;
             else if (!strcmp("LOG_PERROR",tmpOption))
                 tmpInt |= LOG_PERROR;
             else if (!strcmp("LOG_PID",tmpOption))
                 tmpInt |= LOG_PID;
             else
                 goto FORMATERROR; //raise error

                 tmpOption = strtok(NULL," |\n");
         }

         if (tmpInt == 0)
            goto FORMATERROR; //raise error

         syslogOption = tmpInt;

      }
      else if (strcmp(option, "LogFacility") == 0) //DJL libdlog 09/2012
      {
         int tmpInt;
         tmpInt = stringToNumber(value);
         if (tmpInt >= 0 && tmpInt < 256) 
            syslogFacility = tmpInt;
         else if ((!strcmp("LOG_AUTH",value)) || (!strcmp("32",value)))
            syslogFacility = LOG_AUTH;
         else if ((!strcmp("LOG_AUTHPRIV",value)) || (!strcmp("80",value)))
            syslogFacility = LOG_AUTHPRIV;
         else if ((!strcmp("LOG_CRON",value)) || (!strcmp("72",value)))
            syslogFacility = LOG_CRON;
         else if ((!strcmp("LOG_DAEMON",value)) || (!strcmp("24",value)))
            syslogFacility = LOG_DAEMON;
         else if ((!strcmp("LOG_FTP",value)) || (!strcmp("88",value)))
            syslogFacility = LOG_FTP;
         else if ((!strcmp("LOG_KERN",value)) || (!strcmp("0",value)))
            syslogFacility = LOG_KERN;
         else if ((!strcmp("LOG_LOCAL0",value)) || (!strcmp("128",value)))
            syslogFacility = LOG_LOCAL0;
         else if ((!strcmp("LOG_LOCAL1",value)) || (!strcmp("136",value)))
            syslogFacility = LOG_LOCAL1;
         else if ((!strcmp("LOG_LOCAL2",value)) || (!strcmp("144",value)))
            syslogFacility = LOG_LOCAL2;
         else if ((!strcmp("LOG_LOCAL3",value)) || (!strcmp("152",value)))
            syslogFacility = LOG_LOCAL3;
         else if ((!strcmp("LOG_LOCAL4",value)) || (!strcmp("160",value)))
            syslogFacility = LOG_LOCAL4;
         else if ((!strcmp("LOG_LOCAL5",value)) || (!strcmp("168",value)))
            syslogFacility = LOG_LOCAL5;
         else if ((!strcmp("LOG_LOCAL6",value)) || (!strcmp("176",value)))
            syslogFacility = LOG_LOCAL6;
         else if ((!strcmp("LOG_LOCAL7",value)) || (!strcmp("184",value)))
            syslogFacility = LOG_LOCAL7;
         else if ((!strcmp("LOG_LPR",value)) || (!strcmp("48",value)))
            syslogFacility = LOG_LPR;
         else if ((!strcmp("LOG_MAIL",value)) || (!strcmp("16",value)))
            syslogFacility = LOG_MAIL;
         else if ((!strcmp("LOG_NEWS",value)) || (!strcmp("56",value)))
            syslogFacility = LOG_NEWS;
         else if ((!strcmp("LOG_SYSLOG",value)) || (!strcmp("40",value)))
            syslogFacility = LOG_SYSLOG;
         else if ((!strcmp("LOG_USER",value)) || (!strcmp("8",value)))
            syslogFacility = LOG_USER;
         else if ((!strcmp("LOG_UUCP",value)) || (!strcmp("64",value)))
            syslogFacility = LOG_UUCP;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option, "LogLevel") == 0)     //DJL libdlog 10/2012
      {

         int tmpInt;
         tmpInt = stringToNumber(value);
         if (tmpInt >= 0 && tmpInt <= 7)
            syslogLevel = tmpInt;
         else if (!strcmp("LOG_EMERG",value))
            syslogLevel |= LOG_EMERG;
         else if (!strcmp("LOG_ALERT",value))
            syslogLevel |= LOG_ALERT;
         else if (!strcmp("LOG_ERR",value))
            syslogLevel |= LOG_ERR;
         else if (!strcmp("LOG_WARNING",value))
            syslogLevel |= LOG_WARNING;
         else if (!strcmp("LOG_NOTICE",value))
            syslogLevel |= LOG_NOTICE;
         else if (!strcmp("LOG_INFO",value))
            syslogLevel |= LOG_INFO;
         else if (!strcmp("LOG_DEBUG",value))
            syslogLevel |= LOG_DEBUG;
         else
            goto FORMATERROR; //raise error      
      }

      else if (strcmp(option, "LogFilename") == 0)
      {
         strncpy(dlogFilename, value, MAXFILEPATH);
         dlogFilename[MAXFILEPATH-1] = '\0';
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
      else if (strcmp(option,"LogSourcename") == 0)
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
      else if (strcmp(option,"LogExtension") == 0)
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
      else if (strcmp(option,"LogSourcePath") == 0)
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
      else if (strcmp(option,"LogTargetPath") == 0)
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
      else if (strcmp(option,"LogUserID") == 0)
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
      else if (strcmp(option,"LogBatchSize") == 0)
      {  
         int tmpInt = stringToNumber(value);
         if (tmpInt > 0 && tmpInt < 256)
            logBatchSize = (unsigned) tmpInt;
         else
            goto FORMATERROR; //raise error
      }
      else if (strcmp(option,"LogSourceIP") == 0)
      {
         if (!strcmp("yes",value))
         {
            sourceIPFormat = R_YES;
            sourceIPMask = 0xffffffff;
         } else if (!strcmp("no",value))
         {
            sourceIPFormat = R_NO;
         } else if (!strcmp("raw",value))
         {
            sourceIPFormat = R_RAW;
            sourceIPMask = 0xffffffff;
         } else {
            unsigned int d1,d2,d3,d4;
            if (sscanf(value,"%u.%u.%u.%u",&d1,&d2,&d3,&d4) != 4)
               goto FORMATERROR; //raise error
            sourceIPFormat = M_YES;
            sourceIPMask = (d1<<24) | (d2<<16) | (d3<<8) | (d4);
         }
      }
      else if (strcmp(option,"LogTargetIP") == 0)
      {  
         if (!strcmp("yes",value))
         {
            targetIPFormat = R_YES;
            targetIPMask = 0xffffffff;
         } else if (!strcmp("no",value))
         {
            targetIPFormat = R_NO;
         } else if (!strcmp("raw",value))
         {
            targetIPFormat = R_RAW;
            targetIPMask = 0xffffffff;
         } else {
            unsigned int d1,d2,d3,d4;
            if (sscanf(value,"%u.%u.%u.%u",&d1,&d2,&d3,&d4) != 4)
               goto FORMATERROR; //raise error
            targetIPFormat = M_YES;
            targetIPMask = (d1<<24) | (d2<<16) | (d3<<8) | (d4);
         }
      }
   } 
   fclose(configFilenamehandle);  
   alreadyInitialized = YES;
   pthread_mutex_unlock( &generalMutex );
   return 0;

FORMATERROR:
   fclose(configFilenamehandle);  
   alreadyInitialized = YES;
   logDoLogging = NO;
   //fprintf(stderr,"Error in DLOG configuration file format, line (%s)", buf);
   //fprintf(stderr," -- no logging will be done.\n");
   pthread_mutex_unlock( &generalMutex );
   return 4;
}

/**
* Called to end the file transfer session. Does nothing unless
* there are outstanding transfer records (dlogBeginTransfer()
* was called without a corresponding dlogEndTransfer()). If so,
* this will log the outstanding transfer records as transfer errors.
* @return 0 on sucess, nonzero if any errors
*/
unsigned int dlogFinalize()
{ 
   struct dlogLoggingData *cur;
   unsigned long tids[500];
   int i,n;

   // first log any actual ended-but=notlogged entries
   writeLogData();

   // Process up to 500 outstanding transfers at a time, then
   // check if more are left; this is done to ensure no linked
   // list weirdness, because dlogEndTransfer will delete items
   // from the linked list, so our walk through it could break.
   n = 1;
   while (n)  // if n==0, didn't process any last time, but loop
   {          // should really end at the break, just being safe here
      cur = activeXferList; // restart on current list
      if (!cur) break;
      n = 0;
      while (cur && n < 500)
      {
         tids[n++] = cur->id;
         cur = cur->next;
      }
      cur = 0; // for safety
      for (i=0; i < n; i++)
      {
         // now call dlogEndTransfer for the found records
         dlogEndTransfer(tids[i], 0, 1); // record transfer as error
      }
   }
   // now log error entries
   writeLogData();
   return 0;
} 


// Gnu attribute declarations (on the prototype so that they
// don't mess up Doxygen)
void libdlogInitialize (void)  __attribute__((constructor));
void libdlogFinalize (void)  __attribute__((destructor));

/**
* Library constructor: nothing to do for now.
* Uses gcc attribute syntax, other compilers may need something else
* or they might need the function to be named "_init".
* @return nothing
*/
void libdlogInitialize (void)
{
   //printf ("\nBegin Libdlog Attribute\n");
   // Nothing to do here for now, maybe someday
}

/**
* Library finalizer: called after the file transfer session ends. 
* We just call dlogFinalize() in case the app failed to call it; this
* records any outstanding (i.e., failed) transfers.
* Uses gcc attribute syntax, other compilers may need something else
* or they might need the function to be named "_fini".
* @return nothing
*/
void libdlogFinalize (void)
{
   dlogFinalize();
}

/**
* Utility function to get IP address strings from a socket descriptor.
* If your app has an available socket descriptor, give it to this 
* function and it will create the IP address strings for the endpoints,
* which you can then use in your dlogBeginTransfer() calls.
* @param sock is the socket descriptor
* @param localIP is an array for the local IP address string (min 16 bytes)
* @param remoteIP is array for the remote IP address string (min 16 bytes)
* @return nothing
*/
void dlogGetSocketIPs(int sock, char *localIP, char *remoteIP)
{
   struct sockaddr_in saddr;
   unsigned int alen = sizeof(saddr);
   getsockname(sock, (struct sockaddr * __restrict__) &saddr,&alen);
   inet_ntop( AF_INET, &(saddr.sin_addr.s_addr), localIP, INET_ADDRSTRLEN);
   memset(&saddr,0,sizeof(saddr));
   getpeername(sock, (struct sockaddr * __restrict__) &saddr,&alen);
   inet_ntop( AF_INET, &(saddr.sin_addr.s_addr), remoteIP, INET_ADDRSTRLEN);
}

