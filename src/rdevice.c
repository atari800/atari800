/* Originally From Closer To Home (Tom Hunt) */
// 2003.04.03 cmartin@ti.com - Added connecting Host name lookup and port changing as well as CONNECT_STRING changing...
// 2003.04.03 cmartin@ti.com - Added setsockopt SO_REUSEADDR for socket to allow reconnections
// 2003.04.05 cmartin@ti.com - Added sighandler for catching SIGPIPE error...can get this and cause the emu to crash if the other end disconnects prematurely...
// 2003.04.06 cmartin@ti.com - Added translation on/off and line feeds
//                           - Fixed IP address lookup (print ip address if lookup fails...)
// 2003.04.07 cmartin@ti.com - Added Local echo when not connected.
// 2003.04.17 cmartin@ti.com - Added Telnet escape sequence parsing for connecting to telnet

// TODO: Add serial reads/write to tty
//       create a new socket and a buffer for each port number....
//       non-concurrent mode
// Dialing out via telnet:
// Type "ATDL" to toggle Linefeeds on/off (normally off)
// Type "ATDI <hostname> <port>" example: "ATDI localhost 23"
// if the port is omitted, then 23 is assumed.

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <signal.h>
#include    <ctype.h>
#include    <time.h>
#include    <errno.h>
#include    <unistd.h>



#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>


#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "log.h"

#define Peek(a)    (dGetByte((a)))
#define DPeek(a)   (dGetByte((a))+( dGetByte((a)+1)<<8 ) )
#define Poke(x,y)  (dPutByte((x),(y)))

static int connected;
static int do_once;
int my_once = 1;

extern int Device_isvalid(UBYTE ch);
/**********************************************/
/*             R: stuff                       */
/**********************************************/
UBYTE r_dtr, r_rts, r_sd;
UBYTE r_dsr, r_cts, r_cd;
UBYTE r_parity, r_stop;
UBYTE r_error, r_in;
unsigned int r_stat;
unsigned int svainit;




struct sockaddr_in in;
struct sockaddr_in peer_in;
int sock;
char MESSAGE[256];
char CONNECT_STRING[40] = "\r\n_CONNECT 2400\r\n";
int newsock;
int len;
char command_buf[256];
int command_end = 0;
fd_set fd;
struct timeval tv;
int retval;
unsigned char one;
unsigned char bufout[256];
//unsigned char bufin[256];
unsigned char bufend = 0;
int bytesread;
int concurrent;
static char inetaddress[256];
int translation = 1;
int trans_cr = 0;
int linefeeds = 1;


void catch_disconnect(int sig)
{
  Aprint("R*: Disconnected....");
  close ( newsock );
  connected = 0;
  do_once = 0;
  bufout[0] = 0;
  strcat(bufout, "\r\nNO CARRIER\r\n");
  bufend = 13;
  Poke(747,bufend);
  Poke(748,0);

}

void xio_34(void)
{
  int temp;
  int fid;

/* Controls handshake lines DTR, RTS, SD */

  fid = dGetByte(0x2e) >> 4;
  temp = dGetByte(ICAX1Z);

  if(temp & 0x80)
  {
    if(temp & 0x40)
    {
      /* turn DTR on */
    }
    else
    {
      if(connected != 0)
      {
        close ( newsock );
        connected = 0;
        do_once = 0;
        bufend = 0;
      }
    }
  }
  regA = 1;
  regY = 1;
  ClrN;

  Poke(747,0);
}

void xio_36(void)
{
/* Sets baud, stop bits, and ready monitoring. */

  r_cd = dGetByte(ICAX2Z);


  regA = 1;
  regY = 1;
  ClrN;
  Poke(747,0);

}



void xio_38(void)
{
/* Translation and parity */
  int r_in;
  regA = 1;
  regY = 1;
  ClrN;
  Poke(747,0);
  
  r_in = Peek(ICAX1Z);

  if(r_in & 0x04) {
    //Odd Parity
  }

  if(r_in & 0x20) {
    // No Translation
    Aprint("R*: No ATASCII/ASCII TRANSLATION");
    translation = 0;
  }

  if(r_in & 0x40) {
    // Append line feed
    Aprint("R*: Append Line Feeds");
    linefeeds = 1;
  }
  
}

