/* tool to read power production data for SMA solar power convertors 
Copyright Wim Hofman 2010 
Copyright Stephen Collier 2010,2011 

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* compile gcc -lbluetooth -g -o smatool smatool.c */

#define _XOPEN_SOURCE /* glibc needs this */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>



/*
* u16 represents an unsigned 16-bit number.  Adjust the typedef for
* your hardware.
*/
typedef u_int16_t u16;

#define PPPINITFCS16 0xffff /* Initial FCS value    */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */
#define ASSERT(x) assert(x)
#define SCHEMA "2"  /* Current database schema */
#define _XOPEN_SOURCE /* glibc2 needs this */


typedef struct{
	char Inverter[20]; 		/*--inverter 	-i 	*/
	char BTAddress[20];         /*--address  	-a 	*/
	int  bt_timeout;		/*--timeout  	-t 	*/
	char Password[20];          /*--password 	-p 	*/
	char Config[80];            /*--config   	-c 	*/
	char File[80];              /*--file     	-f 	*/
	float latitude_f;           /*--latitude  	-la 	*/
	float longitude_f;          /*--longitude 	-lo 	*/
	char PVOutputURL[80];       /*--pvouturl    -url 	*/
	char PVOutputKey[80];       /*--pvoutkey    -key 	*/
	char PVOutputSid[20];       /*--pvoutsid    -sid 	*/
	char Setting[80];           /*inverter model data*/
	unsigned char InverterCode[4]; /*Unknown code inverter specific*/
	unsigned int ArchiveCode;    /* Code for archive data */
	int nightMode;
	int liveMode;
} ConfType;

typedef struct{
	unsigned int 	key1;
	unsigned int 	key2;
	char		description[20];
	char		units[20];
	float		divisor;
} ReturnType;

int location=0;
int failedbluetooth=0;
unsigned char * last_sent;
char *lineread;
int linenum = 0;
unsigned char address[6] = { 0 };
unsigned char address2[6] = { 0 };
char chan[1];
unsigned char serial[4] = { 0 };
int found = 0, already_read = 0;
ConfType conf;
FILE *fp;
long returnpos;
int returnline;
int archdatalen=0;
struct archdata_type
{
	time_t date;
	char   inverter[20];
	long unsigned int serial;
	float  accum_value;
	float  current_value;
} *archdatalist;
time_t reporttime;
char datefrom[100];
char dateto[100];
int daterange = 0;
int  invcode;
unsigned char timestr[25] = { 0 };
unsigned char tzhex[2] = { 0 };
unsigned char timeset[4] = { 0x30,0xfe,0x7e,0x00 };
int s;
ReturnType *returnkeylist;
int num_return_keys=0;
unsigned char received[1024];
unsigned char * data;
int rr;
int terminated;
int file = 0;


char *accepted_strings[] = {
	"$END",
	"$ADDR",
	"$TIME",
	"$SER",
	"$CRC",
	"$POW",
	"$DTOT",
	"$ADD2",
	"$CHAN",
	"$ITIME",
	"$TMMI",
	"$TMPL",
	"$TIMESTRING",
	"$TIMEFROM1",
	"$TIMETO1",
	"$TIMEFROM2",
	"$TIMETO2",
	"$TESTDATA",
	"$ARCHIVEDATA1",
	"$PASSWORD",
	"$SIGNAL",
	"$UNKNOWN",
	"$INVCODE",
	"$ARCHCODE",
	"$INVERTERDATA",
	"$CNT",        /*Counter of sent packets*/
	"$TIMEZONE",    /*Timezone seconds +1 from GMT*/
	"$TIMESET"    /*Unknown string involved in time setting*/
};

enum 
{
	END,
	ADDR,
	TIME,
	SER,
	CRC,
	POW,
	DTOT,
	ADD2,
	CHAN,
	ITIME,
	TMMINUS,
	TMPLUS,
	TIMESTRING,
	TIMEFROM1,
	TIMETO1,
	TIMEFROM2,
	TIMETO2,
	TESTDATA,
	ARCHIVEDATA1,
	PASSWORD,
	SIGNAL,
	UNKNOWN,
	INVCODE,
	ARCHCODE,
	INVERTERDATA,
	CNT,        /*Counter of sent packets*/
	TIMEZONE,    /*Timezone seconds +1 from GMT*/
	TIMESET    /*Unknown string involved in time setting*/
};

int cc,debug = 0,verbose=0;
unsigned char fl[1024] = { 0 };


static u16 fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
* Calculate a new fcs given the current fcs and the new data.
*/
u16 pppfcs16(u16 fcs, void *_cp, int len)
{
	register unsigned char *cp = (unsigned char *)_cp;
	/* don't worry about the efficiency of these asserts here.  gcc will
	* recognise that the asserted expressions are constant and remove them.
	* Whether they are usefull is another question. 
	*/

	ASSERT(sizeof (u16) == 2);
	ASSERT(((u16) -1) > 0);
	while (len--)
		fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
	return (fcs);
}

/*
* Strip escapes (7D) as they aren't includes in fcs
*/
void strip_escapes(unsigned char *cp, int *len)
{
	int i,j;

	for( i=0; i<(*len); i++ ) {
		if( cp[i] == 0x7d ) { /*Found escape character. Need to convert*/
			cp[i] = cp[i+1]^0x20;
			for( j=i+1; j<(*len)-1; j++ ) cp[j] = cp[j+1];
			(*len)--;
		}
	}
}

/*
* Add escapes (7D) as they are required
*/
void add_escapes(unsigned char *cp, int *len)
{
	int i,j;

	for( i=19; i<(*len); i++ ) {
		switch( cp[i] ) {
		case 0x7d :
		case 0x7e :
		case 0x11 :
		case 0x12 :
		case 0x13 :
			for( j=(*len); j>i; j-- ) cp[j] = cp[j-1];
			cp[i+1] = cp[i]^0x20;
			cp[i]=0x7d;
			(*len)++;
		}
	}
}

void printdebug(char* fmt, ...)
{
	if (debug ==0)
		return;
	va_list args;
	va_start(args,fmt);
	vprintf(fmt,args);
	va_end(args);
	fflush(stdout);
}

void printerror(char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);
	vfprintf(stderr, fmt,args);
	va_end(args);
	fflush(stderr);
}


void printoutput(char* fmt, ...)
{
	if (conf.liveMode)
		return; // no normal output in live mode
	va_list args;
	va_start(args,fmt);
	vprintf(fmt,args);
	va_end(args);
	fflush(stdout);
}

void printlive(char* fmt, ...)
{
	int err;
	if (!conf.liveMode)
		return; // no normal output in live mode
	va_list args;
	va_start(args,fmt);
	err = vprintf(fmt,args);
	if (err<0)
		exit(errno);
	va_end(args);
	fflush(stdout);
}


void printverbose(char* fmt, ...)
{
	if (verbose ==0)
		return;
	va_list args;
	va_start(args,fmt);
	vprintf(fmt,args);
	va_end(args);
}

/*
* Recalculate and update length to correct for escapes
*/
void fix_length_send(unsigned char *cp, int *len)
{
	int	    delta=0;

	printdebug( "sum=%x\n", cp[1]+cp[3] );
	if(( cp[1] != (*len)+1 ))
	{
		delta = (*len)+1 - cp[1];
		printdebug( "  length change from %x to %x diff=%x \n", cp[1],(*len)+1,cp[1]+cp[3] );
		cp[3] = (cp[1]+cp[3])-((*len)+1);
		cp[1] =(*len)+1;

		switch( cp[1] ) {
		case 0x3a: cp[3]=0x44; break;
		case 0x3b: cp[3]=0x43; break;
		case 0x3c: cp[3]=0x42; break;
		case 0x3d: cp[3]=0x41; break;
		case 0x3e: cp[3]=0x40; break;
		case 0x3f: cp[3]=0x41; break;
		case 0x40: cp[3]=0x3e; break;
		case 0x41: cp[3]=0x3f; break;
		case 0x42: cp[3]=0x3c; break;
		case 0x52: cp[3]=0x2c; break;
		case 0x53: cp[3]=0x2b; break;
		case 0x54: cp[3]=0x2a; break;
		case 0x55: cp[3]=0x29; break;
		case 0x56: cp[3]=0x28; break;
		case 0x57: cp[3]=0x27; break;
		case 0x58: cp[3]=0x26; break;
		case 0x59: cp[3]=0x25; break;
		case 0x5a: cp[3]=0x24; break;
		case 0x5b: cp[3]=0x23; break;
		case 0x5c: cp[3]=0x22; break;
		case 0x5d: cp[3]=0x23; break;
		case 0x5e: cp[3]=0x20; break;
		case 0x5f: cp[3]=0x21; break;
		case 0x60: cp[3]=0x1e; break;
		case 0x61: cp[3]=0x1f; break;
		case 0x62: cp[3]=0x1e; break;
		default: printoutput( "NO CONVERSION!" );getchar();break;
		}
		printdebug( "new sum=%x\n", cp[1]+cp[3] );
	}
}

