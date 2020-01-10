/**
* @file libdlogtest.c
*
* This program is to test "libdlog" library with many threads
*
* @author Mohamed Aturban, Jonathan Cook
*
* @date 04/20/2014
* @version 0.9c
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <libdlog.h>
#include <string.h>
//#include <dmalloc.h>

#define MAX_THREAD 10000
#define REC_PER_THREAD 100

void *dlogtester(void *arg);
int checkDataAfterTrans(int numThreads);

//
// Main: Init and launch testing threads
//
int main(int argc, char* argv[]) 
{
   int stat;
   char ebuf[1024];
   //getcwd(ebuf, 1024);
   
   if (--argc < 1) {
      fprintf(stderr,"Usage: %s <libdlog-config-file> [num-threads]\n",argv[0]);
      return 1;
   }
   
   // hardcode an environment variable to force a local config read
   strcpy(ebuf,argv[1]);
   setenv("DLOG_CONFIG",ebuf,1);
   //printf("DLOG_CONFIG is (%s)\n",getenv("DLOG_CONFIG"));
   
   if ((stat=dlogInit("TestProgram")) != 0)
   {
      fprintf(stderr,"Error %d in dlogInit() \n", stat);
   }
   
   //mtrace(); // JEC: what is this here for? not sure, might
   //             have been a dmalloc thing, or profiling thing

   int numThreads=20;
   pthread_t *threads;

   if (argc == 2) 
   {
      numThreads = atoi(argv[2]);
   }
   
   if ((numThreads < 1) || (numThreads > MAX_THREAD)) 
   {
      fprintf(stderr, "Error: # of threads should be 1 - %d.\n",MAX_THREAD);
      return 2;
   }
   
   // delete previous logging file
   remove("./dlogxfer.log");

   threads = (pthread_t *) malloc(numThreads*sizeof(*threads));

   printf("Starting %d logging threads...\n",numThreads);

   /* Assign args to a struct and start thread */
   int i;
   for (i=1; i <= numThreads; i++) 
   {
      pthread_create(&threads[i],NULL,dlogtester,(void *)(i));
   }

   sleep(1);
   printf("Wait for threads...\n");

   /* Wait for all threads. */
   int *x = malloc(sizeof(int));
   for (i=1; i <= numThreads; i++) 
   {
      pthread_join(threads[i],(void*)x);
   }

   dlogFinalize();
   sleep(1);
   
   // check if data is transferred correctly ...
   checkDataAfterTrans(numThreads);

   //fprintf(stderr,"Ending sleep\n");
   //sleep(3);
   //dmalloc_shutdown();
   return 0;
}

//
// Thread function: param is Thread ID
//
void *dlogtester(void *arg)
{
   int tid;
   tid=(int) arg; // must do cast because of pthread_init interface
   int i,s;

   //printf("  thread %d\n",tid);

   srand ( time(NULL) );

   for (i = 0; i < REC_PER_THREAD; i++)
   {
      // generating data 
      int fsize = rand() % 100000 + 1;
      char name[1000];
      char targPath[1000];
      // create various file and path names for recording
      sprintf(name,"/dir%d/file%d.e%d",tid,  i,  i); 
      sprintf(targPath,"/targDir%d/subdir%d/",tid,  i);
      int id=dlogBeginTransfer(name, fsize, tid*REC_PER_THREAD+i, "localhost",
                               targPath, "135.65.74.31", 0,
                               "my comment");
      if (id == 0)
      {
         printf(" Error in 'dlogBeginTransfer' function\n");
      }

      usleep(10000);

      if (i < REC_PER_THREAD-2)
      // cause the last 2-3 to be left unfinished -- see if finalizer works
      // TODO: test file size here, too
      {
         if ( (s=dlogEndTransfer(id, 0, ((id % 5) == 0) ? 2 : 0)) != 0)
         {
            printf(" Error %d in 'dlogEndTransfer' function\n",s);
         }
      }
   }

   return (void*)(1);
}


//
// Code to check if log file is correct? Does this work?
//
int checkDataAfterTrans(int numThreads)
{  
   int ids[(MAX_THREAD+1)*REC_PER_THREAD];
   int i,j,k;
   FILE* logfh;
   char buf[5000];
   char *up; int transID;

   for (i=0; i < (MAX_THREAD+1)*REC_PER_THREAD; i++)
      ids[i] = 0; 
  
   char *xferconf="./dlogxfer.log";
   
   printf("Checking log file for TIDs...\n");

   if (! (logfh = fopen (xferconf,"r")))
   {
      printf("Error : can't open '%s' file\n",xferconf);
      return 0;
   }

   // start reading from logging file
   while (fgets(buf, sizeof(buf), logfh) != 0) 
   {  
      up = strstr(buf,"user=");
      if (up) {
         transID = strtol(up+6,0,10);
         if (transID >= 0 && transID < (MAX_THREAD+1)*REC_PER_THREAD)
            ids[transID]++;
      }      
   } 
   fclose(logfh);
   //printf("Last buf={%s}\n",buf);
   //printf("Last TID=%d\n",transID);
   
   for (i=1; i <= numThreads; i++) {
      for (j=0; j < REC_PER_THREAD; j++) {
         k = i*REC_PER_THREAD + j;
         if (ids[k] != 1) { 
            printf("TID %d (%d,%d:%d) not found in file ...\n",k,i,j,ids[k]);
         }
      }
   }
   
   printf("Finished checking log file\n");   
   return 0;    
}

