/*
 * async.c - async I/O code
 *
 * Copyright (c) 2000 Sebastian Bauer
 * Copyright (c) 2000-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "async.h"

//-------------------------------------
// AS_OpenAsnycFH


#ifdef ASIO_NOEXTERNALS
AsyncFile*
AS_OpenAsyncFH(
	BPTR handle,
	OpenModes mode,
	LONG bufferSize,
	BOOL closeIt,
	struct ExecBase *SysBase,
	struct DosLibrary *DOSBase )
#else
AsyncFile *
AS_OpenAsyncFH( BPTR handle, OpenModes mode, LONG bufferSize, BOOL closeIt )
#endif
{
	struct FileHandle	*fh;
	AsyncFile		*file = NULL;
	BPTR	lock = NULL;
	LONG	blockSize, blockSize2;
	D_S( struct InfoData, infoData );

	if( mode == MODE_READ )
	{
		if( handle )
		{
			lock = DupLockFromFH( handle );
		}
	}
	else
	{
		if( mode == MODE_APPEND )
		{
			/* in append mode, we open for writing, and then seek to the
			 * end of the file. That way, the initial write will happen at
			 * the end of the file, thus extending it
			 */

			if( handle )
			{
				if( Seek( handle, 0, OFFSET_END ) < 0 )
				{
					if( closeIt )
					{
						Close( handle );
					}

					handle = NULL;
				}
			}
		}

		/* we want a lock on the same device as where the file is. We can't
		 * use DupLockFromFH() for a write-mode file though. So we get sneaky
		 * and get a lock on the parent of the file
		 */
		if( handle )
		{
			lock = ParentOfFH( handle );
		}
	}

	if( handle )
	{
		/* if it was possible to obtain a lock on the same device as the
		 * file we're working on, get the block size of that device and
		 * round up our buffer size to be a multiple of the block size.
		 * This maximizes DMA efficiency.
		 */

		blockSize = 512;
		blockSize2 = 1024;

		if( lock )
		{
			if( Info( lock, infoData ) )
			{
				blockSize = infoData->id_BytesPerBlock;
				blockSize2 = blockSize * 2;
				bufferSize = ( ( bufferSize + blockSize2 - 1 ) / blockSize2 ) * blockSize2;
			}

			UnLock(lock);
		}

		/* now allocate the ASyncFile structure, as well as the read buffers.
		 * Add 15 bytes to the total size in order to allow for later
		 * quad-longword alignement of the buffers
		 */

		for( ;; )
		{
			if(( file = AllocVec( sizeof( AsyncFile ) + bufferSize + 15, MEMF_PUBLIC | MEMF_ANY ) ))
			{
				break;
			}
			else
			{
				if( bufferSize > blockSize2 )
				{
					bufferSize -= blockSize2;
				}
				else
				{
					break;
				}
			}
		}

		if( file )
		{
			file->af_File		= handle;
			file->af_ReadMode	= ( mode == MODE_READ );
			file->af_BlockSize	= blockSize;
			file->af_CloseFH	= closeIt;

			/* initialize the ASyncFile structure. We do as much as we can here,
			 * in order to avoid doing it in more critical sections
			 *
			 * Note how the two buffers used are quad-longword aligned. This
			 * helps performance on 68040 systems with copyback cache. Aligning
			 * the data avoids a nasty side-effect of the 040 caches on DMA.
			 * Not aligning the data causes the device driver to have to do
			 * some magic to avoid the cache problem. This magic will generally
			 * involve flushing the CPU caches. This is very costly on an 040.
			 * Aligning things avoids the need for magic, at the cost of at
			 * most 15 bytes of ram.
			 */

			fh			= BADDR( file->af_File );
			file->af_Handler	= fh->fh_Type;
			file->af_BufferSize	= ( ULONG ) bufferSize / 2;
			file->af_Buffers[ 0 ]	= ( APTR ) ( ( ( ULONG ) file + sizeof( AsyncFile ) + 15 ) & 0xfffffff0 );
			file->af_Buffers[ 1 ]	= file->af_Buffers[ 0 ] + file->af_BufferSize;
			file->af_CurrentBuf	= 0;
			file->af_SeekOffset	= 0;
			file->af_PacketPending	= FALSE;
			file->af_SeekPastEOF	= FALSE;
#ifdef ASIO_NOEXTERNALS
			file->af_SysBase	= SysBase;
			file->af_DOSBase	= DOSBase;
#endif

			/* this is the port used to get the packets we send out back.
			 * It is initialized to PA_IGNORE, which means that no signal is
			 * generated when a message comes in to the port. The signal bit
			 * number is initialized to SIGB_SINGLE, which is the special bit
			 * that can be used for one-shot signalling. The signal will never
			 * be set, since the port is of type PA_IGNORE. We'll change the
			 * type of the port later on to PA_SIGNAL whenever we need to wait
			 * for a message to come in.
			 *
			 * The trick used here avoids the need to allocate an extra signal
			 * bit for the port. It is quite efficient.
			 */

			file->af_PacketPort.mp_MsgList.lh_Head		= ( struct Node * ) &file->af_PacketPort.mp_MsgList.lh_Tail;
			file->af_PacketPort.mp_MsgList.lh_Tail		= NULL;
			file->af_PacketPort.mp_MsgList.lh_TailPred	= ( struct Node * ) &file->af_PacketPort.mp_MsgList.lh_Head;
			file->af_PacketPort.mp_Node.ln_Type		= NT_MSGPORT;
			/* MH: Avoid problems with SnoopDos */
			file->af_PacketPort.mp_Node.ln_Name		= NULL;
			file->af_PacketPort.mp_Flags			= PA_IGNORE;
			file->af_PacketPort.mp_SigBit			= SIGB_SINGLE;
			file->af_PacketPort.mp_SigTask			= FindTask( NULL );

			file->af_Packet.sp_Pkt.dp_Link			= &file->af_Packet.sp_Msg;
			file->af_Packet.sp_Pkt.dp_Arg1			= fh->fh_Arg1;
			file->af_Packet.sp_Pkt.dp_Arg3			= file->af_BufferSize;
			file->af_Packet.sp_Pkt.dp_Res1			= 0;
			file->af_Packet.sp_Pkt.dp_Res2			= 0;
			file->af_Packet.sp_Msg.mn_Node.ln_Name		= ( STRPTR ) &file->af_Packet.sp_Pkt;
			file->af_Packet.sp_Msg.mn_Node.ln_Type		= NT_MESSAGE;
			file->af_Packet.sp_Msg.mn_Length		= sizeof( struct StandardPacket );

			if( mode == MODE_READ )
			{
				/* if we are in read mode, send out the first read packet to
				 * the file system. While the application is getting ready to
				 * read data, the file system will happily fill in this buffer
				 * with DMA transfers, so that by the time the application
				 * needs the data, it will be in the buffer waiting
				 */

				file->af_Packet.sp_Pkt.dp_Type	= ACTION_READ;
				file->af_BytesLeft		= 0;

				/* MH: We set the offset to the buffer not being filled, in
				 * order to avoid special case code in SeekAsync. ReadAsync
				 * isn't affected by this, since af_BytesLeft == 0.
				 */
				file->af_Offset = file->af_Buffers[ 1 ];

				if( file->af_Handler )
				{
					AS_SendPacket( file, file->af_Buffers[ 0 ] );
				}
			}
			else
			{
				file->af_Packet.sp_Pkt.dp_Type	= ACTION_WRITE;
				file->af_BytesLeft		= file->af_BufferSize;
				file->af_Offset			= file->af_Buffers[ 0 ];
			}
		}
		else
		{
			if( closeIt )
			{
				Close( handle );
			}
		}
	}

	return( file );
}
//-------------------------------------
// AS_SendPacket
/* send out an async packet to the file system. */
VOID
AS_SendPacket( struct AsyncFile *file, APTR arg2 )
{
#ifdef ASIO_NOEXTERNALS
	struct ExecBase	*SysBase;

	SysBase = file->af_SysBase;
#endif

	file->af_Packet.sp_Pkt.dp_Port = &file->af_PacketPort;
	file->af_Packet.sp_Pkt.dp_Arg2 = ( LONG ) arg2;
	PutMsg( file->af_Handler, &file->af_Packet.sp_Msg );
	file->af_PacketPending = TRUE;
}
//-------------------------------------
// AS_WaitPacket
/* this function waits for a packet to come back from the file system. If no
 * packet is pending, state from the previous packet is returned. This ensures
 * that once an error occurs, it state is maintained for the rest of the life
 * of the file handle.
 *
 * This function also deals with IO errors, bringing up the needed DOS
 * requesters to let the user retry an operation or cancel it.
 */