/*
* Recalculate and update length to correct for escapes
*/
void fix_length_received(unsigned char *received, int *len)
{
	int	    delta=0;
	int	    sum;

	if( received[1] != (*len) )
	{
		sum = received[1]+received[3];
		printdebug( "sum=%x", sum );
		delta = (*len) - received[1];
		printdebug( "length change from %x to %x\n", received[1], (*len) );
		if(( received[3] != 0x13 )&&( received[3] != 0x14 )) { 
			received[1] = (*len);
			switch( received[1] ) {
			case 0x52: received[3]=0x2c; break;
			case 0x5a: received[3]=0x24; break;
			case 0x66: received[3]=0x1a; break;
			case 0x6a: received[3]=0x14; break;
			default:  received[3]=sum-received[1]; break;
			}
		}
	}
}

/*
* How to use the fcs
*/
void
	tryfcs16(unsigned char *cp, int len)
{
	u16 trialfcs;
	unsigned
		int i;	 
	unsigned char stripped[1024] = { 0 };

	memcpy( stripped, cp, len );
	/* add on output */
	if (debug ==2){
		printdebug("String to calculate FCS\n");	 
		for (i=0;i<len;i++) printdebug("%02x ",cp[i]);
		printdebug("\n\n");
	}	
	trialfcs = pppfcs16( PPPINITFCS16, stripped, len );
	trialfcs ^= 0xffff;               /* complement */
	fl[cc] = (trialfcs & 0x00ff);    /* least significant byte first */
	fl[cc+1] = ((trialfcs >> 8) & 0x00ff);
	cc+=2;
	if (debug == 2 ){ 
		printdebug("FCS = %x%x %x\n",(trialfcs & 0x00ff),((trialfcs >> 8) & 0x00ff), trialfcs); 
	}
}


unsigned char conv(char *nn)
{
	unsigned char tt=0,res=0;
	int i;   

	for(i=0;i<2;i++){
		switch(nn[i]){

		case 65: /*A*/
		case 97: /*a*/
			tt = 10;
			break;

		case 66: /*B*/
		case 98: /*b*/
			tt = 11;
			break;

		case 67: /*C*/
		case 99: /*c*/
			tt = 12;
			break;

		case 68: /*D*/
		case 100: /*d*/
			tt = 13;
			break;

		case 69: /*E*/
		case 101: /*e*/
			tt = 14;
			break;

		case 70: /*F*/
		case 102: /*f*/
			tt = 15;
			break;


		default:
			tt = nn[i] - 48;
		}
		res = res + (tt * pow(16,1-i));
	}
	return res;
}

int
	check_send_error( ConfType * conf, int *s, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated, int *already_read )
{
	int bytes_read,i,j;
	unsigned char buf[1024]; /*read buffer*/
	unsigned char header[3]; /*read buffer*/
	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = 0; // set timeout of reading
	tv.tv_usec = 5000;
	memset(buf,0,1024);

	FD_ZERO(&readfds);
	FD_SET((*s), &readfds);

	select((*s)+1, &readfds, NULL, NULL, &tv);

	(*terminated) = 0; // Tag to tell if string has 7e termination
	// first read the header to get the record length
	if (FD_ISSET((*s), &readfds)){	// did we receive anything within 5 seconds
		bytes_read = recv((*s), header, sizeof(header), 0); //Get length of string
		(*rr) = 0;
		for( i=0; i<sizeof(header); i++ ) {
			received[(*rr)] = header[i];
			printdebug("%02x ", received[(*rr)]);
			(*rr)++;
		}
	}
	else
	{
		printverbose("Timeout reading bluetooth socket\n");
		(*rr) = 0;
		memset(received,0,1024);
		return -1;
	}
	if (FD_ISSET((*s), &readfds)){	// did we receive anything within 5 seconds
		bytes_read = recv((*s), buf, header[1]-3, 0); //Read the length specified by header
	}
	else
	{
		printverbose("Timeout reading bluetooth socket\n");
		(*rr) = 0;
		memset(received,0,1024);
		return -1;
	}
	if ( bytes_read > 0){
		printdebug("\nReceiving\n");
		if (debug == 1){ 
			printdebug( "    %08x: .. .. .. .. .. .. .. .. .. .. .. .. ", 0 );
			j=12;
			for( i=0; i<sizeof(header); i++ ) {
				if( j%16== 0 )
					printdebug( "\n    %08x: ",j);
				printdebug("%02x ",header[i]);
				j++;
			}
			for (i=0;i<bytes_read;i++) {
				if( j%16== 0 )
					printdebug( "\n    %08x: ",j);
				printdebug("%02x ",buf[i]);
				j++;
			}
			printdebug(" rr=%d",(bytes_read+(*rr)));
			printdebug("\n\n");
		}
		if ((cc==bytes_read)&&(memcmp(received,last_sent,cc) == 0)){
			printerror( "ERROR received what we sent!" ); getchar();
			//Need to do something
		}
		if( buf[ bytes_read-1 ] == 0x7e )
			(*terminated) = 1;
		else
			(*terminated) = 0;
		for (i=0;i<bytes_read;i++){ //start copy the rec buffer in to received
			if (buf[i] == 0x7d){ //did we receive the escape char
				switch (buf[i+1]){   // act depending on the char after the escape char

				case 0x5e :
					received[(*rr)] = 0x7e;
					break;

				case 0x5d :
					received[(*rr)] = 0x7d;
					break;

				default :
					received[(*rr)] = buf[i+1] ^ 0x20;
					break;
				}
				i++;
			}
			else { 
				received[(*rr)] = buf[i];
			}
			printdebug("%02x ", received[(*rr)]);
			(*rr)++;
		}
		fix_length_received( received, rr );
		if (debug == 1) {
			printdebug("\n");
			for( i=0;i<(*rr); i++ ) printdebug("%02x ", received[(i)]);
		}
		printdebug("\n\n");
		(*already_read)=1;
	}	
	return 0;
}

int
	read_bluetooth( ConfType * conf, int *s, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated )
{
	int bytes_read,i,j;
	unsigned char buf[1024]; /*read buffer*/
	unsigned char header[3]; /*read buffer*/
	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = conf->bt_timeout; // set timeout of reading
	tv.tv_usec = 0;
	memset(buf,0,1024);

	FD_ZERO(&readfds);
	FD_SET((*s), &readfds);

	select((*s)+1, &readfds, NULL, NULL, &tv);

	(*terminated) = 0; // Tag to tell if string has 7e termination
	// first read the header to get the record length
	if (FD_ISSET((*s), &readfds)){	// did we receive anything within 5 seconds
		bytes_read = recv((*s), header, sizeof(header), 0); //Get length of string
		(*rr) = 0;
		for( i=0; i<sizeof(header); i++ ) {
			received[(*rr)] = header[i];
			if (debug == 2) printdebug("%02x ", received[i]);
			(*rr)++;
		}
	}
	else
	{
		printverbose("Timeout reading bluetooth socket\n");
		(*rr) = 0;
		memset(received,0,1024);
		return -1;
	}
	if (FD_ISSET((*s), &readfds)){	// did we receive anything within 5 seconds
		bytes_read = recv((*s), buf, header[1]-3, 0); //Read the length specified by header
	}
	else
	{
		printverbose("Timeout reading bluetooth socket\n");
		(*rr) = 0;
		memset(received,0,1024);
		return -1;
	}
	if ( bytes_read > 0){
		printdebug("\nReceiving\n");
		if (debug == 1){ 
			printdebug( "    %08x: .. .. .. .. .. .. .. .. .. .. .. .. ", 0 );
			j=12;
			for( i=0; i<sizeof(header); i++ ) {
				if( j%16== 0 )
					printdebug( "\n    %08x: ",j);
				printdebug("%02x ",header[i]);
				j++;
			}
			for (i=0;i<bytes_read;i++) {
				if( j%16== 0 )
					printdebug( "\n    %08x: ",j);
				printdebug("%02x ",buf[i]);
				j++;
			}
			printdebug(" rr=%d",(bytes_read+(*rr)));
			printdebug("\n\n");
		}
		if ((cc==bytes_read)&&(memcmp(received,last_sent,cc) == 0)){
			printerror( "ERROR received what we sent!" ); getchar();
			//Need to do something
		}
		if( buf[ bytes_read-1 ] == 0x7e )
			(*terminated) = 1;
		else
			(*terminated) = 0;
		for (i=0;i<bytes_read;i++){ //start copy the rec buffer in to received
			if (buf[i] == 0x7d){ //did we receive the escape char
				switch (buf[i+1]){   // act depending on the char after the escape char

				case 0x5e :
					received[(*rr)] = 0x7e;
					break;

				case 0x5d :
					received[(*rr)] = 0x7d;
					break;

				default :
					received[(*rr)] = buf[i+1] ^ 0x20;
					break;
				}
				i++;
			}
			else { 
				received[(*rr)] = buf[i];
			}
			if (debug == 2) printdebug("%02x ", received[(*rr)]);
			(*rr)++;
		}
		fix_length_received( received, rr );
		if (debug == 2) {
			printdebug("\n");
			for( i=0;i<(*rr); i++ ) printdebug("%02x ", received[(i)]);
		}
		printdebug("\n\n");
	}	
	return 0;
}

int select_str(char *s)
{
	int i;
	for (i=0; i < sizeof(accepted_strings)/sizeof(*accepted_strings);i++)
	{
		//printdebug( "\ni=%d accepted=%s string=%s", i, accepted_strings[i], s );
		if (!strcmp(s, accepted_strings[i])) return i;
	}
	return -1;
}

