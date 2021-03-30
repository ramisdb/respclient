//
//  client_example.c
//  ramis_client
//
//  Created by P. B. Richards on 5/20/20.
//  Copyright © 2020 P. B. Richards. All rights reserved.
//



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // this is only needed for sending news in another thread for main's example
#include <unistd.h>  // only needed for sleep
#include "ramis.h"
#include "resp_protocol.h"
#include "respClient.h"


/* 
   Replies from a command are in the form of an array of "items" within a RESPROTO
   struct. "nItems" identifies how many there are. Commands which return more than
   a single item (eg "keys *") will begin with an RESPISARRAY where item->length
   will indicate how many array members there are. The specification says arrays
   can be nested and there's support fot that, but I've not encountered and example.
   
   RESPISINT   means  int64_t
   
   RESPISFLOAT means  double  ( You will not see this from Redis, only Ramis )
   
   RESPISBULKSTR is a "binary large object" with item->length indicating
                       its size, and item->loc is a pointer to the blob
                       Redis sends most things as this type
                       
   RESPISSTR  means   "Simple String" i.e. a '\0 terminated string in item->loc
                       its length not including the '\0' is in item->length
   
   RESPISPLAINTXT     Another simple string type, but this one was encoded outside 
                      the protocol. Its length not including the '\0' is in item->length
 
                      This type will not be sent to the client
 
 
   RESPISERRORMSG     if this is not NULL, you've encountered and error. item->loc points to it.
*/

void
printResponse(RESPROTO *response)
{
  int i;
 
    if(response)
     {
       RESPITEM *item=response->items;
      
       for(i=0;i<response->nItems;i++,item++)
       {
         switch(item->respType)
         {
            case RESPISNULL:     printf("A NULL\n");break;
            
            case RESPISFLOAT:    printf("Floating Point:%lf\n",item->rfloat);break;
            
            case RESPISINT:      printf("Integer: %lld\n",item->rinteger);break;
            
            case RESPISARRAY:    printf("An array of %lld items\n",item->nItems);break;
            
            case RESPISBULKSTR:  printf("Bulk string (binary) of length %zd: ",item->length);
                                 fwrite(item->loc,1,item->length,stdout);
                                 printf("\n");
                                 break;
            
            case RESPISSTR:      printf("%s\n",item->loc);break;
            
            case RESPISPLAINTXT: printf("%s\n",item->loc);break;
            
            case RESPISERRORMSG: printf("Error message: %s\n",item->loc);break;
         }
       }
     }
    else printf("NULL response == Error\n");
}


/*
   RESPROTO *sendRespCommand(RESPCLIENT *rcp,char *fmt,...) is a printf-like command
   that sends RESP encoded commands to the server, and returns the server's reply
   in RESPROTO struct.
   
   e.g. "SET foo bar" would be transmitted as:
   *3\r\n
   $3\r\n
   set\r\n
   $3\r\n
   foo\r\n
   $3\r\n
   $bar\r\n
   
   Conversions it knows are:
   
   integers:
   %lld  long long
   %ld   long
   %d    int
 
   unsigned integers:
   %llu  unsigned long long
   %lu   unsigned long
   %u    unsigned
   
   floats: 
   %lf   double  Note: Both floats and doubles are transmitted with their full precision
   %f    float
   
   strings:
   %s    '\0' terminated strings that may contain any character (except '\0')
   
   binary objects aka "bulk strings"
   %b    Takes a pointer to a buffer and a size_t length as parameters
 
 
   
   Notes: The Redis server has no internal representation of integers or floating point values.
   In Redis they are stored in their ascii representation. Ramis knows these data types natively.
   
   While it is currently possible to prepend and append prefixes and suffixes to %b encoded data,
   it is not exactly a wise practice, and support may be dropped in the future:
   
   E.G. sendRespCommand(rcp,"set bar%bcue yummy","ribs",(size_t)4);  // may become deprecated
 

*/

void
printErrors(RESPCLIENT *rcp)
{
  char *error=respClienError(rcp);
  if(error)
   fprintf(stderr,"ERROR MESSAGE: %s",error);
}


void *sendNews(void *nothing)
{
  RESPCLIENT *respClient=connectRespServer("127.0.0.1",6379);
  nothing=nothing; // shut up a warning
  if(respClient)
  {
    sleep(5); // wait for second to ensure the SUBSCRIBE thread is ready waiting
    sendRespCommand(respClient,"PUBLISH RamisNews %s","Global warming is something we need to fix now!");
  }
 pthread_exit(NULL);
}

