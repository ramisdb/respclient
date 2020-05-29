//
//  respClient.c
//  ramis_client
//
//  Created by Dr Cube on 5/20/20.
//  Copyright © 2020 P. B. Richards. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <stdarg.h>
#include <poll.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "ramis.h"
#include "resp_protocol.h"
#include "respClient.h"


/*

#define RESPCLIENTBUFSZ 1000000 // Transmit and recieve buffer size

#define RESPCLIENT struct RespClientStruct
RESPCLIENT
{
  RESPROTO   *rppTo;             // resp protocol handler to server
  RESPROTO   *rppFrom;
  byte       *fromBuf;           // where we put junk from the server
  byte       *fromReadp;         // where the next read from server will go
  size_t      fromBufSize;       // how big is the buffer overall now
  size_t      fromBufRemaining;  // bytes left available to read into
  FILE       *fhToServer;
  int        socket;
};

*/

RESPCLIENT *
closeRespClient(RESPCLIENT *rcp)
{
  if(rcp)
  {    
      if(rcp->rppFrom)
         freeRespProto(rcp->rppFrom);
     
      if(rcp->fhToServer)  // this should also close the underlying fds accoring to man(3)
         fclose(rcp->fhToServer);
     
      if(!rcp->fromBuf)
         ramisFree(rcp->fromBuf);

      ramisFree(rcp);
  }
 return(NULL);
}

static
RESPCLIENT *
newRespClient()
{
  RESPCLIENT *rcp=ramisCalloc(1,sizeof(RESPCLIENT));
   if(!rcp)
   {
      fprintf(stderr,"Malloc error in client\n");
      exit(EXIT_FAILURE);
   }
   else
   {
     rcp->rppFrom=newResProto(0); // 0 indicating the parser is not server parsing
     rcp->fromBuf=ramisMalloc(RESPCLIENTBUFSZ);

     if(!rcp->rppFrom || !rcp->fromBuf)
         return(closeRespClient(rcp));
     
     rcp->fromBufSize=RESPCLIENTBUFSZ;
     rcp->fromReadp=rcp->fromBuf;
     
     rcp->socket=-1;
   }
  return(rcp);
}


static int
openRespClientSocket(RESPCLIENT *rcp)
{
 	struct sockaddr_in   address;
	struct hostent       *host;
   	// create socket
	rcp->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rcp->socket<= 0)
	{
		rcp->rppFrom->errorMsg="respClient error: cannot create socket";
		return(RAMISFAIL);
	}

	// connect to server
	address.sin_family = AF_INET;
	address.sin_port = htons(rcp->port);
	host = gethostbyname(rcp->hostname);
	if (!host)
	{
		rcp->rppFrom->errorMsg="respClient error: unknown host";
		return(RAMISFAIL);
	}
	
   memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
	if (connect(rcp->socket, (struct sockaddr *)&address, sizeof(address)))
	{
		rcp->rppFrom->errorMsg="respClient error: cannont connect to host";
		return(RAMISFAIL);
	}

   rcp->fhToServer=fdopen(rcp->socket,"w");
   if(!rcp->fhToServer)
	{
		rcp->rppFrom->errorMsg="respClient error: fdopen socket failed";
		return(RAMISFAIL);
	}
   setvbuf(rcp->fhToServer,NULL,_IOFBF,RESPCLIENTBUFSZ);
   
 return(RAMISOK);
}

// closes and reopens the connection to the server and resets the buffers
int
reconnectRespServer(RESPCLIENT *rcp)
{
  if(rcp->fhToServer)
    fclose(rcp->fhToServer);
  rcp->fromReadp=rcp->fromBuf;
  return(openRespClientSocket(rcp));
}

// Creates a new RESPCLIENT handle and connects to the server
RESPCLIENT *
connectRespServer(char *hostname,int port)
{
   
	RESPCLIENT *rcp=newRespClient();
   
   if(!rcp)
      return(rcp);
   
   rcp->hostname=hostname;
   rcp->port=port;
   
   if(!openRespClientSocket(rcp))
      return(closeRespClient(rcp));

	return(rcp);
}