unsigned char *  get_timezone_in_seconds( unsigned char *tzhex )
{
	time_t curtime;
	struct tm *loctime;
	struct tm *utctime;
	int day,month,year,hour,minute,isdst;
	char *returntime;

	float localOffset;
	int	 tzsecs;

	returntime = (char *)malloc(6*sizeof(char));
	curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
	loctime = localtime(&curtime);
	day = loctime->tm_mday;
	month = loctime->tm_mon +1;
	year = loctime->tm_year + 1900;
	hour = loctime->tm_hour;
	minute = loctime->tm_min; 
	isdst  = loctime->tm_isdst;
	utctime = gmtime(&curtime);


	printdebug( "utc=%04d-%02d-%02d %02d:%02d local=%04d-%02d-%02d %02d:%02d diff %d hours\n", utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, year, month, day, hour, minute, hour-utctime->tm_hour );
	localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
	printdebug( "localOffset=%f\n", localOffset );
	if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
		localOffset+=24;
	if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
		localOffset-=24;
	printdebug( "localOffset=%f isdst=%d\n", localOffset, isdst );
	if( isdst > 0 ) 
		localOffset=localOffset-1;
	tzsecs = (localOffset) * 3600 + 1;
	if( tzsecs < 0 )
		tzsecs=65536+tzsecs;
	printdebug( "tzsecs=%x %d\n", tzsecs, tzsecs );
	tzhex[1] = tzsecs/256;
	tzhex[0] = tzsecs -(tzsecs/256)*256;
	printdebug( "tzsecs=%02x %02x\n", tzhex[1], tzhex[0] );

	return tzhex;
}

float  sunrise( float latitude, float longitude )
{
	//adapted from http://williams.best.vwh.net/sunrise_sunset_algorithm.htm
	time_t curtime;
	struct tm *loctime;
	struct tm *utctime;
	int day,month,year,hour,minute;
	char *returntime;

	double t,M,L,T,RA,Lquadrant,RAquadrant,sinDec,cosDec;
	double cosH, H, UT, localT,lngHour;
	float localOffset,zenith=91;
	double pi=M_PI;

	returntime = (char *)malloc(6*sizeof(char));
	curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
	loctime = localtime(&curtime);
	day = loctime->tm_mday;
	month = loctime->tm_mon +1;
	year = loctime->tm_year + 1900;
	hour = loctime->tm_hour;
	minute = loctime->tm_min; 
	utctime = gmtime(&curtime);


	printdebug( "utc=%04d-%02d-%02d %02d:%02d local=%04d-%02d-%02d %02d:%02d diff %d hours\n", utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, year, month, day, hour, minute, hour-utctime->tm_hour );
	localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
	printdebug( "localOffset=%f\n", localOffset );
	if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
		localOffset+=24;
	if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
		localOffset-=24;
	printdebug( "localOffset=%f\n", localOffset );
	lngHour = longitude / 15;
	t = loctime->tm_yday + ((6 - lngHour) / 24);
	//Calculate the Sun's mean anomaly
	M = (0.9856 * t) - 3.289;
	//Calculate the Sun's tru longitude
	L = M + (1.916 * sin((pi/180)*M)) + (0.020 * sin(2 * (pi/180)*M)) + 282.634;
	if( L > 360 ) L=L-360;
	if( L < 0 ) L=L+360;
	//calculate the Sun's right ascension
	RA = (180/pi)*atan(0.91764 * tan((pi/180)*L));
	//right ascension value needs to be in the same quadrant as L
	Lquadrant  = (floor( L/90)) * 90;
	RAquadrant = (floor(RA/90)) * 90;

	RA = RA + (Lquadrant - RAquadrant);
	//right ascension value needs to be converted into hours
	RA = RA / 15;
	//calculate the Sun's declination
	sinDec = 0.39782 * sin((pi/180)*L);
	cosDec = cos(asin(sinDec));
	//calculate the Sun's local hour angle
	cosH = (cos((pi/180)*zenith) - (sinDec * sin((pi/180)*latitude))) / (cosDec * cos((pi/180)*latitude));

	if (cosH >  1) 
		printerror( "Sun never rises here!\n" );
	//the sun never rises on this location (on the specified date)
	if (cosH < -1)
		printerror( "Sun never sets here!\n" );
	//the sun never sets on this location (on the specified date)
	//finish calculating H and convert into hours
	H = 360 -(180/pi)*acos(cosH);
	H = H/15;
	//calculate local mean time of rising/setting
	T = H + RA - (0.06571 * t) - 6.622;
	//adjust back to UTC
	UT = T - lngHour;
	if( UT < 0 ) UT=UT+24;
	if( UT > 24 ) UT=UT-24;
	day = loctime->tm_mday;
	month = loctime->tm_mon +1;
	year = loctime->tm_year + 1900;
	hour = loctime->tm_hour;
	minute = loctime->tm_min; 
	//convert UT value to local time zone of latitude/longitude
	localT = UT + localOffset;
	if( localT < 0 ) localT=localT+24;
	if( localT > 24 ) localT=localT-24;
	return localT;
}

float sunset( float latitude, float longitude )
{
	//adapted from http://williams.best.vwh.net/sunrise_sunset_algorithm.htm
	time_t curtime;
	struct tm *loctime;
	struct tm *utctime;
	int day,month,year,hour,minute;
	char *returntime;

	double t,M,L,T,RA,Lquadrant,RAquadrant,sinDec,cosDec;
	double cosH, H, UT, localT,lngHour;
	float localOffset,zenith=91;
	double pi=M_PI;

	returntime = (char *)malloc(6*sizeof(char));

	curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
	loctime = localtime(&curtime);
	day = loctime->tm_mday;
	month = loctime->tm_mon +1;
	year = loctime->tm_year + 1900;
	hour = loctime->tm_hour;
	minute = loctime->tm_min; 
	utctime = gmtime(&curtime);


	localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
	if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
		localOffset+=24;
	if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
		localOffset-=24;

	lngHour = longitude / 15;
	t = loctime->tm_yday + ((18 - lngHour) / 24);
	//Calculate the Sun's mean anomaly
	M = (0.9856 * t) - 3.289;
	//Calculate the Sun's tru longitude
	L = M + (1.916 * sin((pi/180)*M)) + (0.020 * sin(2 * (pi/180)*M)) + 282.634;
	if( L > 360 ) L=L-360;
	if( L < 0 ) L=L+360;
	//calculate the Sun's right ascension
	RA = (180/pi)*atan(0.91764 * tan((pi/180)*L));
	//right ascension value needs to be in the same quadrant as L
	Lquadrant  = (floor( L/90)) * 90;
	RAquadrant = (floor(RA/90)) * 90;

	RA = RA + (Lquadrant - RAquadrant);
	//right ascension value needs to be converted into hours
	RA = RA / 15;
	//calculate the Sun's declination
	sinDec = 0.39782 * sin((pi/180)*L);
	cosDec = cos(asin(sinDec));
	//calculate the Sun's local hour angle
	cosH = (cos((pi/180)*zenith) - (sinDec * sin((pi/180)*latitude))) / (cosDec * cos((pi/180)*latitude));

	if (cosH >  1); 
	//the sun never rises on this location (on the specified date)
	if (cosH < -1);
	//the sun never sets on this location (on the specified date)
	//finish calculating H and convert into hours
	H = (180/pi)*acos(cosH);
	H = H/15;
	//calculate local mean time of rising/setting
	T = H + RA - (0.06571 * t) - 6.622;
	//adjust back to UTC
	UT = T - lngHour;
	if( UT > 24 ) UT=UT-24;
	if( UT < 0 ) UT=UT+24;
	//convert UT value to local time zone of latitude/longitude
	localT = UT + localOffset;
	if( localT < 0 ) localT=localT+24;
	if( localT > 24 ) localT=localT-24;
	return localT;
}

