Question "Write a driver to enumerate all timers on the system, Explain why the DPC data assocated with the timer does not seem to make sense"

Answer : "The DPC data is encrypted inside the KeSetTimerEx, this way : "Timer->Dpc = (KiWaitNever ^ __ROR8__(Timer ^ _byteswap_uint64(Dpc ^ KiWaitAlways), KiWaitNever));"

Oh also this is my pseudo code of the `KiInsertTimerTable`, 

```c++
BOOLEAN KiInsertTimerTable(PKRCB Prcb, PKTIMER Timer, PKDPC Dpc, ULONG Index, UINT64 SomeVar) 
{
	
	UINT16 Processor = 0;
	unsigned int InsertionStatus = 0;
	BOOL Active = TRUE;
	
	LARGE_INTEGER DueTime = Timer->DueTime;
	
	/* Check if the period is zero */
	if(!Timer->Period)  Timer->Header.SignalState = FALSE;
	
	/* Trying to determine the Processor to associate */
	if(!KiSerializeTimerExpiration)
	{
		/* Does it exceeds the maximum processor count ? */
		if(Dpc && Dpc->Nmber >= MAX_PROCESSORS)
		{
			/* Use the relative DPC's number */
			Processor = Dpc->Nmber - MAX_PROCESSORS;
		}
		else
		{
			Processor = Prcb->Number; 
			
			/* Determine the best processor within the node */
			auto Var_Affinity = Prcb->ParentNode->NonParkedSet & Prcb->ParentNode->Affinity.Mask
			
			if(Var_Affinity)
			{
				AUTO LeastBitPos;
				UCHAR GroupIndex = Prcb->GroupIndex;
				UCHAR Group = Prcb->Group;
				
				/* Perform a right rotation of GroupIndex bytes on the Var_Affinity */
				Var_Affinity = __ROR8__(Var_Affinity, groupIndex);
				
				 /* Find the least significant bit set to 1 and returns its index */
				_BitScanForward64(&LeastBitPos,Var_Affinity);
				
				/* Compute the index based on the Affinity mask, GroupIndex, Group */
				auto Index = 64 * Group +  ( (LeastBitPos + groupIndex) & 0x3F)
				
				/* Get the Processor Index */
				Processor = KiProcessorNumberToIndexMappingTable[Index]; 
				
			}
			
		}
		
		
	}
	
	Timer->Processor = Processor;
	
	PKRCB currentPrcb = KiProcessorBlock[Processor];
	
	_KTIMER_TABLE_ENTRY* TimerTableEntry = &currentPrcb->TimerTable.TimerEntries[Timer->TimerType][Index];
		
	// Wait on the lock (lock bts) 
	WaitOnLock(TimerTableEntry->Lock);
	
	PLIST_ENTRY ListHead = &TimerTableEntry->Entry;
	
	PLIST_ENTRY NextEntry = TimerTableEntry->Entry.Flink;
	PLIST_ENTRY LastEntry = ListHead->Blink;
	PLIST_ENTRY Entry;
	
	/* The list is empty */
	if(NextEntry == ListHead)
	{
		/* The new timer will be the first entry in the list */
		InsertionStatus = 6; 
	}
	else
	{
		PKTIMER CurrentTimer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
		
		if(DueTime > CurrentTimer->DueTime.QuadPart)
		{

			/* Determine where to start the loop depending on the timer DueTime */
			if( DueTime - CurrentTimer->DueTime.QuadPart <= KeMaximumIncrement >> 2 )
			{				
				/* Loop the timer list, ascending order  */
				while(NextEntry != ListHead)
				{
					CurrentTimer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
					
					/* The Timer to insert have a lower/equal Expiration time, it can fit */
					if(DueTime <= CurrentTimer->DueTime.QuadPart) break;
					
					// Next one 
					NextEntry = NextEntry->Flink;
				}
				
			}
			/* Loop the timer list, descending order  */
			else
			{
				// Start at last entry
				NextEntry = LastEntry;
				
				while(NextEntry != ListHead)
				{
					CurrentTimer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
					
					/* The Timer to insert have a greater/equal Expiration time, it can fit */
					if(DueTime >= CurrentTimer->DueTime.QuadPart) break;
					
					// Next one 
					NextEntry = NextEntry->Blink;
				}
			}
			
		}
		else
		{
			
			if(DueTime < CurrentTimer->Timer.QuadPart)
			{
				/* The new timer have the lowest DueTime */
				InsertionStatus = 2; 
			}
				
		}
		
	}
		
	/* Insert it after the NextEntry */
	InsertTailList(NextEntry, &Timer->TimerListEntry)
	
	
	/* 
	Theses conditions requires a check to see if it has already expired 
		The timer is the first entry
		The timer have the lowest DueTime, 
	*/
	if(InsertionStatus == 6 || InsertionStatus == 2 )
	{
		/* The new timer is the first entry, perform some actions */
		if(InsertionStatus == 6) 
		{
			auto BitPosition;
			auto IndexOffset;
		
			/* Compute the index and offset to use on KiPendingTimerBitmaps depending the Serialization status */ 
			if ( KiSerializeTimerExpiration )
			{
				BitPosition = Index & 0x3F;
				IndexOffset = 8 * (Index >> 6);
			}
			else
			{
				BitPosition = currentPrcb->GroupIndex;
				IndexOffset = Index << 6;
			}
		
		/* Calculate the address of the entry in the KiPendingTimerBitmaps array */
		auto KiPendingTimerBitmapsEntry = KiPendingTimerBitmaps+8[currentPrcb->Group * 2]
		
		/* Set the bit at BitPosition in the KiPendingTimerBitmapsEntry using an atomic operation */
		/* This indicates that there is now at least one timer pending in this slot. */
		
		_interlockedbittestandset64(KiPendingTimerBitmapsEntry + IndexOffset, BitPosition );
			
		}
		
		/* Does the Inserted Timer expired ? */
		if(DueTime <= KI_USER_SHARED_DATA->InterruptTime)
		{
			/* The timer has expired */
			Active = FALSE;
			
			/* Remove the Timer  */ 
			KiRemoveEntryTimer(currentPrcb->TimerTable, Timer, Index, ListHead);
			
		}
		
	}
	
	// Set the lock to 0 (lock and) 
	ReleaseTheLock(TimerTableEntry->Lock);
	
	
	return Active;
	
}
```
