/*
 * compress/uncompress function prototypes
 *
 * $Header: compress.h 1.1 92/09/29 $
 * $Log:	compress.h,v $
 * Revision 1.1  92/09/29  18:02:30  duplain
 * Initial revision
 * 
 */

#ifndef __COMPRESS_H
#define __COMPRESS_H

//#include "cproto.h"

typedef enum { COMPRESS, SQUASH, CRUNCH, UNIX_COMPRESS } CompType;
typedef enum {FNOERR, FEND, FRWERR} Ferror;
typedef enum {NOERR, RERR, WERR, CRCERR } Status;


Status uncompress (unsigned int complen, unsigned int origlen, FILE *ifp, FILE *ofp, CompType type);

#endif /* __COMPRESS_H */