//Set a value depending on inverter
void  SetInverterType( ConfType * conf )  
{
	if( strcmp(conf->Inverter, "1700TL") == 0 ) {
		conf->InverterCode[0] = 0x12;
		conf->InverterCode[1] = 0x1a;
		conf->InverterCode[2] = 0xd9;
		conf->InverterCode[3] = 0x38;
		conf->ArchiveCode    = 0x63;
	}
	if( strcmp(conf->Inverter, "2100TL") == 0 ) {
		conf->InverterCode[0] = 0x17;
		conf->InverterCode[1] = 0x97;
		conf->InverterCode[2] = 0x51;
		conf->InverterCode[3] = 0x38;
		conf->ArchiveCode    = 0x63;
	}
	if( strcmp(conf->Inverter, "3000TL") == 0 ) {
		conf->InverterCode[0] = 0x12;
		conf->InverterCode[1] = 0x1a;
		conf->InverterCode[2] = 0xd9;
		conf->InverterCode[3] = 0x38;
		conf->InverterCode[0] = 0x32;
		conf->InverterCode[1] = 0x42;
		conf->InverterCode[2] = 0x85;
		conf->InverterCode[3] = 0x38;
		conf->ArchiveCode    = 0x71;
	}
	if( strcmp(conf->Inverter, "3000TLHF") == 0 ) {
		conf->InverterCode[0] = 0x1b;
		conf->InverterCode[1] = 0xb1;
		conf->InverterCode[2] = 0xa6;
		conf->InverterCode[3] = 0x38;
		conf->ArchiveCode    = 0x83;
	}
	if( strcmp(conf->Inverter, "4000TL") == 0 ) {
		conf->InverterCode[0] = 0x78;
		conf->InverterCode[1] = 0x21;
		conf->InverterCode[2] = 0xbf;
		conf->InverterCode[3] = 0x3a;
		conf->ArchiveCode    = 0x4e;
	}
	if( strcmp(conf->Inverter, "5000TL") == 0 ) {
		conf->InverterCode[0] = 0x3f;
		conf->InverterCode[1] = 0x10;
		conf->InverterCode[2] = 0xfb;
		conf->InverterCode[3] = 0x39;
		conf->ArchiveCode     = 0x4e;
	}
	if( strcmp(conf->Inverter, "7000") == 0 ) {
		conf->InverterCode[0] = 0xcf;
		conf->InverterCode[1] = 0x84;
		conf->InverterCode[2] = 0x84;
		conf->InverterCode[3] = 0x3a;
		conf->ArchiveCode     = 0x63;
	}
	if( strcmp(conf->Inverter, "10000TL") == 0 ) {
		conf->InverterCode[0] = 0x69;
		conf->InverterCode[1] = 0x45;
		conf->InverterCode[2] = 0x32;
		conf->InverterCode[3] = 0x39;
		conf->ArchiveCode     = 0x80;
	}
	if( strcmp(conf->Inverter, "XXXXTL") == 0 ) {
		conf->InverterCode[0] = 0x99;
		conf->InverterCode[1] = 0x35;
		conf->InverterCode[2] = 0x40;
		conf->InverterCode[3] = 0x36;
		conf->ArchiveCode     = 0x4e;
	}
}

int is_light( float latitude, float longitude)
{

	time_t      curtime;
	int         day,month,year,hour,minute,second;
	struct tm   *loctime;
	float tofd, sr, ss;
	curtime = time(NULL);  //get time in seconds since epoch (1/1/1970) 
	loctime = localtime(&curtime);
	day = loctime->tm_mday;
	month = loctime->tm_mon +1;
	year = loctime->tm_year + 1900;
	hour = loctime->tm_hour;
	minute = loctime->tm_min; 
	second = loctime->tm_sec; 

	tofd = ( minute / 60.0f + hour);
	//	printdebug("tofd = %f\n", tofd);

	sr = sunrise(latitude, longitude);
	ss = sunset(latitude, longitude);
	//	printdebug("%f - %f", sr, ss);

	return (tofd >= sr && tofd <= ss);

}

//Convert a recieved string to a value
long ConvertStreamtoLong( unsigned char * stream, int length, long unsigned int * value )
{
	int	i, nullvalue;

	(*value) = 0;
	nullvalue = 1;

	for( i=0; i < length; i++ ) 
	{
		if( stream[i] != 0xff ) //check if all ffs which is a null value 
			nullvalue = 0;
		(*value) = (*value) + stream[i]*pow(256,i);
	}
	if( nullvalue == 1 )
		(*value) = 0; //Asigning null to 0 at this stage unless it breaks something
	return (*value);
}

//Convert a recieved string to a value
float ConvertStreamtoFloat( unsigned char * stream, int length, float * value )
{
	int	i, nullvalue;

	(*value) = 0;
	nullvalue = 1;

	for( i=0; i < length; i++ ) 
	{
		if( stream[i] != 0xff ) //check if all ffs which is a null value 
			nullvalue = 0;
		(*value) = (*value) + stream[i]*pow(256,i);
	}
	if( nullvalue == 1 )
		(*value) = 0; //Asigning null to 0 at this stage unless it breaks something
	return (*value);
}

//read return value data from init file
ReturnType * 
	InitReturnKeys( ConfType * conf, ReturnType * returnkeylist, int * num_return_keys )
{
	FILE		*fp;
	char		line[400];
	ReturnType   tmp;
	int		i, j, reading, data_follows;

	data_follows = 0;

	fp=fopen(conf->File,"r");

	while (!feof(fp)){	
		if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
			if( line[0] != '#' ) 
			{
				if( strncmp( line, ":unit conversions", 17 ) == 0 )
					data_follows = 1;
				if( strncmp( line, ":end unit conversions", 21 ) == 0 )
					data_follows = 0;
				if( data_follows == 1 ) {
					tmp.key1=0x0;
					tmp.key2=0x0;
					strcpy( tmp.description, "" ); //Null out value
					strcpy( tmp.units, "" ); //Null out value
					tmp.divisor=0;
					reading=0;
					if( sscanf( line, "%x %x", &tmp.key1, &tmp.key2  ) == 2 ) {
						j=0;
						for( i=6; line[i]!='\0'; i++ ) {
							if(( line[i] == '"' )&&(reading==1)) {
								tmp.description[j]='\0';
								break;
							}
							if( reading == 1 )
							{
								tmp.description[j] = line[i];
								j++;
							}

							if(( line[i] == '"' )&&(reading==0))
								reading = 1;
						}
						if( sscanf( line+i+1, "%s %f", tmp.units, &tmp.divisor ) == 2 ) {

							if( (*num_return_keys) == 0 )
								returnkeylist=(ReturnType *)malloc(sizeof(ReturnType));
							else
								returnkeylist=(ReturnType *)realloc(returnkeylist,sizeof(ReturnType)*((*num_return_keys)+1));
							(returnkeylist+(*num_return_keys))->key1=tmp.key1;
							(returnkeylist+(*num_return_keys))->key2=tmp.key2;
							strcpy( (returnkeylist+(*num_return_keys))->description, tmp.description );
							strcpy( (returnkeylist+(*num_return_keys))->units, tmp.units );
							(returnkeylist+(*num_return_keys))->divisor = tmp.divisor;
							(*num_return_keys)++;
						}
					}
				}
			}
		}
	}
	fclose(fp);

	return returnkeylist;
}

//Convert a recieved string to a value
int ConvertStreamtoInt( unsigned char * stream, int length, int * value )
{
	int	i, nullvalue;

	(*value) = 0;
	nullvalue = 1;

	for( i=0; i < length; i++ ) 
	{
		if( stream[i] != 0xff ) //check if all ffs which is a null value 
			nullvalue = 0;
		(*value) = (*value) + stream[i]*pow(256,i);
	}
	if( nullvalue == 1 )
		(*value) = 0; //Asigning null to 0 at this stage unless it breaks something
	return (*value);
}

//Convert a recieved string to a value
time_t ConvertStreamtoTime( unsigned char * stream, int length, time_t * value )
{
	int	i, nullvalue;

	(*value) = 0;
	nullvalue = 1;

	for( i=0; i < length; i++ ) 
	{
		if( stream[i] != 0xff ) //check if all ffs which is a null value 
			nullvalue = 0;
		(*value) = (*value) + stream[i]*pow(256,i);
	}
	if( nullvalue == 1 )
		(*value) = 0; //Asigning null to 0 at this stage unless it breaks something
	return (*value);
}

// Set switches to save lots of strcmps
void  SetSwitches( ConfType *conf, char * datefrom, char * dateto, int *location, int *file, int *daterange, int *test )  
{
	//Check if all location variables are set
	if(( conf->latitude_f <= 180 )&&( conf->longitude_f <= 180 ))
		(*location)=1;
	else
		(*location)=0;
	//Check if all File variables are set
	if( strlen(conf->File) > 0 )
		(*file)=1;
	else
		(*file)=0;
	if(( strlen(datefrom) > 0 )
		&&( strlen(dateto) > 0 ))
		(*daterange)=1;
	else
		(*daterange)=0;
}

unsigned char *
	ReadStream( ConfType * conf, int * s, unsigned char * stream, int * streamlen, unsigned char * datalist, int * datalen, unsigned char * last_sent, int cc, int * terminated, int * togo )
{

	//	printdebug( "%x, %d, %x, %d, %x, %d, %d, %d, %d, %d", conf, *s, stream, *streamlen, datalist, *datalen, *last_sent, cc, *terminated, *togo);

	int	finished;
	int	finished_record;
	int  i, j=0;

	(*togo)=ConvertStreamtoInt( stream+43, 2, togo );
	printdebug( "togo=%d\n", (*togo) );
	i=59; //Initial position of data stream
	(*datalen)=0;

	datalist=(unsigned char *)malloc(sizeof(char));
	finished=0;
	finished_record=0;
	while( finished != 1 ) {
		datalist=(unsigned char *)realloc(datalist,sizeof(char)*((*datalen)+(*streamlen)-i));
		while( finished_record != 1 ) {
			if( i> 500 ) break; //Somthing has gone wrong

			if(( i < (*streamlen) )&&(( (*terminated) != 1)||(i+3 < (*streamlen) ))) 
			{
				datalist[j]=stream[i];
				j++;
				(*datalen)=j;
				i++;
			}
			else
				finished_record = 1;

		}
		finished_record = 0;
		if( (*terminated) == 0 )
		{
			read_bluetooth( conf, s, streamlen, stream, cc, last_sent, terminated );
			i=18;
		}
		else
			finished = 1;
	}
	if( debug== 1 ) {
		printdebug( "len=%d data=", (*datalen) );
		for( i=0; i< (*datalen); i++ )
			printdebug( "%02x ", datalist[i] );
		printdebug( "\n" );
	}
	return datalist;
}

