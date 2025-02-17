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


bool FPropertyWatcher::MemberPath::UpdateItemFromPath(TArray<PropertyItem>& Items)
{
	// Name is the "path" to the member. You can traverse through objects, structs and arrays.
	// E.g.: objectMember.<arrayIndex>.structMember.float/int/bool member

	TArray<FString> MemberArray;
	PathString.ParseIntoArray(MemberArray, TEXT("."));

	if (MemberArray.IsEmpty()) return false;

	bool SearchFailed = false;

	// Find first name in items.
	PropertyItem CurrentItem;
	{
		bool Found = false;
		for (auto& It : Items)
			if (FString(It.GetAuthoredName()) == MemberArray[0])
			{
				//@Fix
				CurrentItem = It;
				Found = true;
			}

		if (!Found) SearchFailed = true;
		MemberArray.RemoveAt(0);
	}

	if (!SearchFailed)
		for (auto& MemberName : MemberArray)
		{
			//if (!CurrentItem->IsValid()) return false;

			// (Membername=Value)
			bool SearchByMemberValue = false;
			//FString MemberNameToTest;
			//FString MemberValueToTest;
			//if (MemberName[0] == '(' && MemberName[MemberName.Len()-1] == ')') {
			//	SearchByMemberValue = true;

			//	MemberName.RemoveAt(0);
			//	MemberName.RemoveAt(MemberName.Len() - 1);

			//	bool Result = MemberName.Split("=", &MemberNameToTest, &MemberValueToTest);
			//	if(!Result) { SearchFailed = true; break; }
			//}

			TArray<PropertyItem> Members;
			CurrentItem.GetMembers(&Members);
			bool Found = false;
			for (auto MemberItem : Members)
			{
				FString ItemName = FString(MemberItem.GetAuthoredName()); //@Fix

				if (SearchByMemberValue)
				{
					//if (ItemName == MemberNameToTest) {
					//	MemberItem.
					//}
				}
				else
				{
					if (ItemName == MemberName)
					{
						CurrentItem = MemberItem;
						Found = true;
						break;
					}
				}
			}
			if (!Found)
			{
				SearchFailed = true;
				break;
			}
		}


	if (!SearchFailed)
		CachedItem = CurrentItem;
	else
		CachedItem = {};

	// Have to set this either way, because we want to see the path in the watch window.
	CachedItem.NameOverwrite = TMem.SToA(PathString);

	return !SearchFailed;
}


void FPropertyWatcher::SimpleSearchParser::ParseExpression(FAView str, TArray<FAView> _Columns)
{
	Commands.Empty();

	struct StackInfo
	{
		TArray<Test> Tests = {{}};
		TArray<Operator> OPs;
	};
	TArray<StackInfo> Stack = {{}};

	auto EatToken = [&str](FAView Token) -> bool
	{
		if (str.StartsWith(Token))
		{
			str.RemovePrefix(Token.Len());
			return true;
		}
		return false;
	};

	auto PushedWord = [&]()
	{
		for (int i = Stack.Last().OPs.Num() - 1; i >= 0; i--)
			Commands.Push({Command_Op, {}, Stack.Last().OPs[i]});

		if (Stack.Last().Tests.Num() > 1)
			if (!Stack.Last().OPs.Contains(OP_Or))
				Commands.Push({Command_Op, {}, OP_And});

		Stack.Last().OPs.Empty();
		Stack.Last().Tests.Push({});
	};

	while (true)
	{
		str.TrimStartInline();
		if (str.IsEmpty()) break;

		if (EatToken("|")) Stack.Last().OPs.Push(OP_Or);
		else if (EatToken("!")) Stack.Last().OPs.Push(OP_Not);
		else if (EatToken("+")) Stack.Last().Tests.Last().Mod = Mod_Exact;
		else if (EatToken("r:")) Stack.Last().Tests.Last().Mod = Mod_Regex;
		else if (EatToken("<=")) Stack.Last().Tests.Last().Mod = Mod_LessEqual;
		else if (EatToken(">=")) Stack.Last().Tests.Last().Mod = Mod_GreaterEqual;
		else if (EatToken("<")) Stack.Last().Tests.Last().Mod = Mod_Less;
		else if (EatToken(">")) Stack.Last().Tests.Last().Mod = Mod_Greater;
		else if (EatToken("=")) Stack.Last().Tests.Last().Mod = Mod_Equal;
		else if (EatToken("(")) Stack.Push({});
		else if (EatToken(")"))
		{
			Stack.Pop();
			if (!Stack.Num()) break;
			PushedWord();
		}
		else
		{
			// Column Name
			{
				bool Found = false;
				for (int i = 0; i < _Columns.Num(); i++)
				{
					auto Col = _Columns[i];
					if (str.StartsWith(Col, ESearchCase::IgnoreCase))
					{
						if (str.RightChop(Col.Len()).StartsWith(':'))
						{
							Stack.Last().Tests.Last().ColumnID = i;
							// ColumnID should always be the index for this to work.
							str.RemovePrefix(Col.Len() + 1);
							Found = true;
							break;
						}
					}
				}
				if (Found) continue;
			}

			if (EatToken("\""))
			{
				int Index;
				if (str.FindChar('"', Index))
				{
					Stack.Last().Tests.Last().Ident = str.Left(Index);
					str.RemovePrefix(Index + 1);

					Commands.Push({Command_Test, Stack.Last().Tests.Last()});
					PushedWord();
				}
				continue;
			}

			// Word
			{
				int Index = 0;
				while (Index < str.Len() && ((str[Index] >= 'A' && str[Index] <= 'Z') || (str[Index] >= 'a' && str[
						Index] <= 'z') ||
					(str[Index] >= '0' && str[Index] <= '9') || str[Index] == '_'))
					Index++;

				// For now we skip chars we don't know.
				if (!Index)
				{
					str.RemovePrefix(1);
					continue;
				}

				Stack.Last().Tests.Last().Ident = str.Left(Index);
				str.RemovePrefix(Index);

				Commands.Push({Command_Test, Stack.Last().Tests.Last()});
				PushedWord();
			}
		}
	}
}