LONG
AS_WaitPacket( AsyncFile *file )
{
#ifdef ASIO_NOEXTERNALS
	struct ExecBase		*SysBase = file->af_SysBase;
	struct DosLibrary	*DOSBase = file->af_DOSBase;
#endif
	LONG bytes;

	if( file->af_PacketPending )
	{
		while( TRUE )
		{
			/* This enables signalling when a packet comes back to the port */
			file->af_PacketPort.mp_Flags = PA_SIGNAL;

			/* Wait for the packet to come back, and remove it from the message
			 * list. Since we know no other packets can come in to the port, we can
			 * safely use Remove() instead of GetMsg(). If other packets could come in,
			 * we would have to use GetMsg(), which correctly arbitrates access in such
			 * a case
			 */
			Remove( ( struct Node * ) WaitPort( &file->af_PacketPort ) );

			/* set the port type back to PA_IGNORE so we won't be bothered with
			 * spurious signals
			 */
			file->af_PacketPort.mp_Flags = PA_IGNORE;

			/* mark packet as no longer pending since we removed it */
			file->af_PacketPending = FALSE;

			bytes = file->af_Packet.sp_Pkt.dp_Res1;

			if( bytes >= 0 )
			{
				/* packet didn't report an error, so bye... */
				return( bytes );
			}

			/* see if the user wants to try again... */
			if( ErrorReport( file->af_Packet.sp_Pkt.dp_Res2, REPORT_STREAM, file->af_File, NULL ) )
			{
				return( -1 );
			}

			/* user wants to try again, resend the packet */
			AS_SendPacket( file,
				file->af_Buffers[ file->af_ReadMode ?
					file->af_CurrentBuf :
					1 - file->af_CurrentBuf ] );
		}
	}

	/* last packet's error code, or 0 if packet was never sent */
	SetIoErr( file->af_Packet.sp_Pkt.dp_Res2 );

	return( file->af_Packet.sp_Pkt.dp_Res1 );
}
//-------------------------------------
// AS_RequeuePacket( AsyncFile *file )
/* this function puts the packet back on the message list of our
 * message port.
 */
