#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "utils.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
PFLT_PORT 	gServerPort;
PFLT_PORT 	gClientPort;
ULONG_PTR OperationStatusCtx = 1;

LIST_ENTRY FileFilter;

#define PTDBG_TRACE_ROUTINES           0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define MINISPY_PORT_NAME L"\\FileRedirectPort"

ULONG gTraceFlags = PTDBG_TRACE_ROUTINES;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

//  Defines the commands between the utility and the filter
typedef enum _MINI_COMMAND {
	ENUM_ADD_FILTER = 0,
	ENUM_CLEAR_FILTER
} MINI_COMMAND;

//  Defines the command structure between the utility and the filter.
typedef struct _FILEFILTERITEM {
	PWSTR sourcePath;
	PWSTR redirectPath;
} FILEFILTERITEM, *PFILEFILTERITEM;


// Defines the structure in driver
typedef struct _FILEFILTERITEMINLIST {
	FILEFILTERITEM filter;
	LIST_ENTRY 	listEntry;
} FILEFILTERITEMINLIST, *PFILEFILTERITEMINLIST;


//  Defines the command structure between the utility and the filter.
typedef struct _COMMAND_MESSAGE {
	MINI_COMMAND 	Command;
	FILEFILTERITEM filter;
} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;



/*************************************************************************
Prototypes
*************************************************************************/

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
_In_ PDRIVER_OBJECT DriverObject,
_In_ PUNICODE_STRING RegistryPath
);


NTSTATUS
FileRedirectUnload(
_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
FileRedirectPreCreate(
_Inout_ PFLT_CALLBACK_DATA Data,
_In_ PCFLT_RELATED_OBJECTS FltObjects,
_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);


FLT_POSTOP_CALLBACK_STATUS
FileRedirectPostCreate(
_Inout_ PFLT_CALLBACK_DATA Data,
_In_ PCFLT_RELATED_OBJECTS FltObjects,
_In_opt_ PVOID CompletionContext,
_In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS
MiniConnect(
__in PFLT_PORT ClientPort,
__in PVOID ServerPortCookie,
__in_bcount(SizeOfContext) PVOID ConnectionContext,
__in ULONG SizeOfContext,
__deref_out_opt PVOID *ConnectionCookie
);

VOID
MiniDisconnect(
__in_opt PVOID ConnectionCookie
);

NTSTATUS
MiniMessage(
__in PVOID ConnectionCookie,
__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
__in ULONG InputBufferSize,
__out_bcount_part_opt(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
__in ULONG OutputBufferSize,
__out PULONG ReturnOutputBufferLength
);

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FileRedirectUnload)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

		{ IRP_MJ_CREATE,
		0,
		FileRedirectPreCreate,
		NULL },
		{ IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags

	NULL,                               //  Context
	Callbacks,                          //  Operation callbacks

	FileRedirectUnload,                           //  MiniFilterUnload

	NULL,                    //  InstanceSetup
	NULL,            //  InstanceQueryTeardown
	NULL,            //  InstanceTeardownStart
	NULL,         //  InstanceTeardownComplete

	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};