/* Init Config to default values */
void InitConfig( ConfType *conf, char * datefrom, char * dateto )
{
	strcpy( conf->Config,"./smatool.conf");
	strcpy( conf->Setting,"./invcode.in");
	strcpy( conf->Inverter, "" );  
	strcpy( conf->BTAddress, "" );  
	conf->bt_timeout = 30;  
	strcpy( conf->Password, "0000" );  
	strcpy( conf->File, "sma.in.new" );  
	conf->latitude_f = 999 ;  
	conf->longitude_f = 999 ;  
	strcpy( conf->PVOutputURL, "http://pvoutput.org/service/r2/addstatus.jsp" );  
	strcpy( conf->PVOutputKey, "" );  
	strcpy( conf->PVOutputSid, "" );  
	conf->InverterCode[0]=0;  
	conf->InverterCode[1]=0;  
	conf->InverterCode[2]=0;  
	conf->InverterCode[3]=0;  
	conf->ArchiveCode=0;  
	strcpy( datefrom, "" );  
	strcpy( dateto, "" );
	conf->liveMode = 0;
	conf->nightMode = 0;
}

/* read Config from file */
int GetConfig( ConfType *conf )
{
	FILE 	*fp;
	char	line[400];
	char	variable[400];
	char	value[400];

	if (strlen(conf->Config) > 0 )
	{
		if(( fp=fopen(conf->Config,"r")) == (FILE *)NULL )
		{
			printerror( "Error! Could not open file %s\n", conf->Config );
			return( -1 ); //Could not open file
		}
	}
	else
	{
		if(( fp=fopen("./smatool.conf","r")) == (FILE *)NULL )
		{
			printerror( "Error! Could not open file ./smatool.conf\n" );
			return( -1 ); //Could not open file
		}
	}
	while (!feof(fp)){	
		if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
			if( line[0] != '#' ) 
			{
				strcpy( value, "" ); //Null out value
				sscanf( line, "%s %s", variable, value );
				printdebug( "variable=%s value=%s\n", variable, value );
				if( value[0] != '\0' )
				{
					if( strcmp( variable, "Inverter" ) == 0 )
						strcpy( conf->Inverter, value );  
					if( strcmp( variable, "BTAddress" ) == 0 )
						strcpy( conf->BTAddress, value );  
					if( strcmp( variable, "BTTimeout" ) == 0 )
						conf->bt_timeout =  atoi(value);  
					if( strcmp( variable, "Password" ) == 0 )
						strcpy( conf->Password, value );  
					if( strcmp( variable, "File" ) == 0 )
						strcpy( conf->File, value );  
					if( strcmp( variable, "Latitude" ) == 0 )
						conf->latitude_f = atof(value) ;  
					if( strcmp( variable, "Longitude" ) == 0 )
						conf->longitude_f = atof(value) ;  
					if( strcmp( variable, "PVOutputURL" ) == 0 )
						strcpy( conf->PVOutputURL, value );  
					if( strcmp( variable, "PVOutputKey" ) == 0 )
						strcpy( conf->PVOutputKey, value );  
					if( strcmp( variable, "PVOutputSid" ) == 0 )
						strcpy( conf->PVOutputSid, value );  
				}
			}
		}
	}
	fclose( fp );
	return( 0 );
}

/* read  Inverter Settings from file */
int GetInverterSetting( ConfType *conf )
{
	FILE 	*fp;
	char	line[400];
	char	variable[400];
	char	value[400];
	int		found_inverter=0;

	if (strlen(conf->Setting) > 0 )
	{
		if(( fp=fopen(conf->Setting,"r")) == (FILE *)NULL )
		{
			printerror( "Error! Could not open file %s\n", conf->Setting );
			return( -1 ); //Could not open file
		}
	}
	else
	{
		if(( fp=fopen("./invcode.in","r")) == (FILE *)NULL )
		{
			printerror( "Error! Could not open file ./invcode.in\n" );
			return( -1 ); //Could not open file
		}
	}
	while (!feof(fp)){	
		if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
			if( line[0] != '#' ) 
			{
				strcpy( value, "" ); //Null out value
				sscanf( line, "%s %s", variable, value );
				printdebug( "variable='%s' value='%s'\n", variable, value );
				if( value[0] != '\0' )
				{
					int val;
					if( strcmp( variable, "Inverter" ) == 0 )
					{
						if( strcmp( value, conf->Inverter ) == 0 )
						{
							found_inverter = 1;
							//							printdebug("found right inverter\n");
						}
						else
							found_inverter = 0;
					}
					if (found_inverter)
					{
						sscanf(value, "%X", &val);
						if(( strcmp( variable, "Code1" ) == 0 )&& found_inverter )
							conf->InverterCode[0] = val;
						if(( strcmp( variable, "Code2" ) == 0 )&& found_inverter )
							conf->InverterCode[1] = val;
						if(( strcmp( variable, "Code3" ) == 0 )&& found_inverter )
							conf->InverterCode[2] = val;
						if(( strcmp( variable, "Code4" ) == 0 )&& found_inverter )
							conf->InverterCode[3] = val;
						if(( strcmp( variable, "InvCode" ) == 0 )&& found_inverter )
							conf->ArchiveCode = val;
					}
				}
			}
		}
		//		printdebug("%x, %x, %x, %x, %x\n", conf->InverterCode[0], conf->InverterCode[1],conf->InverterCode[2], conf->InverterCode[3], conf->ArchiveCode);

	}
	fclose( fp );
	if(( conf->InverterCode[0] == 0 ) ||
		( conf->InverterCode[1] == 0 ) ||
		( conf->InverterCode[2] == 0 ) ||
		( conf->InverterCode[3] == 0 ) ||
		( conf->ArchiveCode == 0 ))
	{
		printerror( "\n Error ! not all codes set\n" );
		fclose( fp );
		return( -1 );
	}
	return( 0 );
}

/* Print a help message */
void PrintHelp()
{
	printoutput( "Usage: smatool [OPTION]\n" );
	printoutput( "  -v,  --verbose                           Give more verbose output\n" );
	printoutput( "  -d,  --debug                             Show debug\n" );
	printoutput( "  -c,  --config CONFIGFILE                 Set config file default smatool.conf\n" );
	printoutput( "       --test                              Run in test mode - don't update data\n" );
	printoutput( "\n" );
	printoutput( "  -from  --datefrom YYYY-DD-MM HH:MM:00    Date range from date\n" );
	printoutput( "  -to  --dateto YYYY-DD-MM HH:MM:00        Date range to date\n" );
	printoutput( "\n" );
	printoutput( "The following options are in config file but may be overridden\n" );
	printoutput( "  -i,  --inverter INVERTER_MODEL           inverter model\n" );
	printoutput( "  -a,  --address INVERTER_ADDRESS          inverter BT address\n" );
	printoutput( "  -t,  --timeout TIMEOUT                   bluetooth timeout (secs) default 5\n" );
	printoutput( "  -p,  --password PASSWORD                 inverter user password default 0000\n" );
	printoutput( "  -f,  --file FILENAME                     command file default sma.in.new\n" );
	printoutput( "Location Information to calculate sunset and sunrise so inverter is not\n" );
	printoutput( "queried in the dark\n" );
	printoutput( "  -lat,  --latitude LATITUDE               location latitude -180 to 180 deg\n" );
	printoutput( "  -lon,  --longitude LONGITUDE             location longitude -90 to 90 deg\n" );
	printoutput( "PVOutput.org (A free solar information system) Configs\n" );
	printoutput( "  -url,  --pvouturl PVOUTURL               pvoutput.org live url\n");
	printoutput( "  -key,  --pvoutkey PVOUTKEY               pvoutput.org key\n");
	printoutput( "  -sid,  --pvoutsid PVOUTSID               pvoutput.org sid\n");
	printoutput( "  -n	                                     Night mode - do not test for the sun being up\n");
	printoutput( "  -l	                                     Live mode - output the live power to stdout every 5 seconds\n");
	printoutput( "\n\n" );
}

