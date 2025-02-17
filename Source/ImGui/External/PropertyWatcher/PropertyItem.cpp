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

#if WITH_EDITORONLY_DATA
#define MetaData_Available true
#else 
#define MetaData_Available false
#endif

FName FPropertyWatcher::PropertyItem::GetName()
{
	if (Type == PointerType::Property && Prop)
		return Prop->GetFName();

	if (Type == PointerType::Object && Ptr)
		return ((UObject*)Ptr)->GetFName();

	if (StructPtr && (Type == PointerType::Struct || Type == PointerType::Function))
		return StructPtr->GetFName();
	//return StructPtr->GetAuthoredName();

	return NAME_None;
}

FAView FPropertyWatcher::PropertyItem::GetAuthoredName()
{
	return !NameOverwrite.IsEmpty() ? NameOverwrite : TMem.NToA(GetName());
}

//FString PropertyItem::GetDisplayName() {
//	FString Result = GetAuthoredName();
//	if (Type == PointerType::Property && CastField<FObjectProperty>(Prop) && Ptr)
//		Result = FString::Printf(TEXT("%s (%s)"), *Result, *((UObject*)Ptr)->GetName());
//
//	return Result;
//}

bool FPropertyWatcher::PropertyItem::IsExpandable()
{
	if (!IsValid())
		return false;

	if (!Ptr || Type == PointerType::Function)
		return false;

	if (Type == PointerType::Object || Type == PointerType::Struct)
		return true;

	if (Prop->IsA(FArrayProperty::StaticClass()) ||
		Prop->IsA(FMapProperty::StaticClass()) ||
		Prop->IsA(FSetProperty::StaticClass()) ||
		Prop->IsA(FObjectProperty::StaticClass()) ||
		Prop->IsA(FWeakObjectProperty::StaticClass()) ||
		Prop->IsA(FLazyObjectProperty::StaticClass()) ||
		Prop->IsA(FSoftObjectProperty::StaticClass()))
		return true;

	if (Prop->IsA(FStructProperty::StaticClass()))
	{
		FStructProperty* StructProp = CastField<FStructProperty>(Prop);

		// @Todo: These shouldn't be hardcoded.
		static TSet<FName> TypeSetX = {
			"Vector", "Rotator", "Vector2D", "IntVector2", "IntVector", "Timespan", "DateTime"
		};

		FName StructType = StructProp->Struct->GetFName();
		bool Inlined = TypeSetX.Contains(StructType);
		return !Inlined;
	}

	if (Prop->IsA(FDelegateProperty::StaticClass()))
		return true;

	if (Prop->IsA(FMulticastDelegateProperty::StaticClass()))
		return true;

	return false;
}

FAView FPropertyWatcher::PropertyItem::GetPropertyType()
{
	FAView Result = "";
	if (Type == PointerType::Property && Prop)
		Result = TMem.NToA(Prop->GetClass()->GetFName());

	else if (Type == PointerType::Object)
		Result = "";

	else if (Type == PointerType::Struct)
		Result = "";

	return Result;
};

FAView FPropertyWatcher::PropertyItem::GetCPPType()
{
	if (Type == PointerType::Property && Prop)
		return TMem.SToA(Prop->GetCPPType());

	if (Type == PointerType::Struct)
		return TMem.SToA(((UScriptStruct*)StructPtr)->GetStructCPPName());

	if (Type == PointerType::Object && Ptr)
	{
		UClass* Class = ((UObject*)Ptr)->GetClass();
		if (Class)
			return TMem.NToA(Class->GetFName());
	}

	// Do we really have to do this? Is there no engine function?
	if (Type == PointerType::Function)
	{
		UFunction* Function = (UFunction*)StructPtr;

		FProperty* ReturnProp = Function->GetReturnProperty();
		FString ts = ReturnProp ? ReturnProp->GetCPPType() : "void";

		ts += TEXT(" (");
		int i = 0;
		for (FProperty* MemberProp : TFieldRange<FProperty>(Function))
		{
			if (MemberProp == ReturnProp) continue;
			if (i == 1) ts += TEXT(", ");
			ts += MemberProp->GetCPPType();
			i++;
		}
		ts += TEXT(")");

		return TMem.SToA(ts);
	}

	return "";
};

int FPropertyWatcher::PropertyItem::GetSize()
{
	if (Prop)
		return Prop->GetSize();

	else if (Type == PointerType::Object)
	{
		UClass* Class = ((UObject*)Ptr)->GetClass();
		if (Class)
			return Class->GetPropertiesSize();
	}
	else if (Type == PointerType::Struct)
		return StructPtr->GetPropertiesSize();

	return -1;
};