VOID
AS_RequeuePacket( AsyncFile *file )
{
#ifdef ASIO_NOEXTERNALS
	struct ExecBase	*SysBase = file->af_SysBase;
#endif

	AddHead( &file->af_PacketPort.mp_MsgList, &file->af_Packet.sp_Msg.mn_Node );
	file->af_PacketPending = TRUE;
}
//-------------------------------------

//-------------------------------------
// AsyncFile *OpenAsync()

#ifdef ASIO_NOEXTERNALS
_LIBCALL AsyncFile *
OpenAsync(
	_REG( a0 ) const STRPTR fileName,
	_REG( d0 ) OpenModes mode,
	_REG( d1 ) LONG bufferSize,
	_REG( a1 ) struct ExecBase *SysBase,
	_REG( a2 ) struct DosLibrary *DOSBase )
#else
_LIBCALL AsyncFile *
OpenAsync(
	_REG( a0 ) const STRPTR fileName,
	_REG( d0 ) OpenModes mode,
	_REG( d1 ) LONG bufferSize )
#endif
{
	static const WORD PrivateOpenModes[] =
	{
		MODE_OLDFILE, MODE_NEWFILE, MODE_READWRITE
	};
	BPTR		handle;
	AsyncFile	*file = NULL;

	if(( handle = Open( fileName, PrivateOpenModes[ mode ] ) ))
	{
#ifdef ASIO_NOEXTERNALS
		file = AS_OpenAsyncFH( handle, mode, bufferSize, TRUE, SysBase, DOSBase );
#else
		file = AS_OpenAsyncFH( handle, mode, bufferSize, TRUE );
#endif

		if( !file )
		{
			Close( handle );
		}
	}

	return( file );
}
//-------------------------------------
// LONG CloseAsync(AsyncFile *file )
_LIBCALL LONG
CloseAsync( _REG( a0 ) AsyncFile *file )
{
	LONG	result;

	if( file )
	{
#ifdef ASIO_NOEXTERNALS
		struct ExecBase		*SysBase = file->af_SysBase;
		struct DosLibrary	*DOSBase = file->af_DOSBase;
#endif
		result = AS_WaitPacket( file );

		if( result >= 0 )
		{
			if( !file->af_ReadMode )
			{
				/* this will flush out any pending data in the write buffer */
				if( file->af_BufferSize > file->af_BytesLeft )
				{
					result = Write(
						file->af_File,
						file->af_Buffers[ file->af_CurrentBuf ],
						file->af_BufferSize - file->af_BytesLeft );
				}
			}
		}

		if( file->af_CloseFH )
		{
			Close( file->af_File );
		}

		FreeVec(file);
	}
	else
	{
#ifndef ASIO_NOEXTERNALS
		SetIoErr( ERROR_INVALID_LOCK );
#endif
		result = -1;
	}

	return( result );
}
//-------------------------------------
// LONG WriteAsync( AsyncFile *file, APTR buffer, LONG numBytes )