/* Init Config to default values */
int ReadCommandConfig( ConfType *conf, int argc, char **argv, char * datefrom, char * dateto, int * verbose, int * debug, int * test)
{
	int	i;

	// these need validation checking at some stage TODO
	for (i=1;i<argc;i++)			//Read through passed arguments
	{
		if ((strcmp(argv[i],"-v")==0)||(strcmp(argv[i],"--verbose")==0)) (*verbose) = 1;
		else if ((strcmp(argv[i],"-d")==0)||(strcmp(argv[i],"--debug")==0)) (*debug) = 1;
		else if ((strcmp(argv[i],"-c")==0)||(strcmp(argv[i],"--config")==0)){
			i++;
			if(i<argc){
				strcpy(conf->Config,argv[i]);
			}
		}
		else if (strcmp(argv[i],"--test")==0) (*test)=1;
		else if (strcmp(argv[i],"-n")==0) conf->nightMode = 1;
		else if (strcmp(argv[i],"-l")==0) conf->liveMode = 1;
		else if ((strcmp(argv[i],"-from")==0)||(strcmp(argv[i],"--datefrom")==0)){
			i++;
			if(i<argc){
				strcpy(datefrom,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-to")==0)||(strcmp(argv[i],"--dateto")==0)){
			i++;
			if(i<argc){
				strcpy(dateto,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-i")==0)||(strcmp(argv[i],"--inverter")==0)){
			i++;
			if (i<argc){
				strcpy(conf->Inverter,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-a")==0)||(strcmp(argv[i],"--address")==0)){
			i++;
			if (i<argc){
				strcpy(conf->BTAddress,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-t")==0)||(strcmp(argv[i],"--timeout")==0)){
			i++;
			if (i<argc){
				conf->bt_timeout = atoi(argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-p")==0)||(strcmp(argv[i],"--password")==0)){
			i++;
			if (i<argc){
				strcpy(conf->Password,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-f")==0)||(strcmp(argv[i],"--file")==0)){
			i++;
			if (i<argc){
				strcpy(conf->File,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-lat")==0)||(strcmp(argv[i],"--latitude")==0)){
			i++;
			if(i<argc){
				conf->latitude_f=atof(argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-long")==0)||(strcmp(argv[i],"--longitude")==0)){
			i++;
			if(i<argc){
				conf->longitude_f=atof(argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-url")==0)||(strcmp(argv[i],"--pvouturl")==0)){
			i++;
			if(i<argc){
				strcpy(conf->PVOutputURL,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-key")==0)||(strcmp(argv[i],"--pvoutkey")==0)){
			i++;
			if(i<argc){
				strcpy(conf->PVOutputKey,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-sid")==0)||(strcmp(argv[i],"--pvoutsid")==0)){
			i++;
			if(i<argc){
				strcpy(conf->PVOutputSid,argv[i]);
			}
		}
		else if ((strcmp(argv[i],"-h")==0) || (strcmp(argv[i],"--help") == 0 ))
		{
			PrintHelp();
			return( -1 );
		}
		else
		{
			printerror("Bad Syntax\n\n" );
			for( i=0; i< argc; i++ )
				printerror( "%s ", argv[i] );
			printerror( "\n\n" );

			PrintHelp();
			return( -1 );
		}
	}
	return( 0 );
}

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) 
{
	size_t written;

	written = fwrite(ptr, size, nmemb, stream);
	return written;
}

char * debugdate()
{
	time_t curtime;
	struct tm *tm;
	static char result[20];

	curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
	tm = localtime(&curtime);
	sprintf( result, "%4d-%02d-%02d %02d:%02d:%02d",
		1900+tm->tm_year,
		1+tm->tm_mon,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec );
	return result;
}

void ReadData()
{
	int i;

	printdebug("[%d] %s Waiting for string\n",linenum, debugdate() );
	cc = 0;
	do{
		lineread = strtok(NULL," ;");
		switch(select_str(lineread)) {

		case END:
			//do nothing
			break;			

		case ADDR:
			for (i=0;i<6;i++){
				fl[cc] = address[i];
				cc++;
			}
			break;	

		case SER:
			for (i=0;i<4;i++){
				fl[cc] = serial[i];
				cc++;
			}
			break;	

		case ADD2:
			for (i=0;i<6;i++){
				fl[cc] = address2[i];
				cc++;
			}
			break;	

		case CHAN:
			fl[cc] = chan[0];
			cc++;
			break;

		default :
			fl[cc] = conv(lineread);
			cc++;
		}

	} while (strcmp(lineread,"$END"));
	if (debug == 1){ 
		printdebug("[%d] %s waiting for: ", linenum, debugdate() );
		for (i=0;i<cc;i++) printdebug("%02x ",fl[i]);
		printdebug("\n\n");
	}
	printdebug("[%d] %s Waiting for data on rfcomm\n", linenum, debugdate());
	found = 0;
	do {
		if( already_read == 0 )
			rr=0;
		if(( already_read == 0 )&&( read_bluetooth( &conf, &s, &rr, received, cc, last_sent, &terminated ) != 0 ))
		{
			already_read=0;
			fseek( fp, returnpos, 0 );
			linenum = returnline;
			found=0;
			if( archdatalen > 0 )
				free( archdatalist );
			archdatalen=0;
			strcpy( lineread, "" );
			sleep(10);
			failedbluetooth++;
			if( failedbluetooth > 3 )
				exit(-1);
			return;
		}
		else {
			already_read=0;
			if (debug == 1){ 
				printdebug( "[%d] %s looking for: ",linenum, debugdate());
				for (i=0;i<cc;i++) printdebug("%02x ",fl[i]);
				printdebug( "\n" );
				printdebug( "[%d] %s received:    ",linenum, debugdate());
				for (i=0;i<rr;i++) printdebug("%02x ",received[i]);
				printdebug("\n\n");
			}

			if (memcmp(fl+4,received+4,cc-4) == 0){
				found = 1;
				printdebug("[%d] %s Found string we are waiting for\n",linenum, debugdate()); 
			} else {
				printdebug("[%d] %s Did not find string\n", linenum,debugdate()); 
			}
		}
	} while (found == 0);
	if (debug == 2){ 
		for (i=0;i<cc;i++) printdebug("%02x ",fl[i]);
		printdebug("\n\n");
	}
}

void WriteData()
{
	int i,j;
	char tt[10] = {48,48,48,48,48,48,48,48,48,48}; 
	char ti[3];	
	time_t fromtime;
	time_t totime;
	struct tm tm;
	int  pass_i;
	unsigned char send_count = 0x0;

	printdebug("[%d] %s Sending\n", linenum,debugdate());
	cc = 0;
	do{
		lineread = strtok(NULL," ;");
		switch(select_str(lineread)) {

		case END:
			//do nothing
			break;			

		case ADDR:
			for (i=0;i<6;i++){
				fl[cc] = address[i];
				cc++;
			}
			break;

		case SER:
			for (i=0;i<4;i++){
				fl[cc] = serial[i];
				cc++;
			}
			break;	


		case ADD2:
			for (i=0;i<6;i++){
				fl[cc] = address2[i];
				cc++;
			}
			break;

		case TIME:	
			// get report time and convert
			sprintf(tt,"%x",(int)reporttime); //convert to a hex in a string
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case TMPLUS:	
			// get report time and convert
			sprintf(tt,"%x",(int)reporttime+1); //convert to a hex in a string
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;


		case TMMINUS:
			// get report time and convert
			sprintf(tt,"%x",(int)reporttime-1); //convert to a hex in a string
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case CRC: //$crc
			tryfcs16(fl+19, cc -19);
			add_escapes(fl,&cc);
			fix_length_send(fl,&cc);
			break;

		case CHAN:
			fl[cc] = chan[0];
			cc++;
			break;

		case TIMESTRING:
			for (i=0;i<25;i++){
				fl[cc] = timestr[i];
				cc++;
			}
			break;

		case TIMEFROM1:	
			// get report time and convert
			if( daterange == 1 ) {
				if( strptime( datefrom, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
				{
					printdebug( "datefrom %s\n", datefrom );
					printdebug( "Time Coversion Error\n" );
					exit(-1);
				}
				tm.tm_isdst=-1;
				fromtime=mktime(&tm);
				if( fromtime == -1 ) {
					// Error we need to do something about it
					printerror( "%03x",(int)fromtime ); getchar();
					printerror( "\n%03x", (int)fromtime ); getchar();
					fromtime=0;
					printerror( "bad from" ); getchar();
				}
			}
			else
			{
				printoutput( "no from" ); 
				fromtime=0;
			}
			sprintf(tt,"%03x",(int)fromtime-300); //convert to a hex in a string and start 5 mins before for dummy read.
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case TIMETO1:	
			if( daterange == 1 ) {
				if( strptime( dateto, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
				{
					printdebug( "dateto %s\n", dateto );
					printerror( "Time Coversion Error\n" );
					exit(-1);
				}
				tm.tm_isdst=-1;
				totime=mktime(&tm);
				if( totime == -1 ) {
					// Error we need to do something about it
					printerror( "%03x",(int)totime ); getchar();
					printerror( "\n%03x", (int)totime ); getchar();
					totime=0;
					printerror( "bad to" ); getchar();
				}
			}
			else
				totime=0;
			sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
			// get report time and convert
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case TIMEFROM2:	
			if( daterange == 1 ) {
				strptime( datefrom, "%Y-%m-%d %H:%M:%S", &tm);
				tm.tm_isdst=-1;
				fromtime=mktime(&tm)-86400;
				if( fromtime == -1 ) {
					// Error we need to do something about it
					printerror( "%03x",(int)fromtime ); getchar();
					printerror( "\n%03x", (int)fromtime ); getchar();
					fromtime=0;
					printerror( "bad from" ); getchar();
				}
			}
			else
			{
				printoutput( "no from" ); 
				fromtime=0;
			}
			sprintf(tt,"%03x",(int)fromtime); //convert to a hex in a string
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case TIMETO2:	
			if( daterange == 1 ) {
				strptime( dateto, "%Y-%m-%d %H:%M:%S", &tm);

				tm.tm_isdst=-1;
				totime=mktime(&tm)-86400;
				if( totime == -1 ) {
					// Error we need to do something about it
					printerror( "%03x",(int)totime ); getchar();
					printerror( "\n%03x", (int)totime ); getchar();
					fromtime=0;
					printerror( "bad from" ); getchar();
				}
			}
			else
				totime=0;
			sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
			for (i=7;i>0;i=i-2){ //change order and convert to integer
				ti[1] = tt[i];
				ti[0] = tt[i-1];	
				ti[2] = '\0';
				fl[cc] = conv(ti);
				cc++;		
			}
			break;

		case PASSWORD:

			j=0;
			for(i=0;i<12;i++){
				if( conf.Password[j] == '\0' )
					fl[cc] = 0x88;
				else {
					pass_i = conf.Password[j];
					fl[cc] = (( pass_i+0x88 )%0xff);
					j++;
				}
				cc++;
			}
			break;	

		case UNKNOWN:
			for (i=0;i<4;i++){
				fl[cc] = conf.InverterCode[i];
				cc++;
			}
			break;

		case INVCODE:
			fl[cc] = invcode;
			cc++;
			break;
		case ARCHCODE:
			fl[cc] = conf.ArchiveCode;
			cc++;
			break;
		case CNT: //send counter
			send_count++;
			fl[cc] = send_count;
			cc++;
			break;
		case TIMEZONE: //timezone in seconds, reverse endian
			fl[cc] = tzhex[0];
			fl[cc+1] = tzhex[1];
			cc+=2;
			break;
		case TIMESET: //unknown setting
			for( i=0; i<4; i++ ) {
				fl[cc] = timeset[i];
				cc++;
			}
			break;

		default :
			fl[cc] = conv(lineread);
			cc++;
		}

	} while (strcmp(lineread,"$END"));
	if (debug == 1){ 
		printdebug( "[%d] %s sending:\n",linenum, debugdate());
		printdebug( "    %08x: .. .. .. .. .. .. .. .. .. .. .. .. ", 0 );
		j=12;
		for (i=0;i<cc;i++) {
			if( j%16== 0 )
				printdebug( "\n    %08x: ",j);
			printdebug("%02x ",fl[i]);
			j++;
		}
		printdebug(" cc=%d",cc);
		printdebug("\n\n");
	}
	last_sent = (unsigned  char *)realloc( last_sent, sizeof( unsigned char )*(cc));
	memcpy(last_sent,fl,cc);
	write(s,fl,cc);
	already_read=0;
	//check_send_error( &conf, &s, &rr, received, cc, last_sent, &terminated, &already_read ); 
}

void ExtractData()
{
	int datalen, togo;
	int day,month,year,hour,minute,second;
	time_t idate;
	struct tm *loctime;
	float currentpower_total;
	int gap=1;
	float dtotal;
	float gtotal;
	float ptotal;
	float strength;
	int return_key;
	int i,j;
	int finished=0;
	unsigned char datarecord[1024];
	time_t prev_idate;



	printdebug("[%d] %s Extracting\n", linenum, debugdate());
	cc = 0;
	do{
		lineread = strtok(NULL," ;");
		//printdebug( "\nselect=%d", select_str(lineread)); 
		switch(select_str(lineread)) {

		case SER: // Extract Serial of Inverter

			data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
			/*
			printdebug( "1.len=%d data=", datalen );
			for( i=0; i< datalen; i++ )
			printdebug( "%02x ", data[i] );
			printdebug( "\n" );
			*/
			serial[3]=data[19];
			serial[2]=data[18];
			serial[1]=data[17];
			serial[0]=data[16];
			printverbose( "serial=%02x:%02x:%02x:%02x\n",serial[3]&0xff,serial[2]&0xff,serial[1]&0xff,serial[0]&0xff ); 
			free( data );
			break;

		case ITIME: // extract Time from Inverter
			idate = (received[66] * 16777216 ) + (received[65] *65536 )+ (received[64] * 256) + received[63];
			loctime = localtime(&idate);
			day = loctime->tm_mday;
			month = loctime->tm_mon +1;
			year = loctime->tm_year + 1900;
			hour = loctime->tm_hour;
			minute = loctime->tm_min; 
			second = loctime->tm_sec; 
			printoutput("Date power = %d/%d/%4d %02d:%02d:%02d\n",day, month, year, hour, minute,second);
			//currentpower = (received[72] * 256) + received[71];
			//printdebug("Current power = %i Watt\n",currentpower);
			break;
		case POW: // extract current power $POW
			data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
			if( (data+3)[0] == 0x08 )
				gap = 40; 
			if( (data+3)[0] == 0x10 )
				gap = 40; 
			if( (data+3)[0] == 0x40 )
				gap = 28;
			if( (data+3)[0] == 0x00 )
				gap = 28;
			for ( i = 0; i<datalen; i+=gap ) 
			{
				idate=ConvertStreamtoTime( data+i+4, 4, &idate );
				loctime = localtime(&idate);
				day = loctime->tm_mday;
				month = loctime->tm_mon +1;
				year = loctime->tm_year + 1900;
				hour = loctime->tm_hour;
				minute = loctime->tm_min; 
				second = loctime->tm_sec; 
				ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
				return_key=-1;
				for( j=0; j<num_return_keys; j++ )
				{
					if(( (data+i+1)[0] == returnkeylist[j].key1 )&&((data+i+2)[0] == returnkeylist[j].key2)) {
						return_key=j;
						break;
					}
				}
				if( return_key >= 0 )
				{
					if (conf.liveMode)
					{
						if (!strcmp(returnkeylist[return_key].description, "Total Power"))
							printlive("Power=%.0f ", currentpower_total/returnkeylist[return_key].divisor);
						if (!strcmp(returnkeylist[return_key].description, "Total Generation"))
							printlive("Total=%.3f ", currentpower_total/returnkeylist[return_key].divisor);

					}
					printoutput("%d-%02d-%02d %02d:%02d:%02d %-20s = %.3f %-20s\n", year, month, day, hour, minute, second, returnkeylist[return_key].description, currentpower_total/returnkeylist[return_key].divisor, returnkeylist[return_key].units );
				}
				else
					printoutput("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS\n", year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[1], currentpower_total );
			}
			free( data );
			break;

		case DTOT: // extract total energy collected today

			gtotal = (received[69] * 65536) + (received[68] * 256) + received[67];
			gtotal = gtotal / 1000;
			printoutput("G total so far = %.2f Kwh\n",gtotal);
			dtotal = (received[84] * 256) + received[83];
			dtotal = dtotal / 1000;
			printoutput("E total today = %.2f Kwh\n",dtotal);
			break;		

		case ADD2: // extract 2nd address
			memcpy(address2,received+26,6);
			printdebug("address 2 \n");
			break;

		case CHAN: // extract bluetooth channel
			memcpy(chan,received+22,1);
			printdebug("Bluetooth channel = %i\n",chan[0]);
			break;

		case TIMESTRING: // extract time strings $TIMESTRING
			if(( received[60] == 0x6d )&&( received[61] == 0x23 ))
			{
				memcpy(timestr,received+63,24);
				printdebug("extracting timestring\n");
				memcpy(timeset,received+79,4);
				idate=ConvertStreamtoTime( received+63,4, &idate );
				/* Allow delay for inverter to be slow */
				if( reporttime > idate ) {
					if( debug == 1 )
						printdebug( "delay=%d\n", (int)(reporttime-idate) );
					//sleep( reporttime - idate );
					sleep(5);    //was sleeping for > 1min excessive
				}
			}
			else
			{
				memcpy(timestr,received+63,24);
				printdebug("bad extracting timestring\n");
				already_read=0;
				fseek( fp, returnpos, 0 );
				linenum = returnline;
				found=0;
				if( archdatalen > 0 )
					free( archdatalist );
				archdatalen=0;
				strcpy( lineread, "" );
				failedbluetooth++;
				if( failedbluetooth > 60 )
					exit(-1);
				return;
			}

			break;

		case TESTDATA: // Test data
			data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
			printoutput( "\n" );

			free( data );
			break;

		case ARCHIVEDATA1:
			finished=0;
			ptotal=0;
			idate=0;
			printoutput( "\n" );
			while( finished != 1 ) {
				data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );

				j=0;
				for( i=0; i<datalen; i++ )
				{
					datarecord[j]=data[i];
					j++;
					if( j > 11 ) {
						if( idate > 0 ) prev_idate=idate;
						else prev_idate=0;
						idate=ConvertStreamtoTime( datarecord, 4, &idate );
						if( prev_idate == 0 )
							prev_idate = idate-300;

						loctime = localtime(&idate);
						day = loctime->tm_mday;
						month = loctime->tm_mon +1;
						year = loctime->tm_year + 1900;
						hour = loctime->tm_hour;
						minute = loctime->tm_min; 
						second = loctime->tm_sec; 
						ConvertStreamtoFloat( datarecord+4, 8, &gtotal );
						if(archdatalen == 0 )
							ptotal = gtotal;
						printoutput("\n%d/%d/%4d %02d:%02d:%02d  total=%.3f Kwh current=%.0f Watts togo=%d i=%d", day, month, year, hour, minute,second, gtotal/1000, (gtotal-ptotal)*12, togo, i);
						if( idate != prev_idate+300 ) {
							printerror( "Date Error! prev=%d current=%d\n", (int)prev_idate, (int)idate );
							break;
						}
						if( archdatalen == 0 )
							archdatalist = (struct archdata_type *)malloc( sizeof( struct archdata_type ) );
						else
							archdatalist = (struct archdata_type *)realloc( archdatalist, sizeof( struct archdata_type )*(archdatalen+1));
						(archdatalist+archdatalen)->date=idate;
						strcpy((archdatalist+archdatalen)->inverter,conf.Inverter);
						ConvertStreamtoLong( serial, 4, &(archdatalist+archdatalen)->serial);
						(archdatalist+archdatalen)->accum_value=gtotal/1000;
						(archdatalist+archdatalen)->current_value=(gtotal-ptotal)*12;
						archdatalen++;
						ptotal=gtotal;
						j=0; //get ready for another record
					}
				}
				if( togo == 0 ) 
					finished=1;
				else
					if( read_bluetooth( &conf, &s, &rr, received, cc, last_sent, &terminated ) != 0 )
					{
						fseek( fp, returnpos, 0 );
						linenum = returnline;
						found=0;
						if( archdatalen > 0 )
							free( archdatalist );
						archdatalen=0;
						strcpy( lineread, "" );
						sleep(10);
						failedbluetooth++;
						if( failedbluetooth > 3 )
							exit(-1);
						return;
					}
			}
			free( data );
			printoutput( "\n" );

			break;
		case SIGNAL: // SIGNAL signal strength

			strength  = (received[22] * 100.0)/0xff;
			printverbose("bluetooth signal = %.0f%%\n",strength);
			break;		
		case INVCODE: // extract time strings $INVCODE
			invcode=received[22];
			printdebug("extracting invcode=%02x\n", invcode);

			break;
		case INVERTERDATA: // Inverter data $INVERTERDATA
			data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
			printdebug( "data=%02x\n",(data+3)[0] );
			if( (data+3)[0] == 0x08 )
				gap = 40; 
			if( (data+3)[0] == 0x10 )
				gap = 40; 
			if( (data+3)[0] == 0x40 )
				gap = 28;
			if( (data+3)[0] == 0x00 )
				gap = 28;
			for ( i = 0; i<datalen; i+=gap ) 
			{
				idate=ConvertStreamtoTime( data+i+4, 4, &idate );
				loctime = localtime(&idate);
				day = loctime->tm_mday;
				month = loctime->tm_mon +1;
				year = loctime->tm_year + 1900;
				hour = loctime->tm_hour;
				minute = loctime->tm_min; 
				second = loctime->tm_sec; 
				ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
				return_key=-1;
				for( j=0; j<num_return_keys; j++ )
				{
					if(( (data+i+1)[0] == returnkeylist[j].key1 )&&((data+i+2)[0] == returnkeylist[j].key2)) {
						return_key=j;
						break;
					}
				}
				if( return_key >= 0 ) {
					if( i==0 )
						printoutput("%d-%02d-%02d  %02d:%02d:%02d %s\n", year, month, day, hour, minute, second, (data+i+8) );
					printoutput("%d-%02d-%02d %02d:%02d:%02d %-20s = %.0f %-20s\n", year, month, day, hour, minute, second, returnkeylist[return_key].description, currentpower_total/returnkeylist[return_key].divisor, returnkeylist[return_key].units );
				}
				else
					printoutput("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS \n", year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[0], currentpower_total );
			}
			free( data );
			break;
		}				
	}

	while (strcmp(lineread,"$END"));
} 

int livepos, liveline;
int  initstarted=0,setupstarted=0,rangedatastarted=0;

void ProcessLine(FILE* fp)
{
	char line[400];
	if (fgets(line,400,fp) != NULL){				//read line from sma.in
		linenum++;
		lineread = strtok(line," ;");
		if(!strcmp(lineread,"R")){		//See if line is something we need to receive
			ReadData();
		}
		if(!strcmp(lineread,"S")){		//See if line is something we need to send
			WriteData();
		}
		if(!strcmp(lineread,"E")){		//See if line is something we need to extract
			ExtractData();
		}
		if(!strcmp(lineread,":init")){		//See if line is something we need to extract
			initstarted=1;
			returnpos=ftell(fp);
			returnline = linenum;
		}
		if(!strcmp(lineread,":setup")){		//See if line is something we need to extract
			setupstarted=1;
			returnpos=ftell(fp);
			returnline = linenum;
		}
		if(!strcmp(lineread,":startsetup")){		//See if line is something we need to extract
			sleep(1);
		}
		if(!strcmp(lineread,":setinverter1")){		//See if line is something we need to extract
			setupstarted=1;
			returnpos=ftell(fp);
			returnline = linenum;
		}
		if(!strcmp(lineread,":getrangedata")){		//See if line is something we need to extract
			rangedatastarted=1;
			returnpos=ftell(fp);
			returnline = linenum;
		}
		if (!strcmp(lineread,":getlivevalues"))
		{
			livepos = ftell(fp);
			liveline = linenum;
		}

	}
}

void  ALARMhandler(int sig)
{
	signal(SIGALRM, SIG_IGN);          /* ignore this signal       */
	printerror("Timeout trying to talk to inverter\n");  
	signal(SIGALRM, ALARMhandler);     /* reinstall the handler    */
	exit(1);
}

void PipeHandler()
{
	printerror("Broken Pipe.");
	exit(EPIPE);
}

int FetchInverterData()
{
	struct sockaddr_rc addr = { 0 };
	int i,status;

	if(((location=0)||conf.nightMode||conf.liveMode||is_light( conf.latitude_f, conf.longitude_f )))
	{
		printverbose("Address %s\n",conf.BTAddress);

		if (file ==1)
			fp=fopen(conf.File,"r");
		else
			fp=fopen("/etc/sma.in","r");
		for( i=1; i<20; i++ ){
			// allocate a socket
			s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

			// set the connection parameters (who to connect to)
			addr.rc_family = AF_BLUETOOTH;
			addr.rc_channel = (uint8_t) 1;
			str2ba( conf.BTAddress, &addr.rc_bdaddr );

			// connect to server
			printdebug( "datefrom=%s dateto=%s\n", datefrom, dateto );
			status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
			if (status <0){
				printerror("Error connecting to %s\n",conf.BTAddress);
				close( s );
			}
			else
			{
				printdebug( "connected to server\n");
				break;
			}
		}
		if (status < 0 )
		{
			return( -1 );
		}

		// convert address
		address[5] = conv(strtok(conf.BTAddress,":"));
		address[4] = conv(strtok(NULL,":"));
		address[3] = conv(strtok(NULL,":"));
		address[2] = conv(strtok(NULL,":"));
		address[1] = conv(strtok(NULL,":"));
		address[0] = conv(strtok(NULL,":"));

		alarm(30);
		while (!feof(fp)){	
			ProcessLine(fp);

			if (conf.liveMode && rangedatastarted)
				break;
		}

		if (conf.liveMode)
			printlive("\n");


		while (conf.liveMode) // livedata...
		{
			alarm(30);
			sleep(5);

			if (conf.nightMode || is_light(conf.latitude_f, conf.longitude_f))
			{
				// go back to the livepos section of the input file
				fseek(fp, livepos, SEEK_SET);
				linenum = liveline;
				rangedatastarted = 0;
				while (rangedatastarted == 0)
				{
					ProcessLine(fp);
				}
				printlive("\n");
			}
			else
			{
				break;
			}
		}
		close(s);

	}

	return 0;
}

int main(int argc, char **argv)
{
	int test=0;

	memset(received,0,1024);
	signal(SIGALRM, ALARMhandler);
	signal(SIGPIPE, PipeHandler);
	alarm(60);

	last_sent = (unsigned  char *)malloc( sizeof( unsigned char ));
	/* get the report time - used in various places */
	reporttime = time(NULL);  //get time in seconds since epoch (1/1/1970)	

	// set config to defaults
	InitConfig( &conf, datefrom, dateto );
	// read command arguments needed so can get config
	if( ReadCommandConfig( &conf, argc, argv, datefrom, dateto, &verbose, &debug, &test) < 0 )
	{
		printerror("Bad Command Config\n");
		exit(0);
	}
	// read Config file
	if( GetConfig( &conf ) < 0 )
	{
		printerror("Bad Config\n");
		exit(-1);
	}
	// read command arguments  again - they overide config
	if( ReadCommandConfig( &conf, argc, argv, datefrom, dateto, &verbose, &debug, &test) < 0 )
		exit(0);
	// read Inverter Setting file
	if( GetInverterSetting( &conf ) < 0 )
	{
		printerror("Bad inverter string \n");
		exit(-1);
	}
	// set switches used through the program
	SetSwitches( &conf, datefrom, dateto, &location, &file, &daterange, &test );  
	// Set value for inverter type
	//SetInverterType( &conf );
	// Get Return Value lookup from file
	returnkeylist = InitReturnKeys( &conf, returnkeylist, &num_return_keys );
	// Get Local Timezone offset in seconds
	get_timezone_in_seconds( tzhex );
	//	printdebug("daterange is %d\n", daterange);

	while (1) // livedata...
	{
		if (conf.nightMode || is_light(conf.latitude_f, conf.longitude_f))
		{
			int result =FetchInverterData();
			if (result != 0)
				return result;
		}
		else
		{
			printlive("Night time\n");
		}
		if (!conf.liveMode)
			break;
		sleep(10);
	}

	

	if( archdatalen > 0 )
		free( archdatalist );
	archdatalen=0;
	free(last_sent);


	return 0;
}