int FPropertyWatcher::PropertyItem::GetMembers(TArray<PropertyItem>* MemberArray)
{
	if (!Ptr) return 0;

	int Count = 0;

	if (Type == PointerType::Object || CastField<FObjectProperty>(Prop))
	{
		UClass* Class = ((UObject*)Ptr)->GetClass();
		if (!Class) return 0;
		for (FProperty* MemberProp : TFieldRange<FProperty>(Class))
		{
			if (!MemberArray)
			{
				Count++;
				continue;
			}

			void* MemberPtr = ContainerToValuePointer(PointerType::Object, Ptr, MemberProp);
			MemberArray->Push(MakePropertyItem(MemberPtr, MemberProp));
		}
	}
	else if (Prop &&
		(Prop->IsA(FWeakObjectProperty::StaticClass()) ||
			Prop->IsA(FLazyObjectProperty::StaticClass()) ||
			Prop->IsA(FSoftObjectProperty::StaticClass())))
	{
		UObject* Obj = 0;
		bool IsValid = GetObjFromObjPointerProp(*this, Obj);
		if (!IsValid) return 0;
		else return MakeObjectItem(Obj).GetMembers(MemberArray);
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		FScriptArrayHelper ScriptArrayHelper(ArrayProp, Ptr);

		if (!MemberArray) return ScriptArrayHelper.Num();
		FProperty* MemberProp = ArrayProp->Inner;
		for (int i = 0; i < ScriptArrayHelper.Num(); i++)
		{
			void* MemberPtr = ContainerToValuePointer(PointerType::Array, ScriptArrayHelper.GetRawPtr(i), MemberProp);
			MemberArray->Push(MakeArrayItem(MemberPtr, MemberProp, i));
		}
	}
	else if (Type == PointerType::Struct || CastField<FStructProperty>(Prop))
	{
		if (!StructPtr)
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
				StructPtr = StructProp->Struct;

		if (StructPtr)
		{
			for (FProperty* MemberProp : TFieldRange<FProperty>(StructPtr))
			{
				if (!MemberArray)
				{
					Count++;
					continue;
				}

				void* MemberPtr = ContainerToValuePointer(PointerType::Object, Ptr, MemberProp);
				MemberArray->Push(MakePropertyItem(MemberPtr, MemberProp));
			}
		}
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		FScriptMapHelper Helper = FScriptMapHelper(MapProp, Ptr);
		if (!MemberArray) return Helper.Num();

		auto KeyProp = Helper.GetKeyProperty();
		auto ValueProp = Helper.GetValueProperty();
		for (int i = 0; i < Helper.Num(); i++)
		{
			uint8* KeyPtr = Helper.GetKeyPtr(i);
			uint8* ValuePtr = Helper.GetValuePtr(i);
			void* ValuePtr2 = ContainerToValuePointer(PointerType::Map, ValuePtr, ValueProp);

			auto KeyItem = MakeArrayItem(KeyPtr, KeyProp, i);
			auto ValueItem = MakeArrayItem(ValuePtr2, ValueProp, i);
			TMem.Append(&KeyItem.NameOverwrite, " Key");
			TMem.Append(&ValueItem.NameOverwrite, " Value");
			MemberArray->Push(KeyItem);
			MemberArray->Push(ValueItem);
		}
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		FScriptSetHelper Helper = FScriptSetHelper(SetProp, Ptr);
		if (!MemberArray) return Helper.Num();

		FProperty* MemberProp = Helper.GetElementProperty();
		for (int i = 0; i < Helper.Num(); i++)
		{
			void* MemberPtr = Helper.Set->GetData(i, Helper.SetLayout);
			MemberPtr = ContainerToValuePointer(PointerType::Array, MemberPtr, MemberProp);
			MemberArray->Push(MakeArrayItem(MemberPtr, MemberProp, i));
		}
	}
	else if (FDelegateProperty* DelegateProp = CastField<FDelegateProperty>(Prop))
	{
		//DelegateProp->SignatureFunction
		auto ScriptDelegate = (TScriptDelegate<FWeakObjectPtr>*)Ptr;
		if (!MemberArray) return ScriptDelegate->IsBound() ? 1 : 0;
		if (ScriptDelegate->IsBound())
		{
			UFunction* Function = ScriptDelegate->GetUObject()->FindFunction(ScriptDelegate->GetFunctionName());
			if (Function)
				MemberArray->Push(MakeFunctionItem(ScriptDelegate->GetUObject(), Function));
		}
	}
	else if (FMulticastDelegateProperty* c = CastField<FMulticastDelegateProperty>(Prop))
	{
		// We would like to call GetAllObjects(), but can't because the invocation list can be invalid and so the function call would fail.
		// And since there is no way to check if the invocation list is invalid we can't handle this property.

		//auto ScriptDelegate = (TMulticastScriptDelegate<FWeakObjectPtr>*)Ptr;
		//if (ScriptDelegate->IsBound()) {
		//	auto BoundObjects = ScriptDelegate->GetAllObjects();
		//	if (!MemberArray) return BoundObjects.Num();

		//	for (auto Obj : BoundObjects)
		//		MemberArray->Push(MakeObjectItem(Obj));
		//} else
		//	if (!MemberArray) return 0;
	}

	return Count;
}

