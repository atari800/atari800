#ifndef _ASYNC_H_
#define _ASYNC_H_

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <clib/asyncio_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#ifdef __MAXON__
#include <pragma/exec_lib.h>
#include <pragma/dos_lib.h>
#else
#include <pragmas/exec_sysbase_pragmas.h>
#include <pragmas/dos_pragmas.h>
#endif

#include <string.h>


/*****************************************************************************/


#ifdef _DCC
	#ifdef ASIO_SHARED_LIB
		#define _LIBCALL __geta4 _ASM _ARGS
	#else
		#define _LIBCALL _ASM _ARGS
	#endif
#else
	#ifdef __GNUC__
		#define _LIBCALL
	#else
		#ifdef __MAXON__
		  #define _LIBCALL
		#else  /* __SASC__ */
			#ifdef ASIO_SHARED_LIB
				#define _LIBCALL __saveds _ASM _ARGS
			#else
				#define _LIBCALL _ASM _ARGS
			#endif
		#endif
	#endif /* _ GNUC_  */
#endif /* _DCC */

#define _CALL _ASM _ARGS


/*****************************************************************************/


#ifndef ASIO_NOEXTERNALS
extern struct DosLibrary	*DOSBase;
extern struct ExecBase		*SysBase;
#endif

#ifdef ASIO_SHARED_LIB
extern struct ExecBase		*SysBase;
extern struct Library		*UtilityBase;
extern struct DosLibrary	*DOSBase;
#endif


/*****************************************************************************/


/* this macro lets us long-align structures on the stack */
#define D_S(type,name) char a_##name[ sizeof( type ) + 3 ]; \
			type *name = ( type * ) ( ( LONG ) ( a_##name + 3 ) & ~3 );

#ifndef MIN
#define MIN(a,b) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#endif


/*****************************************************************************/


#ifdef ASIO_NOEXTERNALS
AsyncFile *
AS_OpenAsyncFH( BPTR handle, OpenModes mode, LONG bufferSize, BOOL closeIt, struct ExecBase *SysBase, struct DosLibrary *DOSBase );
#else
AsyncFile *
AS_OpenAsyncFH( BPTR handle, OpenModes mode, LONG bufferSize, BOOL closeIt );
#endif
VOID AS_SendPacket( AsyncFile *file, APTR arg2 );
LONG AS_WaitPacket( AsyncFile *file );
VOID AS_RequeuePacket( AsyncFile *file );
VOID AS_RecordSyncFailure( AsyncFile *file );

#endif /* _ASYNC_H_ */
