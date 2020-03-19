#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "utils.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES           0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = PTDBG_TRACE_ROUTINES;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/*************************************************************************
    Prototypes
*************************************************************************/

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );


NTSTATUS
FileRedirectUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FileRedirectPreCreate (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );


FLT_POSTOP_CALLBACK_STATUS
FileRedirectPostCreate (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
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

    sizeof( FLT_REGISTRATION ),         //  Size
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

/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )

{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileRedirect!DriverEntry: Entered\n") );

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
FileRedirectUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileRedirect!FileRedirectUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
FileRedirectPreCreate (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )

{
    NTSTATUS status;

	PWCHAR source_path = { 0 };
	PWCHAR target_path = { 0 };
	PWCHAR current_path = { 0 };
	UNICODE_STRING uni_current_path = { 0 };
	PFILE_OBJECT FileObject;
	PFLT_FILE_NAME_INFORMATION nameInfo;

	BOOLEAN IsDirectory = FALSE;

	PUNICODE_PREFIX_TABLE_ENTRY PrefixTableEntry = NULL;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );


	source_path = L"\\Device\\HarddiskVolume1\\Documents and Settings\\N\\Application Data\\Notepad++";
	target_path = L"\\Device\\HarddiskVolume2\\Notepad++";

	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED , &nameInfo);
	if (NT_SUCCESS(status)) {
	
		if (Data->Iopb->Parameters.Create.Options & FILE_DIRECTORY_FILE | FILE_NON_DIRECTORY_FILE){
			//FltParseFileNameInformation(&nameInfo);

			if (wcsstr(nameInfo->Name.Buffer, source_path) != NULL  ){
				current_path = (PWSTR)ExAllocatePool(PagedPool, BUFFER_SIZE);
				RtlZeroMemory(current_path, BUFFER_SIZE);
				RtlCopyMemory(current_path, nameInfo->Name.Buffer, nameInfo->Name.Length);
				wcsrep(current_path, source_path, target_path);

				RtlInitUnicodeString(&uni_current_path, current_path);
				DbgPrint("src %wZ \n", &nameInfo->Name);
				DbgPrint("uni %wZ \n", &uni_current_path);
				FileObject = Data->Iopb->TargetFileObject;
				FileObject->FileName = uni_current_path;
				Data->IoStatus.Information = IO_REPARSE;
				Data->IoStatus.Status = STATUS_REPARSE;

				FltReleaseFileNameInformation(nameInfo);
				ExFreePool(current_path);
				return FLT_PREOP_COMPLETE;
			}
		}

		FltReleaseFileNameInformation(nameInfo);
	}

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