int FPropertyWatcher::PropertyItem::GetMemberCount()
{
	if (CachedMemberCount != -1)
		return CachedMemberCount;

	CachedMemberCount = GetMembers(0);
	return CachedMemberCount;
}


FAView FPropertyWatcher::GetColumnCellText(PropertyItem& Item, int ColumnID, TreeState* State,
                                           TInlineComponentArray<FAView>* CurrentMemberPath, int* StackIndex)
{
	FAView Result = "";
	if (ColumnID == ColumnID_Name)
	{
		if (CurrentMemberPath && StackIndex)
		{
			bool TopLevelWatchListItem = State->CurrentWatchItemIndex != -1 && (*StackIndex) == 0;
			bool PathIsEditable = TopLevelWatchListItem && State->PathStringPtr;

			if (!PathIsEditable)
			{
				if (State->ForceInlineChildItems && State->InlineStackIndexLimit)
				{
					TMemBuilder(Builder);
					for (int i = State->InlineMemberPathIndexOffset; i < CurrentMemberPath->Num(); i++)
						Builder.Appendf("%s.", (*CurrentMemberPath)[i].GetData());
					Builder.Append(Item.GetAuthoredName());
					Result = *Builder;
				}
				else
					Result = Item.GetAuthoredName();
			}
		}
	}
	else if (ColumnID == ColumnID_Value)
	{
		Result = GetValueStringFromItem(Item);
	}
	else if (ColumnID == ColumnID_Metadata && Item.Prop)
	{
#if MetaData_Available
		if (const TMap<FName, FString>* MetaData = Item.Prop->GetMetaDataMap())
		{
			TArray<FName> Keys;
			MetaData->GenerateKeyArray(Keys);
			TStringBuilder<256> Builder;

			int i = -1;
			for (auto Key : Keys)
			{
				i++;
				if (i != 0)
					Builder.Append("\n\n");
				Builder.Appendf(TEXT("%s:\n\t"), *Key.ToString());
				Builder.Append(*MetaData->Find(Key));
			}
			Result = TMem.CToA(*Builder, Builder.Len());
		}
#endif
	}
	else if (ColumnID == ColumnID_Type)
	{
		Result = Item.GetPropertyType();
	}
	else if (ColumnID == ColumnID_Cpptype)
	{
		Result = Item.GetCPPType();
	}
	else if (ColumnID == ColumnID_Class)
	{
		if (Item.Prop)
		{
			FFieldVariant Owner = ((FField*)Item.Prop)->Owner;
			Result = TMem.NToA(Owner.GetFName());
		}
		else if (Item.Type == PointerType::Function)
		{
			UClass* Class = ((UFunction*)Item.StructPtr)->GetOuterUClass();
			if (Class)
				Result = TMem.NToA(Class->GetFName());
		}
	}
	else if (ColumnID == ColumnID_Category)
	{
		Result = GetItemMetadataCategory(Item);
	}
	else if (ColumnID == ColumnID_Address)
	{
		Result = TMem.Printf("0x%" PRIXPTR "\n", (uintptr_t)Item.Ptr);
	}
	else if (ColumnID == ColumnID_Size)
	{
		int Size = Item.GetSize();
		if (Size != -1)
			Result = TMem.Printf("%d B", Size);
	}

	return Result;
}

FAView FPropertyWatcher::GetValueStringFromItem(PropertyItem& Item)
{
	// Maybe we could just serialize the property to string?
	// Since we don't handle that many types for now we can just do it by hand.
	FAView Result;

	if (!Item.Ptr)
		Result = "Null";

	else if (!Item.Prop)
		Result = "";

	else if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Item.Prop))
		Result = TMem.SToA(NumericProp->GetNumericPropertyValueToString(Item.Ptr));

	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Item.Prop))
		Result = ((bool*)(Item.Ptr)) ? "true" : "false";

	else if (Item.Prop->IsA(FStrProperty::StaticClass()))
		Result = TMem.SToA(*(FString*)Item.Ptr);

	else if (Item.Prop->IsA(FNameProperty::StaticClass()))
		Result = TMem.NToA(*((FName*)Item.Ptr));

	else if (Item.Prop->IsA(FTextProperty::StaticClass()))
		Result = TMem.SToA((FString&)((FText*)Item.Ptr)->ToString());

	return Result;
}