//  Polls the socket waiting for data if Timout
//  https://man.openbsd.org/poll.2
static int
waitForRespData(RESPCLIENT *rcp)
{
  struct pollfd pfd;
  int ret;
  
  pfd.fd=rcp->socket;
  pfd.events=POLLIN;
  ret=poll(&pfd,1,1000*RESPCLIENTTIMEOUT);
  
  if(ret==-1)
  {
    rcp->rppFrom->errorMsg="poll() Error on read from server";
    if(rcp->fhToServer)
      fclose(rcp->fhToServer);

    openRespClientSocket(rcp); // attempt reconnect
    return(0);
  }
  else
  if(!ret)
  {// In this case we probably did something stupid and need to reopen it to prevent corruption
    rcp->rppFrom->errorMsg="Timeout reading from server";
    if(rcp->fhToServer)
       fclose(rcp->fhToServer);

    openRespClientSocket(rcp); // attempt reconnect
    return(0);
  }
  return(1);
}


RESPROTO *
getRespReply(RESPCLIENT *rcp)
{
  ssize_t nread;
  int    parseRet;
  int    newBuffer=1;
  size_t totalRead=0;
  size_t bufAvailable=rcp->fromBufSize;

  
  rcp->fromReadp=rcp->fromBuf; // re-init read pointer
  
  do
  {
       if(!waitForRespData(rcp))
         return(NULL);
       
       nread=read(rcp->socket,rcp->fromReadp,bufAvailable);
       if(nread<=0)     // server closed or error
         return(NULL);
     
       totalRead+=nread;
     
       if(nread==bufAvailable)
       {
         rcp->fromBuf=respBufRealloc(rcp->rppFrom,rcp->fromBuf,rcp->fromBufSize+RESPCLIENTBUFSZ); // increase buffer size
         if(!rcp->fromBuf)
         {
            rcp->rppFrom->errorMsg="Could not expand recieve buffer in getRespReply()";
            return(NULL);
         }
         rcp->fromReadp=rcp->fromBuf+totalRead;
         rcp->fromBufSize=rcp->fromBufSize+RESPCLIENTBUFSZ;
         bufAvailable=rcp->fromBufSize-totalRead;
       }
       else rcp->fromReadp+=nread;

       parseRet=parseResProto(rcp->rppFrom,rcp->fromBuf,totalRead,newBuffer);
     
       if(parseRet==RESP_PARSE_ERROR)
         return(NULL);
     
       newBuffer=0;
       
  } while(parseRet==RESP_PARSE_INCOMPLETE);
  return(rcp->rppFrom);
}



// how many individual items are in the format string
static int
countRespCommandItems(char *s)
{
  int count=0;
  while(*s)
  {
    while(isspace(*s)) ++s;
    if(*s)
    {
      ++count;
      while(*s && !isspace(*s)) ++s;
    }

  }
   return(count);
}

// checks to see if neededSize will fit in the buffer , if not, reallocates it
static int
makeItFit(size_t neededSize,char **bufferaddy,char **pbufp,size_t *pbufsz)
{
  if(*pbufp+neededSize<*bufferaddy+*pbufsz)
    return(RAMISOK);
  
  char *newbuffer;
  char *bufp=*pbufp;
  char *oldbuffer=*bufferaddy;
  size_t oldSize=*pbufsz;
   
  neededSize=neededSize+oldSize+RESPCLIENTBUFSZ; // lets make it fit for sure
  newbuffer=ramisRealloc(oldbuffer,neededSize);
  if(!newbuffer)
      return(RAMISFAIL);
  
  *bufferaddy=newbuffer;
  *pbufp=newbuffer+(bufp-oldbuffer); // realloc probably moved us
  *pbufsz=neededSize;
  return(RAMISOK);
}

#define makeitFitOrElse(x)  if(!makeItFit((x),&outBuffer,&bufp,&outBufSz)) goto bufferFail


