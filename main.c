//
//  main.c
//  ramis_client
//
//  Created by P. B. Richards on 5/20/20.
//  Copyright © 2020 P. B. Richards. All rights reserved.
//



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


int main(int argc, const char * argv[])
{
  
  char *host="127.0.0.1";
  int port = 6379;
  RESPCLIENT *respClient;
  RESPROTO   *response;
 /*
   // got args?
	if (argc != 3)
	{
		printf("usage: %s hostname port (port is usually 6379)\n", argv[0]);
		return -1;
	}
   host=argv[1];
   
	// obtain port number
	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong port parameter: %s\n", argv[0],argv[2]);
		return -2;
	}
  */
  
   // Here's my fake blob
   char *blob="The quick brown fox jumped over the lazy dog";
   size_t blobSize=strlen(blob);
   

  
  respClient=connectRespServer(host,port);
  if(respClient)
  {
     response=sendRespCommand(respClient,"PING");
     printResponse(response);
     printErrors(respClient);
     
     response=sendRespCommand(respClient,"MULTI");
     printResponse(response);
     printErrors(respClient);

     
     response=sendRespCommand(respClient,"EXEC");
     printResponse(response);
     printErrors(respClient);

     // do something evil that will cause the server to block or mess things up
     fprintf(respClient->fhToServer,"*100\r\n");
     fflush(respClient->fhToServer);
     
     // this command will faail because of above
     response=sendRespCommand(respClient,"SET key%d this_value",5);
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
     
     // Since sendRespCommand can't do %04d lets talk using RESP's one line command method
     for(int i=0;i<100;i++)
     {
         fprintf(respClient->fhToServer,"SET mykey%04d %d\n",i,i);
         fflush(respClient->fhToServer); // This is mandatory or you'll get out of sync with server
         response=getRespReply(respClient);
         printResponse(response);
         printErrors(respClient);
     }
     response=sendRespCommand(respClient,"keys mykey*");
     if(response->nItems!=101) // 101 because of array declaration
       printf("ERROR: Counts dont match\n");
     
     response=sendRespCommand(respClient,"keys *");
     printResponse(response);
     printErrors(respClient);
  }
}
