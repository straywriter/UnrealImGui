#include "PropertyWatcher.h"
#include "EngineUtils.h"
#include "imgui_internal.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Engine/StaticMeshActor.h"

#include "GameFramework/PlayerController.h"

#include "UObject/Stack.h"
#include "Engine/Level.h"

#include "GameFramework/Actor.h"

#include "Misc/ExpressionParser.h"
#include "Internationalization/Regex.h"

#include "Misc/TextFilterUtils.h"
#include <inttypes.h> // For printing address.

// Switch on to turn on unreal insights events for performance tests.
#define SCOPE_EVENT(name) 
//#define SCOPE_EVENT(name) SCOPED_NAMED_EVENT_TEXT(name, FColor::Orange);

#if WITH_EDITORONLY_DATA
#define MetaData_Available true
#else 
#define MetaData_Available false
#endif


// -------------------------------------------------------------------------------------------

char* FPropertyWatcher::TempMemoryPool::MemBucket::Get(int Count)
{
	char* Result = Data + Position;
	Position += Count;
	return Result;
}

void FPropertyWatcher::TempMemoryPool::Init(int _StartSize)
{
	if (!IsInitialized)
	{
		*this = {};
		IsInitialized = true;
		StartSize = _StartSize;
	}

	if (Buckets.IsEmpty())
		AddBucket();
}

void FPropertyWatcher::TempMemoryPool::AddBucket()
{
	int BucketSize = Buckets.Num() == 0 ? StartSize : Buckets.Last().Size * 2; // Double every bucket.
	MemBucket Bucket = {};
	Bucket.Data = (char*)FMemory::Malloc(BucketSize);
	Bucket.Size = BucketSize;
	Buckets.Add(Bucket);
}

void FPropertyWatcher::TempMemoryPool::ClearAll()
{
	for (auto& It : Buckets)
		FMemory::Free(It.Data);
	Buckets.Empty();
	CurrentBucketIndex = 0;

	Markers.Empty();
}

void FPropertyWatcher::TempMemoryPool::GoToNextBucket()
{
	if (!Buckets.IsValidIndex(CurrentBucketIndex + 1))
		AddBucket();
	CurrentBucketIndex++;
}

char* FPropertyWatcher::TempMemoryPool::Get(int Count)
{
	while (!GetCurrentBucket().MemoryFits(Count))
		GoToNextBucket();

	return GetCurrentBucket().Get(Count);
}

void FPropertyWatcher::TempMemoryPool::Free(int Count)
{
	int BytesToFree = Count;
	while (true)
	{
		auto Bucket = GetCurrentBucket();
		int BucketBytesToFree = FMath::Min(Bucket.Position, BytesToFree);
		Bucket.Free(BucketBytesToFree);
		BytesToFree -= BucketBytesToFree;
		if (BytesToFree)
			break;
		GoToPrevBucket();
	}
}

void FPropertyWatcher::TempMemoryPool::PushMarker()
{
	Markers.Add({CurrentBucketIndex, Buckets[CurrentBucketIndex].Position});
}

void FPropertyWatcher::TempMemoryPool::FreeToMarker()
{
	auto& Marker = Markers.Last();
	while (CurrentBucketIndex != Marker.BucketIndex)
	{
		GetCurrentBucket().Position = 0;
		GoToPrevBucket();
	}
	GetCurrentBucket().Position = Marker.DataPosition;
}

void FPropertyWatcher::TempMemoryPool::PopMarker(bool Free)
{
	if (Free)
		FreeToMarker();
	Markers.Pop(false);
}

// Copied from FString::Printf().
FAView FPropertyWatcher::TempMemoryPool::Printf(const char* Fmt, ...)
{
	int BufferSize = 128;
	ANSICHAR* Buffer = 0;

	int ResultSize;
	while (true)
	{
		int Count = BufferSize * sizeof(ANSICHAR);
		Buffer = (ANSICHAR*)Get(Count);
		GET_VARARGS_RESULT_ANSI(Buffer, BufferSize, BufferSize - 1, Fmt, Fmt, ResultSize);

		if (ResultSize != -1)
			break;

		Free(Count);
		BufferSize *= 2;
	};

	Buffer[ResultSize] = '\0';

	FAView View(Buffer, ResultSize);
	return View;
}

// Copied partially from StringCast.
FAView FPropertyWatcher::TempMemoryPool::CToA(const TCHAR* SrcBuffer, int SrcLen)
{
	int StringLength = TStringConvert<TCHAR, ANSICHAR>::ConvertedLength(SrcBuffer, SrcLen);
	int32 BufferSize = StringLength;
	ANSICHAR* Buffer = (ANSICHAR*)Get(BufferSize + 1);
	TStringConvert<TCHAR, ANSICHAR>::Convert(Buffer, BufferSize, SrcBuffer, SrcLen);
	Buffer[BufferSize] = '\0';

	FAView View(Buffer, BufferSize);
	return View;
}

// I wish there was a way to get the FName data ansi pointer directly instead of having to copy it.
FAView FPropertyWatcher::TempMemoryPool::NToA(FName Name)
{
	const FNameEntry* NameEntry = Name.GetDisplayNameEntry();
	if (NameEntry->IsWide())
		return "<WideFNameError>";

	int Len = NameEntry->GetNameLength();
	ANSICHAR* Buffer = Get(Len + 1);
	NameEntry->GetAnsiName((ANSICHAR(&)[NAME_SIZE])(*Buffer));

	FAView View(Buffer, Len);
	return View;
}

