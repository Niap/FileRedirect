#include "FileRedirect.h"


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
	PSECURITY_DESCRIPTOR sd;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING uniString;

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

	//__debugbreak();
    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //
		InitializeListHead(&FileFilter);
		
        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

	if (!NT_SUCCESS(status)) {
		return status;
	}


	RtlInitUnicodeString(&uniString, MINISPY_PORT_NAME);

	InitializeObjectAttributes(&oa,
		&uniString,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		sd);

	status = FltCreateCommunicationPort(gFilterHandle,
		&gServerPort,
		&oa,
		NULL,
		MiniConnect,
		MiniDisconnect,
		MiniMessage,
		1);

	FltFreeSecurityDescriptor(sd);


    return status;
}

VOID addFilter(PWSTR source_path, PWSTR redirect_path){
	PFILEFILTERITEMINLIST item = (PFILEFILTERITEMINLIST)ExAllocatePool(PagedPool, sizeof(FILEFILTERITEMINLIST));
	item->filter.sourcePath = (PWSTR)ExAllocatePool(PagedPool, BUFFER_SIZE);

	RtlZeroMemory(item->filter.sourcePath, BUFFER_SIZE);
	RtlCopyMemory(item->filter.sourcePath, source_path, wcslen(source_path)*sizeof(WCHAR));

	item->filter.redirectPath = (PWSTR)ExAllocatePool(PagedPool, BUFFER_SIZE);
	RtlZeroMemory(item->filter.redirectPath, BUFFER_SIZE);
	RtlCopyMemory(item->filter.redirectPath, redirect_path, wcslen(redirect_path)*sizeof(WCHAR));

	InsertHeadList(&FileFilter, &item->listEntry);
}

VOID clearFilter(){
	PLIST_ENTRY plist;
	PFILEFILTERITEMINLIST pFileFlterItemInList;
	while (!IsListEmpty(&FileFilter)){
		plist = RemoveHeadList(&FileFilter);
		pFileFlterItemInList = CONTAINING_RECORD(plist, FILEFILTERITEMINLIST, listEntry);
		ExFreePool(pFileFlterItemInList);
	}
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

	
	FltCloseCommunicationPort(gServerPort);
    FltUnregisterFilter( gFilterHandle );
	clearFilter();
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

	FILEFILTERITEM fileFilterItem = { 0 };
	PLIST_ENTRY p = NULL;
	PFILEFILTERITEMINLIST pffil;

	PWCHAR current_path = { 0 };
	UNICODE_STRING uni_current_path = { 0 };
	PFILE_OBJECT FileObject;
	PFLT_FILE_NAME_INFORMATION nameInfo;

	BOOLEAN IsDirectory = FALSE;

	PUNICODE_PREFIX_TABLE_ENTRY PrefixTableEntry = NULL;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );


	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED , &nameInfo);
	if (NT_SUCCESS(status)) {
	
		if (Data->Iopb->Parameters.Create.Options & FILE_DIRECTORY_FILE | FILE_NON_DIRECTORY_FILE){
			//FltParseFileNameInformation(&nameInfo);
			if (nameInfo->Stream.Length == 0){
				current_path = (PWSTR)ExAllocatePool(PagedPool, BUFFER_SIZE);
				RtlZeroMemory(current_path, BUFFER_SIZE);
				RtlCopyMemory(current_path, nameInfo->Name.Buffer, nameInfo->Name.Length);
				for (p = FileFilter.Blink; p != &FileFilter; p = p->Blink){
					pffil = CONTAINING_RECORD(p, FILEFILTERITEMINLIST, listEntry);
					if (wcsstr(current_path, pffil->filter.sourcePath) != NULL){
						
						wcsrep(current_path, pffil->filter.sourcePath, pffil->filter.redirectPath);
						RtlInitUnicodeString(&uni_current_path, current_path);
						FileObject = Data->Iopb->TargetFileObject;
						FileObject->FileName = uni_current_path;
						Data->IoStatus.Information = IO_REPARSE;
						Data->IoStatus.Status = STATUS_REPARSE;

						FltReleaseFileNameInformation(nameInfo);
						return FLT_PREOP_COMPLETE;
					}
				}
			}
			
		}

		FltReleaseFileNameInformation(nameInfo);
	}

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

//communicate 

NTSTATUS
MiniConnect(
__in PFLT_PORT ClientPort,
__in PVOID ServerPortCookie,
__in_bcount(SizeOfContext) PVOID ConnectionContext,
__in ULONG SizeOfContext,
__deref_out_opt PVOID *ConnectionCookie
)
{
	DbgPrint("[mini-filter] NPMiniConnect\n");
	PAGED_CODE();

	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);

	ASSERT(gClientPort == NULL);
	gClientPort = ClientPort;
	return STATUS_SUCCESS;
}

VOID
MiniDisconnect(
__in_opt PVOID ConnectionCookie
)
{
	PAGED_CODE();
	UNREFERENCED_PARAMETER(ConnectionCookie);
	DbgPrint("[mini-filter] NPMiniDisconnect\n");

	//  Close our handle
	FltCloseClientPort(gFilterHandle, &gClientPort);
}

NTSTATUS
MiniMessage(
__in PVOID ConnectionCookie,
__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
__in ULONG InputBufferSize,
__out_bcount_part_opt(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
__in ULONG OutputBufferSize,
__out PULONG ReturnOutputBufferLength
)
{

	MINI_COMMAND command;
	NTSTATUS status;
	FILEFILTERITEM filterItem;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(ConnectionCookie);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);

	DbgPrint("[mini-filter] NPMiniMessage");


	if ((InputBuffer != NULL) &&
		(InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE, Command) +
		sizeof(MINI_COMMAND)))) {

		try  {
			//  Probe and capture input message: the message is raw user mode
			//  buffer, so need to protect with exception handler
			command = ((PCOMMAND_MESSAGE)InputBuffer)->Command;
			filterItem = ((PCOMMAND_MESSAGE)InputBuffer)->filter;

		} except(EXCEPTION_EXECUTE_HANDLER) {

			return GetExceptionCode();
		}

		switch (command) {
		case ENUM_ADD_FILTER:
		{
			addFilter(filterItem.sourcePath,filterItem.redirectPath);
			status = STATUS_SUCCESS;
			break;
		}
		case ENUM_CLEAR_FILTER:
		{
			clearFilter();
			status = STATUS_SUCCESS;
			break;
		}

		default:
			DbgPrint("[mini-filter] default");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
	}
	else {

		status = STATUS_INVALID_PARAMETER;
	}

	return status;
}