// RESP encodes parameters in a printf kind of way and sends them to the server
// returns the server's reply in the form of a list of items in RESPROTO
RESPROTO *
sendRespCommand(RESPCLIENT *rcp,char *fmt,...)
{
  FILE *fh=rcp->fhToServer;
  char    *p,*q,t;
  size_t  thisLen;
  char   *outBuffer=ramisMalloc(RESPCLIENTBUFSZ);
  size_t  outBufSz=RESPCLIENTBUFSZ;
  char   *bufp;
  char   *fmtCopy=strdup(fmt);
  
// char   *nullBulkString="$-1\r\n"; PBR WTF I have not yet implemented NULL transmission
  
  if(!outBuffer || !fmtCopy)
  {
     bufferFail:
     if(outBuffer)
         ramisFree(outBuffer);
     if(fmtCopy)
         ramisFree(fmtCopy);
     rcp->rppFrom->errorMsg="Malloc error in sendRespCommand";
     return(NULL);
  }
  
  va_list arg;
  
  rcp->rppFrom->errorMsg=NULL;
  fprintf(fh,"*%d\r\n",countRespCommandItems(fmt));
  
  va_start(arg,fmt);
  for(p=fmtCopy;*p;)
  {
    while(isspace(*p)) ++p;

    for(q=p;*q && !isspace(*q);q++); // find the end of this sequence and terminate it
    t=*q;  // save whatever's pointed to by q
    *q='\0';
    
    bufp=outBuffer;
    while(*p)
    {
      if(*p=='%')
      {
        ++p;
        if(*p=='%') // %% == %
        {
          makeitFitOrElse(1);
          *bufp++=*p;
        }
       else if(*p=='s')
        {
          char *thisArg=va_arg(arg,char *);
          size_t len=strlen(thisArg);
          makeitFitOrElse(len);
          strncpy(bufp,thisArg,len);
          bufp+=len;
          ++p;
        }
       else if(*p=='b') // PBR WTF %b shouldn't do the buffer copy but it prevents user errors like "bar%bfoo"
       {
          byte *thisArg=va_arg(arg,byte *);
          size_t len=va_arg(arg,size_t);
          makeitFitOrElse(len);
          memcpy(bufp,thisArg,len);
          bufp+=len;
          ++p;
       }
       else if(*p=='d')
       {
         int thisArg=va_arg(arg,int);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%d",thisArg);
         bufp+=strlen(bufp);
         ++p;
       }
       else if(*p=='f') // render to the precision of a float
       {
         float thisArg=va_arg(arg,double);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,":%#.*e\r\n",FLT_DECIMAL_DIG-1,thisArg);
         bufp+=strlen(bufp);
         ++p;
       }
       else if(*p=='d') // render to the precision of a double
       {
         float thisArg=va_arg(arg,double);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,":%#.*e\r\n",DBL_DECIMAL_DIG-1,thisArg);
         bufp+=strlen(bufp);
         ++p;
       }
       else if(!strncmp(p,"ld",2))
       {
         long thisArg=va_arg(arg,long);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%ld",thisArg);
         bufp+=strlen(bufp);
         p+=2;
       }
       else if(!strncmp(p,"lld",3))
       {
         long long thisArg=va_arg(arg,long long);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%lld",thisArg);
         bufp+=strlen(bufp);
         p+=3;
       }
       else if(!strncmp(p,"u",3))
       {
         unsigned thisArg=va_arg(arg,unsigned);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%u",thisArg);
         bufp+=strlen(bufp);
         p+=3;
       }
       else if(!strncmp(p,"lu",2))
       {
         unsigned long thisArg=va_arg(arg,unsigned long);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%lu",thisArg);
         bufp+=strlen(bufp);
         p+=2;
       }
       else if(!strncmp(p,"llu",3))
       {
         unsigned long long thisArg=va_arg(arg,unsigned long long);
         makeitFitOrElse(RESPMAXDIGITS);
         sprintf(bufp,"%llu",thisArg);
         bufp+=strlen(bufp);
         p+=3;
       }
       else
       {
         rcp->rppFrom->errorMsg="Invalid % code in sendRespCommand()";
         return(NULL);
       }
      }
      else
      {
        makeitFitOrElse(1);
        *bufp=*p;
        ++bufp;
        ++p;
      }
    }
    thisLen=bufp-outBuffer; // the sum length of the data payload
    makeitFitOrElse(2);
    *bufp++='\r';
    *bufp++='\n';
    fprintf(rcp->fhToServer,"$%zu\r\n",thisLen);
    fwrite(outBuffer,bufp-outBuffer,1,rcp->fhToServer);
    bufp=outBuffer;
   *q=t; // put the saved character back and keep going
  }
  va_end(arg);
  
  if(fmtCopy)
   ramisFree(fmtCopy);
  if(outBuffer)
   ramisFree(outBuffer);
   
  if(fflush(rcp->fhToServer)!=0)
  {
     rcp->rppFrom->errorMsg="Could not send data to server";
     return(NULL);
  }
  return(getRespReply(rcp)); // everything was fine so far, so return the reply from the server 
}


// Sees if anything went wrong. If everything's ok returns NULL , otherwise an error message.
char *
respClienError(RESPCLIENT *rcp)
{
  RESPROTO *rpp=rcp->rppFrom;
  
  if(rpp->errorMsg!=NULL)
    return(rpp->errorMsg);
 
  if(rpp->nItems && rpp->items[0].respType==RESPISERRORMSG)
    return((char *)rpp->items[0].loc);
  
  return(NULL);
}