bool FPropertyWatcher::SimpleSearchParser::ApplyTests(CachedColumnText& ColumnTexts)
{
	TArray<bool> Bools;
	for (auto Command : Commands)
	{
		if (Command.Type == Command_Test)
		{
			Test& Tst = Command.Tst;
			FAView* FoundString = ColumnTexts.Get(Tst.ColumnID);
			if (!FoundString)
				continue;
			FAView ColStr = *FoundString;

			bool Result;
			if (!Tst.Mod) Result = StringView_Contains<ANSICHAR>(ColStr, Tst.Ident);
			else if (Tst.Mod == Mod_Exact) Result = ColStr.Equals(Tst.Ident, ESearchCase::IgnoreCase);
			else if (Tst.Mod == Mod_Equal) Result = ColStr.Len()
				                                        ? FCStringAnsi::Atod(*ColStr) == FCStringAnsi::Atod(*Tst.Ident)
				                                        : false;
			else if (Tst.Mod == Mod_Greater) Result = ColStr.Len()
				                                          ? FCStringAnsi::Atod(*ColStr) > FCStringAnsi::Atod(*Tst.Ident)
				                                          : false;
			else if (Tst.Mod == Mod_Less) Result = ColStr.Len()
				                                       ? FCStringAnsi::Atod(*ColStr) < FCStringAnsi::Atod(*Tst.Ident)
				                                       : false;
			else if (Tst.Mod == Mod_GreaterEqual) Result = ColStr.Len()
				                                               ? FCStringAnsi::Atod(*ColStr) >= FCStringAnsi::Atod(
					                                               *Tst.Ident)
				                                               : false;
			else if (Tst.Mod == Mod_LessEqual) Result = ColStr.Len()
				                                            ? FCStringAnsi::Atod(*ColStr) <= FCStringAnsi::Atod(
					                                            *Tst.Ident)
				                                            : false;

			else if (Tst.Mod == Mod_Regex)
			{
				FRegexMatcher RegMatcher(FRegexPattern(*Tst.Ident), *ColStr);
				Result = RegMatcher.FindNext();
			}
			Bools.Push(Result);
		}
		else if (Command.Type == Command_Op)
		{
			if (Command.Op == OP_And)
			{
				if (Bools.Num() > 1)
				{
					Bools[Bools.Num() - 2] &= Bools.Last();
					Bools.Pop();
				}
			}
			else if (Command.Op == OP_Or)
			{
				if (Bools.Num() > 1)
				{
					Bools[Bools.Num() - 2] |= Bools.Last();
					Bools.Pop();
				}
			}
			else if (Command.Op == OP_Not)
				Bools.Last() = !Bools.Last();
		}
	}

	if (Bools.Num())
		return Bools[0];
	else
		return false;
}


FAView FPropertyWatcher::SimpleSearchParser::Command::ToString()
{
	if (Type == Command_Test)
	{
		FAView s = TMem.Printf("%d: %s", Tst.ColumnID, *Tst.Ident);
		if (Tst.Mod)
		{
			if (Tst.Mod == Mod_Exact) TMem.Append(&s, "[Exact]");
			else if (Tst.Mod == Mod_Regex) TMem.Append(&s, "[Regex]");
			else if (Tst.Mod == Mod_Equal) TMem.Append(&s, "[Equal]");
			else if (Tst.Mod == Mod_Greater) TMem.Append(&s, "[Greater]");
			else if (Tst.Mod == Mod_Less) TMem.Append(&s, "[Less]");
			else if (Tst.Mod == Mod_GreaterEqual) TMem.Append(&s, "[GreaterEqual]");
			else if (Tst.Mod == Mod_LessEqual) TMem.Append(&s, "[LessEqual]");
		}
		return s;
	}
	else if (Type == Command_Op)
	{
		if (Op == OP_And) return "AND";
		else if (Op == OP_Or) return "OR";
		else if (Op == OP_Not) return "NOT";
	}
	else if (Type == Command_Store) return "STORE";

	return "";
}