void xio_40(void)
{

/* Sets concurrent mode.  Also checks for dropped carrier. */
  int r_in;
/*
    if(connected == 0)
      Poke(747,0);
*/
  regA = 1;
  regY = 1;
  ClrN;

  r_in = Peek(ICAX1Z);


  if(r_in >= 12) {
    Poke(747,0);
    concurrent = 1;
    Aprint("R*: Entering concurrent IO mode...");
  } else {
    concurrent = 0;
    sprintf(MESSAGE, "R*: XIO 40, %d", r_in);
    Aprint(MESSAGE);
  
  }

}

void open_connection(char * address, int port) 
{
  struct hostent *host;
    if((address != NULL) && (strlen(address) > 0)) {

      close(newsock);
      close(sock);
      do_once = 1;
      connected = 1;
      memset ( &peer_in, 0, sizeof ( struct sockaddr_in ) );
      //newsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
      newsock = socket ( AF_INET, SOCK_STREAM, 0 );
      peer_in.sin_family = AF_INET;
      inet_pton(AF_INET, inetaddress, &peer_in.sin_addr);
      if(peer_in.sin_addr.s_addr == -1) {
        host = gethostbyname(inetaddress);
        if(host != NULL) {
           //peer_in.sin_addr  = (struct in_addr *) *host->h_addr_list;
        }
      }
      if(port > 0) {
        peer_in.sin_port = htons (port);
      } else {  /* telnet port */
        peer_in.sin_port = htons (23);
      }
      if(connect(newsock, (struct sockaddr *)&peer_in, sizeof ( peer_in )) < 0) {
        perror("connect");
      }
      signal(SIGPIPE, catch_disconnect); //Need to see if the other end disconnects...
      signal(SIGHUP, catch_disconnect); //Need to see if the other end disconnects...
      sprintf(MESSAGE, "R*: Connecting to %s", address);
      Aprint(MESSAGE);
      fcntl(newsock, F_SETFL, O_NONBLOCK);

      /* Telnet negotiation */
      sprintf(MESSAGE, "%c%c%c%c%c%c%c%c%c", 0xff, 0xfb, 0x01, 0xff, 0xfb, 0x03, 0xff, 0xfd, 0x0f3);
      write(newsock, MESSAGE, 9);
      Aprint("R*: Negotiating Terminal Options...");
    }


}

void Device_GetInetAddress(void)
{
  int bufadr;
  int offset = 0;
  int devnam = TRUE;

  bufadr = (dGetByte(ICBAHZ) << 8) | dGetByte(ICBALZ);

  while (Device_isvalid(dGetByte(bufadr))) {
    int byte = dGetByte(bufadr);

    if (!devnam) {
      if (isupper(byte))
        byte = tolower(byte);

      inetaddress[offset++] = byte;
    }
    else if (byte == ':')
      devnam = FALSE;

    bufadr++;
  }

  inetaddress[offset++] = '\0';
}


void Device_ROPEN(void)
{
  int  port;
  int  direction;

  
  Poke(747,0);
  Poke(748,0);
  Poke(749,0);
  regA = 1;
  regY = 1;
  ClrN;


  port = Peek(ICAX2Z);
  direction = Peek(ICAX1Z);
  if(direction & 0x04) {
    Aprint("R*: Open for Reading...");
  } 
  if(direction & 0x08) {
    Aprint("R*: Open for Writing...");
    Device_GetInetAddress();
    open_connection(inetaddress, port);
  }
  if(direction & 0x01) {
    /* Open for concurrent mode */
  }
}

void Device_RCLOS(void)
{
  Poke(747,0);
  Poke(748,0);
  Poke(749,0);
  bufend = 0;
  regA = 1;
  regY = 1;
  ClrN;
  concurrent = 0;
  close ( newsock );
}

void Device_RREAD(void)
{
  int j;
  
  bufend = Peek(747);

  if(bufend != 255) 
  {
     if(translation) {
       if(bufout[0] == 0x0d) {
         regA = 0x9b;
       }
     } else {
       regA = bufout[0];
     }

     bufend--;

     for(j = 0; j <= bufend; j++)
     {
       bufout[j] = bufout[j+1];
     }

     //Cycle the buffer again to skip over linefeed....
     if(translation && linefeeds && (bufout[0] == 0x0a)) { 
       for(j = 0; j <= bufend; j++)
       {
         bufout[j] = bufout[j+1];
       }
     }
     return;
   } 

   if(concurrent) {
     Poke(747,bufend);
     Poke(748,0);
   }
   regY = 1;
   ClrN;
}