_LIBCALL LONG
WriteAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG numBytes )
{
#ifdef ASIO_NOEXTERNALS
	struct ExecBase	*SysBase = file->af_SysBase;
#endif
	LONG totalBytes = 0;

	/* this takes care of NIL: */
	if( !file->af_Handler )
	{
		file->af_Offset		= file->af_Buffers[ 0 ];
		file->af_BytesLeft	= file->af_BufferSize;
		return( numBytes );
	}

	while( numBytes > file->af_BytesLeft )
	{
		if( file->af_BytesLeft )
		{
			CopyMem( buffer, file->af_Offset, file->af_BytesLeft );

			numBytes	-= file->af_BytesLeft;
			buffer		=  ( APTR ) ( ( ULONG ) buffer + file->af_BytesLeft );
			totalBytes	+= file->af_BytesLeft;
		}

		if( AS_WaitPacket( file ) < 0 )
		{
			return( -1 );
		}

		/* send the current buffer out to disk */
		AS_SendPacket( file, file->af_Buffers[ file->af_CurrentBuf ] );

		file->af_CurrentBuf	= 1 - file->af_CurrentBuf;
		file->af_Offset		= file->af_Buffers[ file->af_CurrentBuf ];
		file->af_BytesLeft	= file->af_BufferSize;
	}

	CopyMem( buffer, file->af_Offset, numBytes );
	file->af_BytesLeft	-= numBytes;
	file->af_Offset		+= numBytes;

	return ( totalBytes + numBytes );
}
//-------------------------------------
// LONG WriteCharAsync( AsyncFile *file, UBYTE ch )
_CALL LONG
WriteCharAsync( _REG( a0 ) AsyncFile *file, _REG( d0 ) UBYTE ch )
{
	if( file->af_BytesLeft )
	{
		/* if there's any room left in the current buffer, directly write
		 * the byte into it, updating counters and stuff.
		 */

		*file->af_Offset = ch;
		--file->af_BytesLeft;
		++file->af_Offset;

		/* one byte written */
		return( 1 );
	}

	/* there was no room in the current buffer, so call the main write
	 * routine. This will effectively send the current buffer out to disk,
	 * wait for the other buffer to come back, and then put the byte into
	 * it.
	 */

	{
		TEXT	c;

		c = ch;		/* SAS/C workaround... */

		return( WriteAsync( file, &c, 1 ) );
	}
}
//-------------------------------------
// LONG ReadAsync( AsyncFile *file, APTR buffer, LONG numBytes )
_LIBCALL LONG
ReadAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG numBytes )
{
#ifdef ASIO_NOEXTERNALS
	struct ExecBase	*SysBase = file->af_SysBase;
#endif
	LONG totalBytes = 0;
	LONG bytesArrived;

	/* if we need more bytes than there are in the current buffer, enter the
	 * read loop
	 */

	while( numBytes > file->af_BytesLeft )
	{
		/* drain buffer */
		CopyMem( file->af_Offset, buffer, file->af_BytesLeft );

		numBytes		-= file->af_BytesLeft;
		buffer			=  ( APTR ) ( ( ULONG ) buffer + file->af_BytesLeft );
		totalBytes		+= file->af_BytesLeft;
		file->af_BytesLeft	=  0;

		bytesArrived = AS_WaitPacket( file );

		if( bytesArrived <= 0 )
		{
			if( bytesArrived == 0 )
			{
				return( totalBytes );
			}

			return( -1 );
		}

		/* ask that the buffer be filled */
		AS_SendPacket( file, file->af_Buffers[ 1 - file->af_CurrentBuf ] );

		/* in case we tried to seek past EOF */
		if( file->af_SeekOffset > bytesArrived )
		{
			file->af_SeekOffset = bytesArrived;
		}

		file->af_Offset		= file->af_Buffers[ file->af_CurrentBuf ] + file->af_SeekOffset;
		file->af_CurrentBuf	= 1 - file->af_CurrentBuf;
		file->af_BytesLeft	= bytesArrived - file->af_SeekOffset;
		file->af_SeekOffset	= 0;
	}

	CopyMem( file->af_Offset, buffer, numBytes );
	file->af_BytesLeft	-= numBytes;
	file->af_Offset		+= numBytes;

	return( totalBytes + numBytes );
}
//-------------------------------------


