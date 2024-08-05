#pragma once
#include <ntifs.h>

typedef struct _KTIMER_TABLE_ENTRY {
	
	KSPIN_LOCK Lock;
	_LIST_ENTRY Entry;
	_ULARGE_INTEGER Time;
}_KTIMER_TABLE_ENTRY;


typedef struct _KTIMER_TABLE {
	
	PKTIMER TimerExpiry [64];
	_KTIMER_TABLE_ENTRY TimerEntries [2][256];

}_KTIMER_TABLE;

typedef struct _KPRCB {
	char pad0 [0x3940];
	_KTIMER_TABLE TimerTable;
	
}_KPRCB;