void Device_RWRIT(void)
{
  int port;
  unsigned char out_char;
  
  regY = 1;
  ClrN;

  bufend = Peek(747); 

    /* Translation mode */
    if(translation) {
      if(regA == 0x9b) {
        out_char = 0x0d;  
        if(linefeeds) {
          if(connected == 0) { /* local echo */
            bufend++;
            bufout[bufend-1] = out_char;
            bufout[bufend] = 0;

            command_end = 0;
            command_buf[command_end] = 0;
            strcat(bufout, "OK\r\n");
            bufend += 4;
            Poke(747,bufend);

          } else {
            write(newsock, &out_char, 1); // Write return
          }
          out_char = 0x0a;  //set char for line feed to be output later....
        }
      }
    } else {
      out_char = regA;
    }

    /* Translate the CR to a LF for telnet, ftp, etc */
    if(connected && trans_cr && (out_char == 0x0d)) {
      out_char = 0x0a;
    }
    
    if(out_char == 255) {
      //Aprint("R: Writing IAC...");
      retval = write(newsock, &out_char, 1); /* IAC escape sequence */
    }
    //if(retval == -1)
    if(connected == 0) { /* Local echo */
      bufend++;
      bufout[bufend-1] = out_char;
      bufout[bufend] = 0; 
      // Grab Command
      if((out_char == 0x9b) || (out_char == 0x0d)) {
        command_end = 0;
        if((command_buf[0] == 'A') && (command_buf[1] == 'T') && (command_buf[2] == 'D') && (command_buf[3] == 'I')) {
          //Aprint(command_buf);
          if(strchr(command_buf, ' ') != NULL) {
            port = atoi((char *)(strrchr(command_buf, ' ')+1));
            open_connection((char *)(strchr(command_buf, ' ')+1), port);
          }
          command_buf[command_end] = 0;
          strcat(bufout, "OK\r\n");
          bufend += 4;
        } else if((command_buf[0] == 'A') && (command_buf[1] == 'T') && (command_buf[2] == 'D') && (command_buf[3] == 'L')) {
          trans_cr = (trans_cr + 1) % 2;

          command_buf[command_end] = 0;
          strcat(bufout, "OK\r\n");
          bufend += 4;
        }
      } else {
        if(((out_char == 0x7f) || (out_char == 0x08)) && (command_end > 0)) {
          command_end--; /* backspace */
          command_buf[command_end] = 0;
        } else {
          command_buf[command_end] = out_char;
          command_buf[command_end+1] = 0;
          command_end = (command_end + 1) % 256;
        }
      }

    } else if(write(newsock, &out_char, 1) < 1) { /* returns -1 if disconnected or 0 if could not send */ 
      perror("write");
      Aprint("R*: ERROR on write.");
      SetN;
      regY = 135;
      //bufend = 13; /* To catch NO CARRIER message */
    } 


  regA = 1;
  Poke(747,bufend);
  Poke(748,0);
}