int newsThread()
{
   pthread_t newsThread;
   
   if(pthread_create(&newsThread, NULL,sendNews,NULL))
   {
       printf("ERROR: pthread_create for PUBLISH failed\n");
       exit(EXIT_FAILURE);
   }
  return(1);
}

 #include <sys/time.h>
double
stopwatch()
{
 static struct timeval Wastime;
 static struct timeval time;
 static int init=0;
 
 double now;
 if(!init)
 {
   gettimeofday(&Wastime, NULL);
   init=1;
   return(0.0);
 }
 gettimeofday(&time, NULL);
 now=time.tv_sec-Wastime.tv_sec;
 now+=((double)time.tv_usec / 1e6);
 
 
 return(now);
}



#define SZ 5
#define N  25000
#define P  //printResponse
#define THREADS 4
byte buffer[SZ];
void *testThread()
{
   RESPCLIENT *respClient=connectRespServer("127.0.0.1",6379);
   //respClientWaitForever(respClient,1);
  // byte *buffer=ramisMalloc(SZ);
  int i;
  buffer[1]='1';buffer[2]='2';buffer[3]='3';buffer[4]='\0';
  buffer[SZ-1]='#';
  
  if(respClient)
  {
      for(i=0;i<N;i++)
      {
          P(sendRespCommand(respClient,"SET SPEEDKEY%d %b",i,buffer,(size_t)SZ));
      }
      for(i=0;i<N;i++)
      {
         P(sendRespCommand(respClient,"GET SPEEDKEY%d",i));
      }
      for(i=0;i<N;i++)
      {
          P(sendRespCommand(respClient,"DEL SPEEDKEY%d",i));
      }

  }
 pthread_exit(NULL);
}


void test()
{
  pthread_t tid[THREADS];
  stopwatch();
  int i;
  for(i=0;i<THREADS;i++)
    pthread_create(&tid[i], NULL, testThread, NULL);
  for(i=0;i<THREADS;i++)
    pthread_join(tid[i], NULL);
   
  double elapsed=stopwatch();
  printf("SECS=%lf TPS=%lf\n",elapsed,(double)(N*THREADS*3)/elapsed);
  pthread_exit(NULL);
}

/*** ********************************************************************* **/
#include <math.h>
#define MAXVALTWEAKED 250e6
void
pftest()
{
  char buf[256];
  char bufa[80];
  FILE *fh=fopen("/Users/Cube/Downloads/bigdict.txt","r");
  int i;
  RESPROTO *response;
  RESPITEM *item;
  RESPCLIENT *rcp=connectRespServer("127.0.0.1",6379);
  respClientWaitForever(rcp,1);
   

  double estimate;
  double estimate2;
  uint64_t iEstimate;

  response=sendRespCommand(rcp,"DEL hll");
  response=sendRespCommand(rcp,"DEL hll2");

  printf("T,iE,E1,E2,Scale,Error,Error2\n");
  for(i=1;i<MAXVALTWEAKED;i++)
  {
   
    if(!fgets(bufa,80,fh))
    {
      fclose(fh);
      fgets(bufa,80,fh);
      fh=fopen("/Users/Cube/Downloads/bigdict.txt","r");
    }
    sprintf(buf,"%s %10X %d %.10lf ",bufa,i*7,i,(double)i/7);
   
    response=sendRespCommand(rcp,"PFADD hll %b",buf,(size_t)strlen(buf));
    

    for(char*p=buf;*p;p++)
      *p^=0x4f;
    response=sendRespCommand(rcp,"PFADD hll2 %b",buf,(size_t)strlen(buf));
 
    if(i<50000 || (i+1)%2000==1 )
    {
      response=sendRespCommand(rcp,"PFCOUNT hll %b",buf,(size_t)strlen(buf));
      estimate=(double)response->items[0].rinteger;
      response=sendRespCommand(rcp,"PFCOUNT hll2 %b",buf,(size_t)strlen(buf));
      estimate2=(double)response->items[0].rinteger;
      double scale=(double)i/estimate;
      double error=fabs(((double)i-estimate)/estimate);

      iEstimate=round(estimate);
 
      printf("%6d, %6lld, %10lf, %10lf, %10lf, %10lf, %10lf\n",i,iEstimate,estimate,estimate2,scale,error,fabs(((double)i-estimate2)/estimate2));
    }
  }

}

/* **************************************************************************** */




