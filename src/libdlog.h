/** 
* @file  libdlog.h
* Public API declarations for libdlog
*/

#ifndef LIBDLOG_H
#define LIBDLOG_H

/* call dlogInit once, giving it a short application name string; log
* entries will be marked with this name
*/
unsigned int dlogInit(char * applicationName);

/* call dlogFinalize once, after all transfers are completed */
unsigned int dlogFinalize();

/* call dlogBeginTransfer before an individual file transfer is started,
* with the transfer information in the arguments; the return value is
* the unique transfer ID, 0 if error; thread safe; xferFlag is DLOG_SEND 
* (0) or DLOG_RECEIVE (1). Any data you do not want to provide can be
* either constant strings like "unavailable", empty strings, but should
* not be null pointers. Hostnames can be "localhost" and even IP address
*  strings.
*/
unsigned long int dlogBeginTransfer(char* filename, unsigned long fileSize,
         unsigned long logUserID, char* srcHostName, char* targetPath, 
         char* targetHostName, unsigned int xferFlag, char* annotation);

/* call dlogEndTransfer after a file transfer is complete or aborted;
* the transferID must be the value returned from dlogBeginTransfer for
* this transfer; transferError is 0 if the transfer completed, any other
* application-specific code otherwise (will be logged uninterpreted)
*/
unsigned int dlogEndTransfer(unsigned long transferID,
                             unsigned long fileSize,
                             unsigned int transferError);

#define DLOG_SEND 0
#define DLOG_RECEIVE 1

#endif        //  #ifndef LIBDLOG_H

