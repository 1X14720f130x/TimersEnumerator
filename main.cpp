#include "Structs.h"
#include <wdmsec.h> // IoCreateDeviceSecure
#include <TraceLoggingProvider.h>
#include <evntrace.h>

/*
 I developed this driver as a quick solution for a book project.  It provided the correct results.
	 However, I am not a kernel programming expert, I'm still learning, so please be aware that the code below may contain errors.
*/

/*
This driver enumerates all timers on the system.

It logs information about each timers using Event Tracing for Windows (ETW).
*/

#pragma warning(disable : 4100) // Unreferenced parameter

// {B6FAA951-0C06-46B4-BF9A-49519BA138D9}
TRACELOGGING_DEFINE_PROVIDER( g_Provider , "TimersEnumerator" , (0xb6faa951 , 0xc06 , 0x46b4 , 0xbf , 0x9a , 0x49 , 0x51 , 0x9b , 0xa1 , 0x38 , 0xd9 ));

#define IOCTL_ENUM_TIMERS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define SYMBOLIC_NAME L"\\??\\TimersEnumerator"
#define DEVICE_NAME L"\\Device\\TimersEnumerator"



void DriverUnload( _In_ PDRIVER_OBJECT DriverObject );
NTSTATUS DriverDispatchCreateClose( _In_ PDEVICE_OBJECT DeviceObject , _Inout_  PIRP Irp );
NTSTATUS DriverDispatchControl( _In_ PDEVICE_OBJECT DeviceObject , _Inout_  PIRP Irp );
void EnumerateAndLogTimers( const PLIST_ENTRY ListHead );

UNICODE_STRING gSymbolicName;




extern "C" NTSTATUS DriverEntry( PDRIVER_OBJECT DriverObject , PUNICODE_STRING RegisteryPath ) {

	TraceLoggingRegister( g_Provider );

	NTSTATUS status;
	UNICODE_STRING deviceName;
	UNICODE_STRING SDDLstring;
	PDEVICE_OBJECT DeviceObject;


	DriverObject->DriverUnload = DriverUnload;

	RtlInitUnicodeString( &deviceName , DEVICE_NAME );
	RtlInitUnicodeString( &gSymbolicName , SYMBOLIC_NAME );
	RtlInitUnicodeString( &SDDLstring , L"D:P(A;;GA;;;SY)(A;;GRGWGX;;;BA)" ); // SYSTEM + Administrators only 

	status = IoCreateDeviceSecure( DriverObject , 0 , &deviceName , FILE_DEVICE_UNKNOWN , FILE_DEVICE_SECURE_OPEN , false , &SDDLstring , nullptr , &DeviceObject );

	if ( !NT_SUCCESS( status ) ) return status;

	status = IoCreateSymbolicLink( &gSymbolicName , &deviceName );

	if ( !NT_SUCCESS( status ) ) {

		IoDeleteDevice( DriverObject->DeviceObject );
		return status;
	}


	DriverObject->MajorFunction [IRP_MJ_CREATE] = &DriverDispatchCreateClose;
	DriverObject->MajorFunction [IRP_MJ_CLOSE] = &DriverDispatchCreateClose;
	DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = &DriverDispatchControl;


	return STATUS_SUCCESS;
}


void DriverUnload( _In_ PDRIVER_OBJECT DriverObject ) {



	IoDeleteSymbolicLink( &gSymbolicName );
	IoDeleteDevice( DriverObject->DeviceObject );
	TraceLoggingUnregister( g_Provider );

	return;
}

NTSTATUS DriverDispatchCreateClose( _In_ PDEVICE_OBJECT DeviceObject , _Inout_  PIRP Irp ) {

	UNREFERENCED_PARAMETER( DeviceObject );

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = NULL;

	IoCompleteRequest( Irp , 0 );

	return STATUS_SUCCESS;

}

NTSTATUS DriverDispatchControl( _In_ PDEVICE_OBJECT DeviceObject , _Inout_  PIRP Irp ) {

	NTSTATUS status { STATUS_SUCCESS };
	constexpr ULONG_PTR information { };

	auto pIrpStackLocation = IoGetCurrentIrpStackLocation( Irp );

	switch ( pIrpStackLocation->Parameters.DeviceIoControl.IoControlCode ) {
	case IOCTL_ENUM_TIMERS:
	{

		KIRQL Irql;

		KAFFINITY ActiveProcBitmask = KeQueryActiveProcessors( );

		for ( ULONG ProcNumber = 0; ( ActiveProcBitmask >> ProcNumber ) & 0x01; ProcNumber++ ) {

			// Change the processor to the specified one 
			KeSetSystemAffinityThread( ( KAFFINITY ) ( ( ULONG_PTR ) 1 << ProcNumber ) );

			// Raise to Dispatch_level preventing timer execution and potential timers list modification
			KeRaiseIrql( DISPATCH_LEVEL , &Irql );

			// Get the TimerTable associated with the current processor
			_KTIMER_TABLE* TimerTable = &KeGetPcr( )->CurrentPrcb->TimerTable;

			// Iterate over each entry inside the TimerTable
			for ( int i = 0; i < 2; i++ ) {

				for ( int j = 0; j < 256; j++ ) {

					_KTIMER_TABLE_ENTRY* TimerTableEntry = &TimerTable->TimerEntries [i][j];

					// Lock the TimerTable entry 
					KeAcquireSpinLockAtDpcLevel( &TimerTableEntry->Lock );

					// Enumerate and log the timers
					EnumerateAndLogTimers( &TimerTableEntry->Entry );

					// Release the lock of the TimerTable entry 
					KeReleaseSpinLockFromDpcLevel( &TimerTableEntry->Lock );
					

				}
			}
			// Back to PASSIVE_LEVEL, allowing timers to execute and list to be modified on the current processor 
			KeLowerIrql( Irql );

		}
		 // Restore the original affinity
		KeRevertToUserAffinityThread( );

		break;

	}
	default:
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	}


	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = information;
	IoCompleteRequest( Irp , IO_NO_INCREMENT );


	return status;
}

void EnumerateAndLogTimers( const PLIST_ENTRY ListHead ) 	{

	for ( auto NextEntry = ListHead->Flink; NextEntry != ListHead; NextEntry = NextEntry->Flink ) 	{

		PKTIMER Timer = CONTAINING_RECORD( NextEntry , KTIMER , TimerListEntry );

		TraceLoggingWrite( g_Provider ,
						   "Timer Enumeration" ,
						   TraceLoggingLevel( TRACE_LEVEL_INFORMATION ) ,
						   TraceLoggingPointer( Timer , "Timer" ) ,
						   TraceLoggingValue( ( Timer->TimerType == NotificationTimer ) ? "NotificationTimer" : "SynchronizationTimer" , "Type" ) ,
						   TraceLoggingUInt64( Timer->Period , "Period" ) ,
						   TraceLoggingUInt64( Timer->DueTime.QuadPart , "DueTime" ),
						   TraceLoggingUInt64( Timer->Processor , "Processor" )
		);
	}

	return;

}
