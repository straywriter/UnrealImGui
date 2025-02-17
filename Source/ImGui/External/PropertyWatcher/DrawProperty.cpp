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



void FPropertyWatcher::DrawPropertyValue(PropertyItem& Item)
{
	static TArray<char> StringBuffer;
	static int IntStep = 1;
	static int IntStepFast8 = 10;
	static int IntStepFast = 100;
	static int64 Int64Step = 1;
	static int64 Int64StepFast = 100;
	//static float FloatStepFast = 1;
	//static double DoubleStepFast = 100;

	bool DragEnabled = ImGui::IsKeyDown(ImGuiMod_Alt);

	if (Item.Ptr == 0)
	{
		ImGui::Text("<Null>");
	}
	else if (Item.Prop == 0)
	{
		ImGui::Text("{%d}", Item.GetMemberCount());
	}
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Item.Prop))
	{
		UClass* Class = (UClass*)Item.Ptr;
		ImGui::Text(ImGui_StoA(*Class->GetAuthoredName()));
	}
	else if (FClassPtrProperty* ClassPtrProp = CastField<FClassPtrProperty>(Item.Prop))
	{
		ImGui::Text("<Not Implemented>");
	}
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Item.Prop))
	{
		FSoftObjectPtr* SoftClass = (FSoftObjectPtr*)Item.Ptr;
		if (SoftClass->IsStale())
			ImGui::Text("<Stale>");
		else if (SoftClass->IsPending())
			ImGui::Text("<Pending>");
		else if (!SoftClass->IsValid())
			ImGui::Text("<Null>");
		else
		{
			FString Path = SoftClass->ToSoftObjectPath().ToString();
			if (Path.Len())
				ImGuiAddon::InputString("##SoftClassProp", Path, StringBuffer);
		}
	}
	else if (FWeakObjectProperty* WeakObjProp = CastField<FWeakObjectProperty>(Item.Prop))
	{
		TWeakObjectPtr<UObject>* WeakPtr = (TWeakObjectPtr<UObject>*)Item.Ptr;
		if (WeakPtr->IsStale())
			ImGui::Text("<Stale>");
		if (!WeakPtr->IsValid())
			ImGui::Text("<Null>");
		else
		{
			auto NewItem = MakeObjectItem(WeakPtr->Get());
			DrawPropertyValue(NewItem);
		}
	}
	else if (FLazyObjectProperty* LayzObjProp = CastField<FLazyObjectProperty>(Item.Prop))
	{
		TLazyObjectPtr<UObject>* LazyPtr = (TLazyObjectPtr<UObject>*)Item.Ptr;
		if (LazyPtr->IsStale())
			ImGui::Text("<Stale>");
		if (!LazyPtr->IsValid())
			ImGui::Text("<Null>");
		else
		{
			auto NewItem = MakeObjectItem(LazyPtr->Get());
			DrawPropertyValue(NewItem);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		FString ID = LazyPtr->GetUniqueID().ToString();
		ImGuiAddon::InputString("##LazyObjectProp", ID, StringBuffer);
	}
	else if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Item.Prop))
	{
		TSoftObjectPtr<UObject>* SoftObjPtr = (TSoftObjectPtr<UObject>*)Item.Ptr;
		if (SoftObjPtr->IsPending())
			ImGui::Text("<Pending>");
		else if (!SoftObjPtr->IsValid())
			ImGui::Text("<Null>");
		else
		{
			FString Path = SoftObjPtr->ToSoftObjectPath().ToString();
			if (Path.Len())
				ImGuiAddon::InputString("##SoftObjProp", Path, StringBuffer);
		}
	}
	else if (Item.Type == PointerType::Function)
	{
		if (ImGui::Button("Call Function"))
		{
			UObject* Obj = (UObject*)Item.Ptr;
			UFunction* Function = (UFunction*)Item.StructPtr;
			static char buf[256];
			Obj->ProcessEvent(Function, buf);
		}
	}
	else if (Item.Type == PointerType::Object ||
		Item.Prop->IsA(FObjectProperty::StaticClass()))
	{
		int MemberCount = Item.GetMemberCount();
		if (MemberCount)
			ImGui::Text("{%d}", MemberCount);
		else
			ImGui::Text("{}");
	}
	else if (CastField<FByteProperty>(Item.Prop) && CastField<FByteProperty>(Item.Prop)->IsEnum())
	{
		FByteProperty* ByteProp = CastField<FByteProperty>(Item.Prop);
		if (ByteProp->Enum)
		{
			UEnum* Enum = ByteProp->Enum;
			int Count = Enum->NumEnums();

			StringBuffer.Empty();
			for (int i = 0; i < Count; i++)
			{
				FString Name = Enum->GetNameStringByIndex(i);
				StringBuffer.Append(ImGui_StoA(*Name), Name.Len());
				StringBuffer.Push('\0');
			}
			StringBuffer.Push('\0');
			uint8* Value = (uint8*)Item.Ptr;
			int TempInt = *Value;
			if (ImGui::Combo(*TMem.Printf("##Enum_%p", Item.Prop), &TempInt, StringBuffer.GetData(), Count))
				*Value = TempInt;
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Item.Prop))
	{
		bool TempBool = BoolProp->GetPropertyValue(Item.Ptr);
		ImGui::Checkbox(*TMem.Printf("##Checkbox_%p", BoolProp), &TempBool);
		BoolProp->SetPropertyValue(Item.Ptr, TempBool);
	}
	else if (Item.Prop->IsA(FInt8Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FInt8Property_%p", Item.Prop), ImGuiDataType_S8, Item.Ptr, &IntStep,
		                   &IntStepFast8);
	}
	else if (Item.Prop->IsA(FByteProperty::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FByteProperty_%p", Item.Prop), ImGuiDataType_U8, Item.Ptr, &IntStep,
		                   &IntStepFast8);
	}
	else if (Item.Prop->IsA(FInt16Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FInt16Property_%p", Item.Prop), ImGuiDataType_S16, Item.Ptr, &IntStep,
		                   &IntStepFast);
	}
	else if (Item.Prop->IsA(FUInt16Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FUInt16Property_%p", Item.Prop), ImGuiDataType_U16, Item.Ptr, &IntStep,
		                   &IntStepFast);
	}
	else if (Item.Prop->IsA(FIntProperty::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FIntProperty_%p", Item.Prop), ImGuiDataType_S32, Item.Ptr, &IntStep,
		                   &IntStepFast);
	}
	else if (Item.Prop->IsA(FUInt32Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FUInt32Property_%p", Item.Prop), ImGuiDataType_U32, Item.Ptr, &IntStep,
		                   &IntStepFast);
	}
	else if (Item.Prop->IsA(FInt64Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FInt64Property_%p", Item.Prop), ImGuiDataType_S64, Item.Ptr, &Int64Step,
		                   &Int64StepFast);
	}
	else if (Item.Prop->IsA(FUInt64Property::StaticClass()))
	{
		ImGui::InputScalar(*TMem.Printf("##FUInt64Property_%p", Item.Prop), ImGuiDataType_U64, Item.Ptr, &Int64Step,
		                   &Int64StepFast);
	}
	else if (Item.Prop->IsA(FFloatProperty::StaticClass()))
	{
		//ImGui::IsItemHovered
		//if(!DragEnabled)
		ImGui::InputFloat(*TMem.Printf("##FFloatProperty_%p", Item.Prop), (float*)Item.Ptr);
		//else 
		//ImGui::DragFloat("##FFloatProperty", (float*)Item.Ptr, 1.0f);
	}
	else if (Item.Prop->IsA(FDoubleProperty::StaticClass()))
	{
		ImGui::InputDouble(*TMem.Printf("##FDoubleProperty_%p", Item.Prop), (double*)Item.Ptr);
	}
	else if (Item.Prop->IsA(FStrProperty::StaticClass()))
	{
		ImGuiAddon::InputString(*TMem.Printf("##StringProp_%p", Item.Prop), *(FString*)Item.Ptr, StringBuffer);
	}
	else if (Item.Prop->IsA(FNameProperty::StaticClass()))
	{
		FString Str = ((FName*)Item.Ptr)->ToString();
		if (ImGuiAddon::InputString(*TMem.Printf("##NameProp_%p", Item.Prop), Str, StringBuffer)) (*((FName*)Item.Ptr))
			= FName(Str);
	}
	else if (Item.Prop->IsA(FTextProperty::StaticClass()))
	{
		FString Str = ((FText*)Item.Ptr)->ToString();
		if (ImGuiAddon::InputString(*TMem.Printf("##TextProp_%p", Item.Prop), Str, StringBuffer)) (*((FText*)Item.Ptr))
			= FText::FromString(Str);
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Item.Prop))
	{
		FProperty* CurrentProp = ArrayProp->Inner;
		FScriptArrayHelper ScriptArrayHelper(ArrayProp, Item.Ptr);
		ImGui::Text("%s [%d]", ImGui_StoA(*CurrentProp->GetCPPType()), ScriptArrayHelper.Num());
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(Item.Prop))
	{
		FScriptMapHelper Helper = FScriptMapHelper(MapProp, Item.Ptr);
		ImGui::Text("<%s, %s> (%d)", ImGui_StoA(*MapProp->KeyProp->GetCPPType()),
		            ImGui_StoA(*MapProp->ValueProp->GetCPPType()), Helper.Num());
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(Item.Prop))
	{
		FScriptSetHelper Helper = FScriptSetHelper(SetProp, Item.Ptr);
		ImGui::Text("<%s> {%d}", ImGui_StoA(*Helper.GetElementProperty()->GetCPPType()), Helper.Num());
	}
	else if (FMulticastDelegateProperty* MultiDelegateProp = CastField<FMulticastDelegateProperty>(Item.Prop))
	{
		auto ScriptDelegate = (TMulticastScriptDelegate<FWeakObjectPtr>*)Item.Ptr;
		//MultiDelegateProp->SignatureFunction->
		//ScriptDelegate->ProcessMulticastDelegate(asdf);
		//auto BoundObjects = ScriptDelegate->GetAllObjects();
		//if (!MemberArray) return BoundObjects.Num();

		//for (auto Obj : BoundObjects)
		//	MemberArray->Push(MakeObjectItem(Obj));

		// ImGui::BeginDisabled(); defer{ ImGui::EndDisabled(); };

		ImGui::PushID(TCHAR_TO_ANSI(*(MultiDelegateProp->GetName())));
		if (ImGui::Button("Broadcast"))
		{
			// @Todo
			UE_LOG(LogTemp, Warning, TEXT("TODO: Implement Multicast Delegate Broadcast"));
		}
		ImGui::PopID();
	}
	else if (FDelegateProperty* DelegateProp = CastField<FDelegateProperty>(Item.Prop))
	{
		auto ScriptDelegate = (TScriptDelegate<FWeakObjectPtr>*)Item.Ptr;
		FString Text;
		if (ScriptDelegate->IsBound()) Text = ScriptDelegate->GetFunctionName().ToString();
		else Text = "<No Function Bound>";
		ImGui::Text(ImGui_StoA(*Text));
	}
	else if (FMulticastInlineDelegateProperty* MultiInlineDelegateProp = CastField<
		FMulticastInlineDelegateProperty>(Item.Prop))
	{
		ImGui::Text("<NotImplemented>"); // @Todo
	}
	else if (FMulticastSparseDelegateProperty* MultiSparseDelegateProp = CastField<
		FMulticastSparseDelegateProperty>(Item.Prop))
	{
		ImGui::Text("<NotImplemented>"); // @Todo
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Item.Prop))
	{
		FString Extended;
		FString StructType = StructProp->GetCPPType(&Extended, 0);

		if (StructType == "FVector")
		{
			ImGui::InputScalarN("##FVector", ImGuiDataType_Double, &((FVector*)Item.Ptr)->X, 3);
		}
		else if (StructType == "FRotator")
		{
			ImGui::InputScalarN("##FVector", ImGuiDataType_Double, &((FRotator*)Item.Ptr)->Pitch, 3);
		}
		else if (StructType == "FVector2D")
		{
			ImGui::InputScalarN("##FVector2D", ImGuiDataType_Double, &((FVector2D*)Item.Ptr)->X, 2);
		}
		else if (StructType == "FIntVector")
		{
			ImGui::InputInt3("##FIntVector", &((FIntVector*)Item.Ptr)->X);
		}
		else if (StructType == "FIntPoint")
		{
			ImGui::InputInt2("##FIntPoint", &((FIntPoint*)Item.Ptr)->X);
		}
		else if (StructType == "FTimespan")
		{
			FString s = ((FTimespan*)Item.Ptr)->ToString();
			if (ImGuiAddon::InputString("##FTimespan", s, StringBuffer))
				FTimespan::Parse(s, *((FTimespan*)Item.Ptr));
		}
		else if (StructType == "FDateTime")
		{
			FString s = ((FDateTime*)Item.Ptr)->ToString();
			if (ImGuiAddon::InputString("##FDateTime", s, StringBuffer))
				FDateTime::Parse(s, *((FDateTime*)Item.Ptr));
		}
		else if (StructType == "FLinearColor")
		{
			FLinearColor* lCol = (FLinearColor*)Item.Ptr;
			FColor sCol = lCol->ToFColor(true);
			float c[4] = {sCol.R / 255.0f, sCol.G / 255.0f, sCol.B / 255.0f, sCol.A / 255.0f};
			if (ImGui::ColorEdit4("##FLinearColor", c, ImGuiColorEditFlags_AlphaPreview))
			{
				sCol = FColor(c[0] * 255, c[1] * 255, c[2] * 255, c[3] * 255);
				*lCol = FLinearColor::FromSRGBColor(sCol);
			}
		}
		else if (StructType == "FColor")
		{
			FColor* sCol = (FColor*)Item.Ptr;
			float c[4] = {sCol->R / 255.0f, sCol->G / 255.0f, sCol->B / 255.0f, sCol->A / 255.0f};
			if (ImGui::ColorEdit4("##FColor", c, ImGuiColorEditFlags_AlphaPreview))
				*sCol = FColor(c[0] * 255, c[1] * 255, c[2] * 255, c[3] * 255);
		}
		else
		{
			ImGui::Text("{%d}", Item.GetMemberCount());
		}
	}
	else
	{
		ImGui::Text("<UnknownType>");
	}
}