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


FPropertyWatcher::PropertyItem FPropertyWatcher::MakeObjectItem(void* _Ptr)
{
    return {PointerType::Object, _Ptr};
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakeObjectItemNamed(void* _Ptr, const char* _NameOverwrite,
                                                                     FAView NameID)
{
    return MakeObjectItemNamed(_Ptr, FAView(_NameOverwrite));
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakeObjectItemNamed(void* _Ptr, FString _NameOverwrite, FAView NameID)
{
    TMem.Init(TMemoryStartSize);
    return MakeObjectItemNamed(_Ptr, TMem.SToA(_NameOverwrite));
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakeObjectItemNamed(void* _Ptr, FAView _NameOverwrite, FAView NameID)
{
    return {PointerType::Object, _Ptr, 0, _NameOverwrite, 0, NameID};
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakeArrayItem(void* _Ptr, FProperty* _Prop, int _Index,
                                                               bool IsObjectProp)
{
    return {PointerType::Property, _Ptr, _Prop, TMem.Printf("[%d]", _Index)};
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakePropertyItem(void* _Ptr, FProperty* _Prop)
{
    return {PointerType::Property, _Ptr, _Prop};
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakePropertyItemNamed(void* _Ptr, FProperty* _Prop, FAView Name,
                                                                       FAView NameID)
{
    return {PointerType::Property, _Ptr, _Prop, Name, 0, NameID};
}

FPropertyWatcher::PropertyItem FPropertyWatcher::MakeFunctionItem(void* _Ptr, UFunction* _Function)
{
    return {PointerType::Function, _Ptr, 0, "", _Function};
}


void* FPropertyWatcher::ContainerToValuePointer(PointerType Type, void* ContainerPtr, FProperty* MemberProp)
{
    switch (Type)
    {
    case Object:
        {
            if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(MemberProp))
                return ObjectProp->GetObjectPropertyValue_InContainer(ContainerPtr);
            else
                return MemberProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        }
        break;
    case Array:
        {
            if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(MemberProp))
                return ObjectProp->GetObjectPropertyValue_InContainer(ContainerPtr);
            else
                return ContainerPtr; // Already memberPointer in the case of arrays.
        }
        break;
    case Map:
        {
            if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(MemberProp))
                return *(UObject**)ContainerPtr; // Not sure why this is different than from arrays.
            else
                return ContainerPtr; // Already memberPointer.
        }
        break;
    }

    return 0;
}


FString FPropertyWatcher::ConvertWatchedMembersToString(TArray<FPropertyWatcher::MemberPath>& WatchedMembers)
{
    TArray<FString> Strings;
    for (auto It : WatchedMembers)
        Strings.Push(It.PathString);

    FString Result = FString::Join(Strings, TEXT(","));
    return Result;
}

void FPropertyWatcher::LoadWatchedMembersFromString(FString Data, TArray<FPropertyWatcher::MemberPath>& WatchedMembers)
{
    WatchedMembers.Empty();

    TArray<FString> Strings;
    Data.ParseIntoArray(Strings, TEXT(","));
    for (auto It : Strings)
    {
        FPropertyWatcher::MemberPath Path = {};
        Path.PathString = It;

        WatchedMembers.Push(Path);
    }
}

