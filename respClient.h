//
//  respClient.h
//  ramis_client
//
//  Created by Dr Cube on 5/20/20.
//  Copyright © 2020 P. B. Richards. All rights reserved.
//

#ifndef respClient_h
#define respClient_h
#include <stdarg.h>
#include "ramis.h"
#include "resp_protocol.h"

#define RESPCLIENTBUFSZ    8192 // Transmit and recieve buffer size
#define RESPCLIENTTIMEOUT     3 // Number of seconds to wait for a response
#define RESPMAXDIGITS        50 // Maximum number of ascii digits in a rendered number 

#define RESPCLIENT struct RespClientStruct
RESPCLIENT
{
  RESPROTO   *rppFrom;
  byte       *fromBuf;           // where we put junk from the server
  byte       *fromReadp;         // where the next read from server will go
  size_t      fromBufSize;       // how big is the buffer overall now
  FILE       *fhToServer;        // FILE * so we can buffer output (it's setbuf(IO_FBF) to RESPCLIENTBUFSZ)
  int         socket;            // the raw socket
  char       *hostname;          // these are kept from the initial open so we can reconnect
  int         port;
};

// https://stackoverflow.com/questions/5891221/variadic-macros-with-zero-arguments explains the ## below
//#define sendRespCommand(rcp,fmtString,...) {respPrintf((rcp)->rppTo,(rcp)->fhToServer,fmtString, ## __VA_ARGS__);}

// gets a reply from the RESP server and parses it into items list within the RESPROTO struct
RESPROTO *  getRespReply(RESPCLIENT *rcp);

// closes and reopens the connection to the server and resets the buffers
int reconnectRespServer(RESPCLIENT *rcp);

// connect to the RESP Server
RESPCLIENT *connectRespServer(char *hostname,int port);

// disconnect and free resources
RESPCLIENT *closeRespClient(RESPCLIENT *rcp);

// a formatted way to send data to the server
RESPROTO *  sendRespCommand(RESPCLIENT *rcp,char *fmt,...);

// Sees if anything went wrong. If everything's ok returns NULL , otherwise an error message.
char *respClienError(RESPCLIENT *rcp);

#endif /* respClient_h */
