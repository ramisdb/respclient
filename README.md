# RESP Client
## A simple lightweight C client to Redis or Ramis

The API consists of:

`connectRepServer()` does what is says. It returns a `RESPCLIENT *` That you'll use for the remainder of the conversation with the host, or `NULL` on error.

`reconnectRespServer()` will close and re-open a connection to the server. This function will reset all the `RESPCLIENT` buffers to their initial state as well. This function should be called if you suspect you might have become out of sync with the server's replies.

```C
// connect to the RESP Server
RESPCLIENT *connectRespServer(char *hostname,int port);
// closes and reopens the connection to the server and resets the buffers
int reconnectRespServer(RESPCLIENT *rcp);
```

`sendRespCommand()` is the primary metheod of sending commands to the server. It is is a `printf`-like command that sends RESP encoded commands to the server, and returns the server's reply in a pointer to a `RESPROTO` struct, or `NULL` if there was an error. Calling  `respClienError()` will return a string describing the error.

```c
// a formatted way to send data to the server
RESPROTO *  sendRespCommand(RESPCLIENT *rcp,char *fmt,...);
```

`sendRespCommand()`  encodes the contents of the `fmt` string and the following variable arguments in a manner similar to printf. So, `sendRespCommand(rcp,"SET %s %s","foo","bar");` will result in the following being transmitted to the server:
```ascii
*3\r\n
$3\r\n
set\r\n
$3\r\n
foo\r\n
$3\r\n
$bar\r\n
```
`sendRespCommand()` knows the following `%` escape codes:
```ascii
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
   
   binary objects aka "bulk strings":
   %b    Takes a pointer to a buffer and a size_t length as parameters
   
   Note: no other form of the above % codes may be used. i.e. %04d is invalid.
 ```
Whitespace within the format string only serves to delineate the separate command arguments and is not preserved. Contatenation is acceptable. `sendRespCommand(rcp,"set foo:%s baz","bar");` is equivalent to `set foo:bar baz`.

Usage of `%b` requires two arguments. The first is a pointer to a buffer of binary data that you wish to send and the second is a `size_t` indicating how many bytes are in that buffer. 

`sendRespCommand()` returns a `RESPROTO *` that contains the server's reply described below.

```
// gets a reply from the RESP server and parses it into items list within the RESPROTO struct
RESPROTO *  getRespReply(RESPCLIENT *rcp);
```

It is also possible to directly use `fprintf()` to send an ascii one line command to the server per the RESP protocol spec. Use `getRespReply(RESPCLIENT *rcp)` to parse the server's reply. Here's an example:

     for(int i=0;i<100;i++)
     {
         fprintf(respClient->fhToServer,"SET mykey%04d %d\n",i,i);
         fflush(respClient->fhToServer); // This is mandatory or you'll get out of sync with server
         response=getRespReply(respClient);
         // Do something with the reply here
     }
     
     













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