void Device_RSTAT(void)
{
  int devnum;
  int portnum = 9000;
  int on = 1;
  unsigned char telnet_command[2];
//static char IACctr = 0;

/* are we connected? */
  Poke(746,0);
  Poke(748,0);
  Poke(749,0);
  regA = 1;
  regY = 1;
  ClrN;
 
  bufend = Peek(747);

  if(Peek(764) == 1) { /* Hack for Ice-T Terminal program to work! */
    Poke(764, 255);
  }
  devnum = dGetByte(ICDNOZ);

  if(connected == 0)
  {

    if(do_once == 0)
    {

      //strcpy(PORT,"23\n");
      //strcpy(PORT,"8000\n");
      //sprintf(PORT, "%d", 8000 + devnum);
      portnum = portnum + devnum - 1;
/* Set up the listening port. */
      do_once = 1;
      memset ( &in, 0, sizeof ( struct sockaddr_in ) );
      sock = socket ( AF_INET, SOCK_STREAM, 0 );
      in.sin_family = AF_INET;
      in.sin_addr.s_addr = INADDR_ANY;
      //in.sin_port = htons ( atoi ( PORT ) );
      in.sin_port = htons (portnum);
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) ); // cmartin 
      if(bind ( sock, (struct sockaddr *)&in, sizeof ( struct sockaddr_in ) ) < 0) perror("bind");
      listen ( sock, 5 );
/*    sethostent(1); */
      retval = fcntl( sock, F_SETFL, O_NONBLOCK);
      len = sizeof ( struct sockaddr_in );
      bufend = 0;
      sprintf(MESSAGE, "R%d: Listening on port %d...", devnum, portnum);
      Aprint(MESSAGE);
    }

    newsock = accept ( sock, (struct sockaddr *)&peer_in, &len );
    if(newsock != -1)
    {
      struct hostent *host;
      if (getpeername(newsock, (struct sockaddr *) &peer_in, &len) < 0) { 
        perror("getpeername"); 
      } else { 
        sprintf(MESSAGE, "R%d: Serving Connection from %s...", devnum, inet_ntoa(peer_in.sin_addr));
        if ((host = gethostbyaddr((char *) &peer_in.sin_addr, sizeof peer_in.sin_addr, AF_INET)) == NULL) { 
          //perror("gethostbyaddr"); 
          //Aprint("Connected.");
        } else {
          sprintf(MESSAGE, "R%d: Serving Connection from %s.", devnum, host->h_name);
        }
      } 
      Aprint(MESSAGE);
      signal(SIGPIPE, catch_disconnect); //Need to see if the other end disconnects...
      signal(SIGHUP, catch_disconnect); //Need to see if the other end disconnects...
      retval = fcntl( newsock, F_SETFL, O_NONBLOCK);

      /* Telnet negotiation */
        sprintf(MESSAGE, "%c%c%c%c%c%c%c%c%c", 0xff, 0xfb, 0x01, 0xff, 0xfb, 0x03, 0xff, 0xfd, 0x0f3);
        write(newsock, MESSAGE, 9);
        Aprint("R*: Negotiating Terminal Options...");

      connected = 1;
/*
      retval = write(newsock, &IACdoBinary, 3); 
      retval = write(newsock, &IACwillBinary, 3); 
      retval = write(newsock, &IACdontLinemode, 3); 
      retval = write(newsock, &IACwontLinemode, 3); 
*/


      bufout[0] = 0;
      strcat(bufout, CONNECT_STRING);
      bufend = strlen(CONNECT_STRING);
      Poke(747,bufend);

      close(sock);
      return;
    }
  } 
  else 
  {
    /* Actually reading and setting the Atari input buffer here */
    if(concurrent == 1)
    {
      bytesread = read(newsock, &one, 1);
      if(bytesread > 0)
      {
        if(one == 0xff)
        {
          /* Start Telnet escape seq processing... */
            while(read(newsock, telnet_command,2) != 2) {};

            //sprintf(MESSAGE, "Telnet Command = 0x%x 0x%x", telnet_command[0], telnet_command[1]);
            //Aprint(MESSAGE);
            if(telnet_command[0] ==  0xfd) { //DO
              if((telnet_command[1] == 0x01) || (telnet_command[1] == 0x03)) { /* WILL ECHO and GO AHEAD (char mode) */
                telnet_command[0] = 0xfb; // WILL
              } else {
                telnet_command[0] = 0xfc; // WONT
              }
            } else if(telnet_command[0] == 0xfb) { //WILL
              //telnet_command[0] = 0xfd; //DO
              telnet_command[0] = 0xfe; //DONT
            } else if(telnet_command[0] == 0xfe) { //DONT
              telnet_command[0] = 0xfc;
            } else if(telnet_command[0] == 0xfc) { //WONT
              telnet_command[0] = 0xfe;
            } 
  
            if(telnet_command[0] == 0xfa) { /* subnegotiation */
              while(read(newsock, &one, 1) != 1) {};
              while(one != 0xf0) { /* wait for end of sub negotiation */
                while(read(newsock, &one, 1) != 1) {};
              }
            } else {
              write(newsock, &one, 1);
              write(newsock, telnet_command, 2);
            }

        }
        else
        {
          bufend++;
          bufout[bufend-1] = one;
          bufout[bufend] = 0;
          Poke(747,bufend);
          return;
        }
      }
    }
    else
    {       
      Aprint("R*: Not in concurrent mode....");
      //Poke(747,8);
      Poke(746,0);
      Poke(747,(12+48+192));
      return;
    }
  }
}

void Device_RSPEC(void)
{
  r_in = Peek(ICCOMZ);
  sprintf(MESSAGE, "R*: XIO %d", r_in); 
  Aprint(MESSAGE);

/*
  Aprint("ICCOMZ =");
  Aprint("%d",r_in);
  Aprint("^^ in ICCOMZ");
*/

  switch (r_in) {
    case 34:
      xio_34();
      break;
    case 36:
      xio_36();
      break;
    case 38:
      xio_38();
      break;
    case 40:
      xio_40();
      break;
    default:
      Aprint("Unsupported XIO #.");
      break;
  }
/*
  regA = 1;
  regY = 1;
  ClrN;
*/

}

void Device_RINIT(void)
{
  Aprint( "R*: INIT" );
  regA = 1;
  regY = 1;
  ClrN;
}