int main(int argc, const char * argv[])
{
  
  char *host="127.0.0.1";
  int port = 6379;
  RESPCLIENT *respClient;
  RESPROTO   *response;
  int i;
  

	if (argc == 3)
	{
      host=(char *)argv[1];
     	// obtain port number
      if (sscanf(argv[2], "%d", &port) <= 0)
      {
         fprintf(stderr, "%s: error: wrong port parameter: %s\n", argv[0],argv[2]);
         return -2;
      }
	}

 
    pftest(); // Uncomment this and the line below to test speed
   exit(0);

  
  
   // Here's my fake blob
   char *blob="The quick brown fox jumped over the lazy dog";
   size_t blobSize=strlen(blob);
   
   
  
  respClient=connectRespServer(host,port);
  respClientWaitForever(respClient,1);
  
  if(respClient)
  {
     response=sendRespCommand(respClient,"PING");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"SET a %lf%",22.0/7.0); // syntax error in %
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"SET a %f",22.0/7.0);
     printResponse(response);
     printErrors(respClient);
     
     if(newsThread())
     {
         respClientWaitForever(respClient,1); // we're going to use subscribe
         response=sendRespCommand(respClient,"SUBSCRIBE RamisNews");
         printResponse(response); // this is the response from the subscribe command
         printf("\nNow you're going to wait 5 seconds for your SUBSCRIBE\n ");
         response=getRespReply(respClient); // now we're waiting for news to be published
         printResponse(response);
        
         respClientWaitForever(respClient,0); // use normal poll() wait
         response=sendRespCommand(respClient,"UNSUBSCRIBE RamisNews");
         printResponse(response); // this is the response from the subscribe command
     }

     
     response=sendRespCommand(respClient,"MULTI");
     printResponse(response);
     printErrors(respClient);

     
     response=sendRespCommand(respClient,"EXEC");
     printResponse(response);
     printErrors(respClient);


     
     // disconnecting and reconnecting to the server here because of the evil above
     // It would be a safe practice to call this on errors. The RESP protocol has no re-sync ability
     if(reconnectRespServer(respClient))
         printf("We should be ok again\n");
     
     response=sendRespCommand(respClient,"GET key%d",5);
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"SET %s %s","ZZZ","I'm not sleeping");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"GET ZZZ");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"SET blobKey \"%b\"",blob,blobSize);
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"GET blobKey");
     printResponse(response);
     printErrors(respClient);
     
     
     // all of this is to just show how to use [ and (
     printResponse(sendRespCommand(respClient,"del myzset")); // removing from prior run
     printResponse(sendRespCommand(respClient,"ZADD myzset 0 aaaa 0 b 0 c 0 d 0 e"));
     printResponse(sendRespCommand(respClient,"ZADD myzset 0 foo 0 zap 0 zip 0 ALPHA 0 alpha"));
     printResponse(sendRespCommand(respClient,"ZRANGE myzset 0 -1"));
     // see https://redis.io/commands/zremrangebylex
     printResponse(sendRespCommand(respClient,"ZREMRANGEBYLEX myzset [%s (%s","alpha","omega"));
     printResponse(sendRespCommand(respClient,"ZRANGE myzset 0 -1"));

     // no such command will generate and error
      if(!sendRespCommand(respClient,"PONG"))
      {
         printErrors(respClient);
      }
     
     
     // another error
     response=sendRespCommand(respClient,"HELLO  world");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"HMSET me name %s age %d password %s","bart",60,"whackanoodle");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"HINCRBY me age %d",1);
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"HGETALL me");
     printResponse(response);
     printErrors(respClient);
     
     // Since sendRespCommand can't do %04d we have to to it in two stages
     for(i=0;i<100;i++)
     {
         char buf[11];
         sprintf(buf,"mykey%04d",i);
         response=sendRespCommand(respClient,"SET %s %d\n",buf,i);
         printResponse(response);
         printErrors(respClient);
     }
     
     for(i=0;i<10;i++)
     {
       char line[80];
       sprintf(line,"SET RAWWRITE%03d %s%03d\r\n",i,"HELLO-I-AM-RAW",i);
       write(respClient->socket,line,strlen(line));
       response=getRespReply(respClient);
       printResponse(response);
       printErrors(respClient);
     }
     
     response=sendRespCommand(respClient,"keys RAW*");
     printResponse(response);
     printErrors(respClient);

     
     response=sendRespCommand(respClient,"keys mykey*");
     if(response->nItems!=101) // 101 because of array declaration
       printf("ERROR: Counts dont match\n");
     else
       printf("keys mykey* got 101 like we expected\n");
     
     response=sendRespCommand(respClient,"keys *");
     printResponse(response);
     printErrors(respClient);
  }
  
  exit(EXIT_SUCCESS);
}
