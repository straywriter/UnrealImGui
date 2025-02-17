/* 
	Development notes:

	Info:
		https://forums.unrealengine.com/t/a-sample-for-calling-a-ufunction-with-reflection/276416/9
		https://ikrima.dev/ue4guide/engine-programming/uobject-reflection/uobject-reflection/

		void UObject::ParseParms( const TCHAR* Parms )
		https://www.cnblogs.com/LynnVon/p/14794664.html

		https://github.com/ocornut/imgui/issues/718
		https://forums.unrealengine.com/t/using-clang-to-compile-on-windows/17478/51

	Bugs:
		- Classes left navigation doesnt work.
		- Blueprint functions have wrong parameters in property cpp type.
		- Delegates don't get inlined?
		- Crash on WeakPointerGet when bp selected camera search for private and disable classes.

		- Error when inlining this PlayerController.AcknowledgedPawn.PlayerState.PawnPrivate.Controller.PlayerInput.DebugExecBindings in watch tab
			and setting inline stack to 2
			Structs don't inline correctly in watch window as first item.
		- '/' can't be searched
		- Actor list should clear on restart because pointers will be invalid. Should stop using statics.

	ToDo:
		- Global settings to change float text input to sliders, and also global settings for drag change speed.
		- Fix blueprint struct members that have _C_0 and so on.
		- Button to clear custom storage like inlining.
		- Handle UberGaphFrame functions.
		- Column texts as value or string to distinguish between text and value filtering.
		- WorldOutline, Actor components, Widget tree.
		- Set favourites on variables, stored via classes/struct types, stored temporarily, should load.
			Should be put on top of list, maybe as "favourites" node.
		- Check performance.
		- Show the column keywords somewhere else and not in the helper popup.

		Easy to implement:
			- Ctrl + 123 to switch between tabs.
			- Make value was updated animations.
			- Make background transparent mode, overlay style.
			- If search input already focused, ctrl+f should select everything.

		Not Important:
			- Handle right clip copy and paste.
			- Export/import everything with json. (Try to use unreal serialization functions in properties.)
			- Watch items can be selected and deleted with del.
			- Selection happens on first cell background?
			- Sorting.
			- Better actors line trace.
			- Make save and restore functions for window settings.
			- Super bonus: Hide things that have changed from base object like in details viewer UE.
			- Ability to modify containers like array and map, add/remove/insert.
			- Look at EFieldIterationFlags
			- Metadata: Use keys: Category, UIMin, UIMax. (Maybe: tooltip, sliderexponent?)
			- Metadata categories: Property->MetaDataMap.
			- Add hot keys for enabling filter, classes, functions.
			- Call functions in watchItemPaths like Manager.SubObject.CallFunc:GetCurrentThing.Member.
			- Make it easier to add unique struct handlings from user side.
			- Do a clang compile test.

		Add before release:
			- Search should be able to do structvar.membervar = 5.
			- Better search: maybe make a mode "hide non value types" that hides a list of things like delegates that you never want to see.
				  Maybe 2 search boxes, second for permanent stuff like hiding delegates, first one for quick searches.

			- Search jump to next result.
			- Detached watch windows.
			- Custom draw functions for data.
				- Add right click "draw as" (rect, whatever).
				- Manipulator widget based on data like position, show in world.
			- Simple function calling.
				Make functions expandable in tree form, and leafs are arguments and return value.
			- Simple delegate calling.
			- Fix maps with touples.
			- Auto complete for watch window members.
			- Property path by member value.
				- Make path with custom getter-> instead of array index look at object and test for name or something.
				- Make custom path by dragging from a value, that should make link to parent node with that value being looked for.

			- Try imgui viewports.

	Ideas:
		- Print container preview up to property value region size, the more you open the column the more you see.
		- Make mode that diffs current values of object with default class variables.
		- To call functions dynamically we could use all our widgets for input and then store the result in an input.
			We could then display it with drawItem, all the information is in the properties of the function, including the result property.
		- Add mode to clamp property name text to right side and clip left so right side of text is always visible, which is more important.
		- Class pointer members maybe do default object.
		- Ability to make memory snapshot.

	Session:
		- Actor component tree.
		- Widget component tree.
		- Favourites.
		- Variable/function categories.
		- Add filter mode that hides everything.
*/

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
#endif

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

FPropertyWatcher::TempMemoryPool FPropertyWatcher::TMem;

void FPropertyWatcher::Update(FString WindowName, TArray<PropertyItemCategory>& CategoryItems,
                              TArray<MemberPath>& WatchedMembers, UWorld* World, bool* IsOpen, bool* WantsToSave,
                              bool* WantsToLoad,
                              bool Init)
{
	SCOPE_EVENT("PropertyWatcher::Update");

	TMem.Init(TMemoryStartSize);
	defer { TMem.ClearAll(); };

	*WantsToLoad = false;
	*WantsToSave = false;

	// Performance test.
	static const int TimerCount = 30;
	static double Timers[TimerCount] = {};
	static int TimerIndex = 0;
	double StartTime = FPlatformTime::Seconds();

	ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
	bool WindowIsOpen = ImGui::Begin(ImGui_StoA(*("Property Watcher: " + WindowName)), IsOpen, ImGuiWindowFlags_MenuBar);
	defer { ImGui::End(); };
	if (!WindowIsOpen)
		return;

	static ImVec2 FramePadding = ImVec2(ImGui::GetStyle().CellPadding.x, 2);
	static bool ShowObjectNamesOnAllProperties = true;
	static bool ShowPerformanceInfo = false;

	// Menu.
	if (ImGui::BeginMenuBar())
	{
		defer { ImGui::EndMenuBar(); };
		if (ImGui::BeginMenu("Settings"))
		{
			defer { ImGui::EndMenu(); };

			ImGui::SetNextItemWidth(150);
			ImGui::DragFloat2("Item Padding", &FramePadding[0], 0.1f, 0.0, 20.0f, "%.0f");
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				FramePadding = ImVec2(ImGui::GetStyle().FramePadding.x, 2);

			ImGui::Checkbox("Show object names on all properties", &ShowObjectNamesOnAllProperties);
			ImGuiAddon::QuickTooltip("This puts the (<ObjectName>) at the end of properties that are also objects.");

			ImGui::Checkbox("Show debug/performance info", &ShowPerformanceInfo);
			ImGuiAddon::QuickTooltip("Displays item count and average elapsed time in ms.");
		}
		if (ImGui::BeginMenu("Help"))
		{
			defer { ImGui::EndMenu(); };
			ImGui::Text(HelpText);
		}
	}

	ColumnInfos ColInfos = {};
	{
		int FlagNoSort = ImGuiTableColumnFlags_NoSort;
		int FlagDefault = ImGuiTableColumnFlags_WidthStretch;
		ColInfos.Infos.Add({ ColumnID_Name,     "name",     "Property Name",  FlagDefault | ImGuiTableColumnFlags_NoHide });
		ColInfos.Infos.Add({ ColumnID_Value,    "value",    "Property Value", FlagDefault | FlagNoSort });
		ColInfos.Infos.Add({ ColumnID_Metadata, "metadata", "Metadata",       FlagNoSort | ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("(?)").x });
		ColInfos.Infos.Add({ ColumnID_Type,     "type",     "Property Type",  FlagDefault | FlagNoSort | ImGuiTableColumnFlags_DefaultHide });
		ColInfos.Infos.Add({ ColumnID_Cpptype,  "cpptype",  "CPP Type",       FlagDefault });
		ColInfos.Infos.Add({ ColumnID_Class,    "class",    "Owner Class",    FlagDefault | FlagNoSort | ImGuiTableColumnFlags_DefaultHide });
		ColInfos.Infos.Add({ ColumnID_Category, "category", "Category",       FlagDefault | FlagNoSort | ImGuiTableColumnFlags_DefaultHide });
		ColInfos.Infos.Add({ ColumnID_Address,  "address",  "Adress",         FlagDefault | ImGuiTableColumnFlags_DefaultHide });
		ColInfos.Infos.Add({ ColumnID_Size,     "size",     "Size",           FlagDefault | ImGuiTableColumnFlags_DefaultHide });
		ColInfos.Infos.Add({ ColumnID_Remove,   "",         "Remove",         ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight() });
	}

	// Top region.
	static bool ListFunctionsOnObjectItems = false;
	static bool EnableClassCategoriesOnObjectItems = true;
	static bool SearchFilterActive = false;
	static char SearchString[100];
	SimpleSearchParser SearchParser;
	{
		// Search.
		bool SelectAll = false;
		if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_F))
		{
			SelectAll = true;
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::SetNextItemWidth(
			ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)(?)(?)Filter(?)Classes(?)Functions").x);
		int Flags = ImGuiInputTextFlags_AutoSelectAll;
		ImGui::InputTextWithHint("##SearchEdit", "Search Properties (Ctrl+F)", SearchString, IM_ARRAYSIZE(SearchString),
		                         Flags);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			defer { ImGui::EndTooltip(); };

			FString Text = SearchBoxHelpText;
			Text.Append("\nCurrent keywords for columns are: \n\t");
			Text.Append(FString::Join(ColInfos.GetSearchNameArray(), *FString(", ")));
			ImGui::Text(ImGui_StoA(*Text));
		}
		ImGui::SameLine();
		//ImGuiWindow* window = ImGui::GetCurrentWindow();
		auto Window = ImGui::GetCurrentContext()->CurrentWindow;
		ImGui::Checkbox("Filter", &SearchFilterActive);
		ImGuiAddon::QuickTooltip("Enable filtering of rows that didn't pass the search in the search box.");
		ImGui::SameLine();
		ImGui::Checkbox("Classes", &EnableClassCategoriesOnObjectItems);
		ImGuiAddon::QuickTooltip("Enable sorting of actor member variables by classes with subsections.");
		ImGui::SameLine();
		ImGui::Checkbox("Functions", &ListFunctionsOnObjectItems);
		ImGuiAddon::QuickTooltip("Show functions in actor items.");
		ImGui::Spacing();

		SearchParser.ParseExpression(SearchString, ColInfos.GetSearchNameArray());
	}

	// Tabs.
	int ItemCount = 0;
	float ScrollRegionHeight = 0;
	if (ImGui::BeginTabBar("PropertyWatcherTabBar"))
	{
		defer { ImGui::EndTabBar(); };

		bool AddressHoveredThisFrame = false;
		static void* HoveredAddress = 0;
		static bool DrawHoveredAddresses = false;
		defer { DrawHoveredAddresses = AddressHoveredThisFrame; };

		static TArray<FString> Tabs = {"Objects", "Actors", "Watch"};
		for (auto CurrentTab : Tabs)
		{
			if (ImGui::BeginTabItem(ImGui_StoA(*CurrentTab)))
			{
				defer { ImGui::EndTabItem(); };

				if (CurrentTab == "Watch")
					WatchTab(true, WatchedMembers, WantsToSave, WantsToLoad, CategoryItems);
				else if (CurrentTab == "Actors")
					ActorsTab(true, World);

				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, FramePadding);
				defer { ImGui::PopStyleVar(); };

				ImVec2 TableSize = ImVec2(0, ImGui::GetContentRegionAvail().y);
				if (ShowPerformanceInfo)
					TableSize.y -= ImGui::GetTextLineHeightWithSpacing();

				ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_Borders |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
					ImGuiTableFlags_ScrollY | ImGuiTableFlags_SortTristate;
				if (CurrentTab == "Actors")
					TableFlags |= ImGuiTableFlags_Sortable;

				// Table.
				if (ImGui::BeginTable("table", ColInfos.Infos.Num(), TableFlags, TableSize))
				{
					ImGui::TableSetupScrollFreeze(0, 1);
					for (int i = 0; i < ColInfos.Infos.Num(); i++)
					{
						auto It = ColInfos.Infos[i];
						int Flags = It.Flags;
						if (It.DisplayName == "Remove" && CurrentTab != "Watch")
							Flags |= ImGuiTableColumnFlags_Disabled;
						ImGui::TableSetupColumn(ImGui_StoA(*It.DisplayName), Flags, It.InitWidth, i);
					}
					ImGui::TableHeadersRow();

					TArray<FString> CurrentPath;
					TreeState State = {};
					State.SearchFilterActive = SearchFilterActive;
					State.DrawHoveredAddress = DrawHoveredAddresses;
					State.HoveredAddress = HoveredAddress;
					State.CurrentWatchItemIndex = -1;
					State.EnableClassCategoriesOnObjectItems = EnableClassCategoriesOnObjectItems;
					State.ListFunctionsOnObjectItems = ListFunctionsOnObjectItems;
					State.ShowObjectNamesOnAllProperties = ShowObjectNamesOnAllProperties;
					State.SearchParser = SearchParser;
					State.ScrollRegionRange = FFloatInterval(ImGui::GetScrollY(), ImGui::GetScrollY() + TableSize.y);

					defer
					{
						if (State.AddressWasHovered)
						{
							State.AddressWasHovered = false;
							AddressHoveredThisFrame = true;
							HoveredAddress = State.HoveredAddress;
						};
					};

					if (CurrentTab == "Objects")
						ObjectsTab(false, CategoryItems, &State);
					else if (CurrentTab == "Actors")
						ActorsTab(false, World, &State, &ColInfos, Init);
					else if (CurrentTab == "Watch")
						WatchTab(false, WatchedMembers, WantsToSave, WantsToLoad, CategoryItems, &State);

					// Allow overscroll.
					{
						// Calculations are off, but it's fine for now.
						float Offset = ImGui::TableGetHeaderRowHeight() + ImGui::GetTextLineHeight() * 2 +
							ImGui::GetStyle().FramePadding.y * 2;
						ImGui::TableNextRow(0, TableSize.y - Offset);
						ImGui::TableNextColumn();

						auto BGColor = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
						ImGui::TableSetBgColor(ImGuiTableBgTarget_::ImGuiTableBgTarget_RowBg0,
						                       ImGui::GetColorU32(BGColor), -1);
					}

					ItemCount = State.ItemDrawCount;
					ScrollRegionHeight = ImGui::GetScrollMaxY();
					ImGui::EndTable();
				}
			}
		}
	}

	if (ShowPerformanceInfo)
	{
		ImGui::Text("Item: %d", ItemCount);
		ImGui::SameLine();
		ImGui::Text("-");
		ImGui::SameLine();
		{
			Timers[TimerIndex] = FPlatformTime::Seconds() - StartTime;
			TimerIndex = (TimerIndex + 1) % TimerCount;
			double AverageTime = 0;
			for (int i = 0; i < TimerCount; i++)
				AverageTime += Timers[i];
			AverageTime /= TimerCount;
			ImGui::Text("%.3f ms", AverageTime * 1000.0f);
		}
		ImGui::SameLine();
		ImGui::Text("-");
		ImGui::SameLine();
		ImGui::Text("ScrollHeight: %.0f", ScrollRegionHeight);

		ImGui::SameLine();
		ImGui::Text("-");
		ImGui::SameLine();

		ImGui::Text("HoveredId: %u", ImGui::GetHoveredID());
	}

	ImRect TargetRect(ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax());
	TargetRect.Translate(ImGui::GetWindowPos());
	TargetRect.Translate(ImVec2(0, ImGui::GetScrollY()));

	int HoveredID = ImGui::GetHoveredID() == 0 ? 1 : ImGui::GetHoveredID();
	if (ImGui::BeginDragDropTargetCustom(TargetRect, HoveredID))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PropertyWatcherMember"))
		{
			char* Payload = (char*)payload->Data;
			MemberPath Member;

			Member.PathString = FString::ConstructFromPtrSize(Payload, payload->DataSize);
			WatchedMembers.Push(Member);
		}
	}
}

void FPropertyWatcher::ObjectsTab(bool DrawControls, TArray<PropertyItemCategory>& CategoryItems, TreeState* State)
{
	if (DrawControls)
	{
		return;
	}
	
	TInlineComponentArray<FAView> CurrentPath;
	for (auto& Category : CategoryItems) {
		bool MakeCategorySection = !Category.Name.IsEmpty();

		TreeNodeState NodeState = {};
		if (MakeCategorySection)
			BeginSection(TMem.SToA(Category.Name), NodeState, *State, -1, ImGuiTreeNodeFlags_DefaultOpen);

		if (NodeState.IsOpen || !MakeCategorySection)
			for (auto& Item : Category.Items)
				DrawItemRow(*State, Item, CurrentPath);

		if (MakeCategorySection)
			EndSection(NodeState, *State);
	}
}

void FPropertyWatcher::ActorsTab(bool DrawControls, UWorld* World, TreeState* State, ColumnInfos* ColInfos,
                                 bool Init)
{
	static TArray<PropertyItem> ActorItems;
	static bool UpdateActorsEveryFrame = false;
	static bool SearchAroundPlayer = false;
	static float ActorsSearchRadius = 5;
	static bool DrawOverlapSphere = false;

	if (Init)
		ActorItems.Empty();

	if (DrawControls)
	{
		static TArray<bool> CollisionChannelsActive;
		static bool InitActors = true;
		if (InitActors)
		{
			InitActors = false;
			int NumCollisionChannels = StaticEnum<ECollisionChannel>()->NumEnums();
			for (int i = 0; i < NumCollisionChannels; i++)
				CollisionChannelsActive.Push(false);
		}

		if (!World)
			return;

		if (World->GetCurrentLevel())
		{
			ImGui::Text("Current World: %s, ", ImGui_StoA(*World->GetName()));
			ImGui::SameLine();
			ImGui::Text("Current Level: %s", ImGui_StoA(*World->GetCurrentLevel()->GetName()));
			ImGui::Spacing();
		}

		bool UpdateActors = UpdateActorsEveryFrame;
		if (UpdateActorsEveryFrame) ImGui::BeginDisabled();
		if (ImGui::Button("Update Actors"))
			UpdateActors = true;
		ImGui::SameLine();
		if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0)))
			ActorItems.Empty();
		if (UpdateActorsEveryFrame) ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Checkbox("Update actors every frame", &UpdateActorsEveryFrame);
		ImGui::SameLine();
		ImGui::Checkbox("Search around player", &SearchAroundPlayer);
		ImGui::Spacing();

		bool DoRaytrace = false;
		{
			if (!SearchAroundPlayer) ImGui::BeginDisabled();

			if (ImGui::Button("Set Channels"))
				ImGui::OpenPopup("SetChannelsPopup");

			if (ImGui::BeginPopup("SetChannelsPopup"))
			{
				for (int i = 0; i < CollisionChannelsActive.Num(); i++)
				{
					FString Name = StaticEnum<ECollisionChannel>()->GetNameStringByIndex(i);
					ImGui::Selectable(ImGui_StoA(*Name), &CollisionChannelsActive[i],
					                  ImGuiSelectableFlags_DontClosePopups);
				}

				ImGui::Separator();
				if (ImGui::Button("Clear All"))
					for (auto& It : CollisionChannelsActive)
						It = false;

				ImGui::EndPopup();
			}

			static bool RaytraceReady = false;
			ImGui::SameLine();
			if (ImGui::Button("Do Mouse Trace"))
			{
				ImGui::OpenPopup("PopupMouseTrace");
				RaytraceReady = true;
			}

			if (ImGui::BeginPopup("PopupMouseTrace"))
			{
				ImGui::Text("Click on screen to trace object.");
				ImGui::EndPopup();
			}
			else
			{
				if (RaytraceReady)
				{
					RaytraceReady = false;
					DoRaytrace = true;
				}
			}

			ImGui::SetNextItemWidth(150);
			ImGui::InputFloat("Search radius in meters", &ActorsSearchRadius, 1.0, 1.0, "%.1f");
			ImGui::SameLine();
			ImGui::Checkbox("Draw Search sphere", &DrawOverlapSphere);

			if (!SearchAroundPlayer) ImGui::EndDisabled();
		}

		{
			APlayerController* PlayerController = World->GetFirstPlayerController();
			TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
			for (int i = 0; i < CollisionChannelsActive.Num(); i++)
			{
				bool Active = CollisionChannelsActive[i];
				if (Active)
					traceObjectTypes.Add(UEngineTypes::ConvertToObjectType((ECollisionChannel)i));
			}

			FVector SpherePos = PlayerController->GetPawn()->GetActorTransform().GetLocation();
			if (UpdateActors)
			{
				ActorItems.Empty();

				if (SearchAroundPlayer)
				{
					TArray<AActor*> ResultActors;
					{
						//UClass* seekClass = AStaticMeshActor::StaticClass();					
						UClass* seekClass = 0;
						TArray<AActor*> ignoreActors = {};

						UKismetSystemLibrary::SphereOverlapActors(World, SpherePos, ActorsSearchRadius * 100,
						                                          traceObjectTypes, seekClass, ignoreActors,
						                                          ResultActors);
					}

					for (auto It : ResultActors)
					{
						if (!It) continue;
						ActorItems.Push(MakeObjectItem(It));
					}
				}
				else
				{
					ULevel* CurrentLevel = World->GetCurrentLevel();
					if (CurrentLevel)
					{
						TArray<AActor*> Actors;
						for (TActorIterator<AActor> It(World, AActor::StaticClass()); It; ++It)
						{
							AActor* Actor = *It;

							// todo cache

							bool AddActor = true;
							// if (Filter != nullptr && Filter->IsActive())
							// {
							//     const auto ActorName = StringCast<ANSICHAR>(*GetActorName(*Actor));
							//     if (Filter != nullptr && Filter->PassFilter(ActorName.Get()) == false)
							//     {
							//         AddActor = false;
							//     }
							// }

							if (AddActor)
							{
								Actors.Add(Actor);
							}
						}


						for (auto& Actor : Actors)
						{
							if (!Actor) continue;
							ActorItems.Push(MakeObjectItem(Actor));
						}
					}
				}
			}

			if (DrawOverlapSphere)
				DrawDebugSphere(World, SpherePos, ActorsSearchRadius * 100, 20, FColor::Purple, false, 0.0f);

			if (DoRaytrace)
			{
				FHitResult HitResult;
				bool Result = PlayerController->GetHitResultUnderCursorForObjects(traceObjectTypes, true, HitResult);
				if (Result)
				{
					AActor* HitActor = HitResult.GetActor();
					if (HitActor)
						ActorItems.Push(MakeObjectItem(HitActor));
				}
			}
		}

		return;
	}

	// Sorting.
	{
		static ImGuiTableSortSpecs* s_current_sort_specs = NULL;
		auto SortFun = [](const void* lhs, const void* rhs) -> int
		{
			PropertyItem* a = (PropertyItem*)lhs;
			PropertyItem* b = (PropertyItem*)rhs;
			for (int n = 0; n < s_current_sort_specs->SpecsCount; n++)
			{
				const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[n];
				int delta = 0;
				switch (sort_spec->ColumnUserID)
				{
				//case ColumnID_Name:    delta = (strcmp(ImGui_StoA(*((UObject*)a->Ptr)->GetName()), ImGui_StoA(*((UObject*)b->Ptr)->GetName())));
				//case ColumnID_Cpptype: delta = strcmp(a->GetCPPType().GetData(), b->GetCPPType().GetData());
				case ColumnID_Name:
					{
						delta = FCString::Strcmp(*((UObject*)a->Ptr)->GetName(), *((UObject*)b->Ptr)->GetName());
					}
					break;
				case ColumnID_Cpptype:
					{
						delta = a->GetCPPType().Compare(b->GetCPPType());
					}
					break;
				case ColumnID_Address:
					{
						delta = (int64)a->Ptr - (int64)b->Ptr;
					}
					break;
				case ColumnID_Size:
					{
						delta = a->GetSize() - b->GetSize();
					}
					break;
				default: IM_ASSERT(0);
				}
				if (delta > 0)
					return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
				if (delta < 0)
					return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
			}

			return ((int64)a->Ptr - (int64)b->Ptr);
		};

		if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
		{
			if (sorts_specs->SpecsDirty)
			{
				s_current_sort_specs = sorts_specs; // Store in variable accessible by the sort function.
				if (ActorItems.Num() > 1)
					qsort(ActorItems.GetData(), (size_t)ActorItems.Num(), sizeof(ActorItems[0]), SortFun);
				s_current_sort_specs = NULL;
				sorts_specs->SpecsDirty = false;
			}
		}
	}

	TInlineComponentArray<FAView> CurrentPath;
	for (auto& Item : ActorItems)
		DrawItemRow(*State, Item, CurrentPath);
}

void FPropertyWatcher::WatchTab(bool DrawControls, TArray<MemberPath>& WatchedMembers, bool* WantsToSave,
                                bool* WantsToLoad, TArray<PropertyItemCategory>& CategoryItems, TreeState* State)
{
	if (DrawControls)
	{
		if (ImGui::Button("Clear All"))
			WatchedMembers.Empty();
		ImGui::SameLine();
		if (ImGui::Button("Save"))
			*WantsToSave = true;
		ImGui::SameLine();
		if (ImGui::Button("Load"))
			*WantsToLoad = true;

		return;
	}

	TArray<PropertyItem> Items;
	for (auto& It : CategoryItems)
		Items.Append(It.Items);

	TInlineComponentArray<FAView> CurrentPath;
	int MemberIndexToDelete = -1;
	bool MoveHappened = false;
	int MoveIndexFrom = -1;
	int MoveIndexTo = -1;
	FString NewPathName;

	int i = 0;
	for (auto& Member : WatchedMembers)
	{
		State->CurrentWatchItemIndex = i++;
		State->WatchItemGotDeleted = false;
		State->RenameHappened = false;
		State->PathStringPtr = &Member.PathString;

		bool Found = Member.UpdateItemFromPath(Items);
		if (!Found)
		{
			Member.CachedItem.Ptr = 0;
			Member.CachedItem.Prop = 0;
		}

		DrawItemRow(*State, Member.CachedItem, CurrentPath);

		if (State->WatchItemGotDeleted)
			MemberIndexToDelete = State->CurrentWatchItemIndex;

		if (State->MoveHappened)
		{
			MoveHappened = true;
			MoveIndexFrom = State->MoveFrom;
			MoveIndexTo = State->MoveTo;
		}
	}

	if (MemberIndexToDelete != -1)
		WatchedMembers.RemoveAt(MemberIndexToDelete);

	if (MoveHappened)
		WatchedMembers.Swap(MoveIndexFrom, MoveIndexTo);
}



void FPropertyWatcher::DrawItemRow(TreeState& State, PropertyItem& Item,
                                   TInlineComponentArray<FAView>& CurrentMemberPath, int StackIndex)
{
	SCOPE_EVENT("PropertyWatcher::DrawItemRow");

	if (State.ItemDrawCount > 100000) // @Todo: Random safety measure against infinite recursion, could be better?
		return;

	bool ItemCanBeOpened = Item.CanBeOpened();
	bool ItemIsVisible = State.IsCurrentItemVisible();
	bool SearchIsActive = (bool)State.SearchParser.Commands.Num();

	CachedColumnText ColumnTexts;
	FAView ItemDisplayName;
	bool ItemIsSearched = false;

	if (!ItemIsVisible && !State.SearchFilterActive)
	{
		ItemDisplayName = "";
	}
	else
	{
		ItemDisplayName = GetColumnCellText(Item, ColumnID_Name, &State, &CurrentMemberPath, &StackIndex);

		if (SearchIsActive)
		{
			ColumnTexts.Add(ColumnID_Name, ItemDisplayName); // Default.

			// Cache the cell texts that we need for the text search.
			for (auto& Command : State.SearchParser.Commands)
				if (Command.Type == SimpleSearchParser::Command_Test && !ColumnTexts.Get(Command.Tst.ColumnID))
				{
					FAView view = GetColumnCellText(Item, Command.Tst.ColumnID, &State, &CurrentMemberPath,
					                                &StackIndex);
					if (view.IsEmpty())
					{
						int stop = 234;
					}
					ColumnTexts.Add(Command.Tst.ColumnID, view);
				}

			ItemIsSearched = State.SearchParser.ApplyTests(ColumnTexts);
		}
	}

	auto FindOrGetColumnText = [&Item, &ColumnTexts](int ColumnID) -> FAView
	{
		if (FAView* Result = ColumnTexts.Get(ColumnID))
			return *Result;
		else
			return GetColumnCellText(Item, ColumnID);
	};

	// Item is skipped.
	if (State.SearchFilterActive && !ItemIsSearched && !ItemCanBeOpened)
		return;

	// Misc setup.

	State.ItemDrawCount++;
	FAView ItemAuthoredName = ItemCanBeOpened ? Item.GetAuthoredName() : "";
	if (ItemCanBeOpened)
		CurrentMemberPath.Push(ItemAuthoredName);

	FString ItemName = FString(ItemCanBeOpened ? Item.GetAuthoredName() : "") + FString(TMem.Printf(" Property%p", Item.Prop));
	ItemAuthoredName = TCHAR_TO_ANSI(*ItemName);

	TreeNodeState NodeState;
	{
		NodeState = {};
		NodeState.HasBranches = ItemCanBeOpened;
		NodeState.ItemInfo.Set(Item);

		if (ItemIsVisible && ItemIsSearched)
		{
			NodeState.PushTextColor = true;
			NodeState.TextColor = ImVec4(1, 0.5f, 0, 1);
		}
	}

	bool IsTopWatchItem = State.CurrentWatchItemIndex != -1 && StackIndex == 0;
	if (IsTopWatchItem)
		ImGui::PushID(State.CurrentWatchItemIndex);

	// @Column(name): Property name
	{
		if (ItemIsVisible && !Item.IsValid())
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

		BeginTreeNode(*ItemAuthoredName, *ItemDisplayName, NodeState, State, StackIndex, 0);

		bool NodeIsMarkedAsInlined = false;

		// Right click popup for inlining.
		if (NodeState.HasBranches)
		{
			// Do tree push to get right bool from storage when node is open or closed.
			if (!NodeState.IsOpen)
				ImGui::TreePush(*ItemAuthoredName);

			ImGuiID StorageIDIsInlined = ImGui::GetID("IsInlined");
			ImGuiID StorageIDInlinedStackDepth = ImGui::GetID("InlinedStackDepth");

			auto Storage = ImGui::GetStateStorage();
			NodeIsMarkedAsInlined = Storage->GetBool(StorageIDIsInlined);
			int InlinedStackDepth = Storage->GetInt(StorageIDInlinedStackDepth, 1);

			if (NodeIsMarkedAsInlined && !State.ForceInlineChildItems && InlinedStackDepth)
				TreeNodeSetInline(NodeState, State, CurrentMemberPath.Num(), StackIndex, InlinedStackDepth);

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				ImGui::OpenPopup("ItemPopup");

			if (ImGui::BeginPopup("ItemPopup"))
			{
				if (ImGui::Checkbox("Inlined", &NodeIsMarkedAsInlined))
					Storage->SetBool(StorageIDIsInlined, NodeIsMarkedAsInlined);

				ImGui::SameLine();
				ImGui::BeginDisabled(!NodeIsMarkedAsInlined);
				if (ImGui::SliderInt("Stack Depth", &InlinedStackDepth, 0, 9))
					Storage->SetInt(StorageIDInlinedStackDepth, InlinedStackDepth);
				ImGui::EndDisabled();

				ImGui::EndPopup();
			}

			if (!NodeState.IsOpen)
				ImGui::TreePop();
		}

		if (ItemIsVisible)
		{
			// Drag to watch window.
			if (StackIndex > 0 && Item.Type != PointerType::Function)
			{
				if (ImGui::BeginDragDropSource())
				{
					TMemBuilder(Builder);
					for (int i = 0; i < CurrentMemberPath.Num(); i++)
					{
						if (i > 0) Builder.AppendChar('.');
						Builder.Append(CurrentMemberPath[i]);
					}
					if (!NodeState.HasBranches)
						Builder << '.' << ItemDisplayName;

					ImGui::SetDragDropPayload("PropertyWatcherMember", *Builder, Builder.Len());
					ImGui::Text("Add to watch list:");
					ImGui::Text(*Builder);
					ImGui::EndDragDropSource();
				}
			}

			// Drag to move watch item. Only available when top level watch list item.
			if (IsTopWatchItem)
			{
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("PropertyWatcherMoveIndex", &State.CurrentWatchItemIndex, sizeof(int));
					ImGui::Text("Move Item: %d", State.CurrentWatchItemIndex);
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PropertyWatcherMoveIndex"))
					{
						if (Payload->Data)
						{
							State.MoveHappened = true;
							State.MoveFrom = *(int*)Payload->Data;
							State.MoveTo = State.CurrentWatchItemIndex;
						}
					}
					ImGui::EndDragDropTarget();
				}

				// Handle watch list item path editing.
				if (State.PathStringPtr)
				{
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight());

					FString StringID = FString::Printf(TEXT("##InputPathText %d"), State.CurrentWatchItemIndex);

					static TArray<char> StringBuffer;
					StringBuffer.Empty();
					StringBuffer.Append(ImGui_StoA(**State.PathStringPtr), State.PathStringPtr->Len() + 1);
					if (ImGuiAddon::InputText(ImGui_StoA(*StringID), StringBuffer,
					                          ImGuiInputTextFlags_EnterReturnsTrue))
						(*State.PathStringPtr) = FString(StringBuffer);

					ImGui::PopStyleColor(1);
				}
			}

			if (NodeIsMarkedAsInlined)
			{
				ImGui::SameLine();
				ImGui::Text("*");
			}

			// This puts the (<ObjectName>) at the end of properties that are also objects.
			if (State.ShowObjectNamesOnAllProperties)
			{
				if (Item.Type == PointerType::Property && CastField<FObjectProperty>(Item.Prop) && Item.Ptr)
				{
					ImGui::SameLine();
					ImGui::BeginDisabled();
					FName Name = ((UObject*)Item.Ptr)->GetFName();
					ImGui::Text(*TMem.Printf("(%s)", *TMem.NToA(Name)));
					ImGui::EndDisabled();
				}
			}
		}

		if (ItemIsVisible && !Item.IsValid())
			ImGui::PopStyleColor(1);
	}

	// Draw other columns if visible. 
	if (!ItemIsVisible || NodeState.ItemIsInlined)
	{
		ImGui::TableSetColumnIndex(ImGui::TableGetColumnCount() - 1);
	}
	else
	{
		// @Column(value): Property Value
		if (ImGui::TableNextColumn())
		{
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (Item.IsValid())
				DrawPropertyValue(Item);
		}

		// @Column(metadata): Metadata							
		if (ImGui::TableNextColumn() && Item.Prop)
		{
			if (ItemHasMetaData(Item))
			{
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

					if (MetaData_Available)
						ImGui::TextUnformatted(*FindOrGetColumnText(ColumnID_Metadata));
					else
						ImGui::TextUnformatted("Data not available in shipping builds.");

					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
				ImGui::IsItemHovered();
			}
		}

		// @Column(type): Property Type
		if (ImGui::TableNextColumn())
		{
			if (Item.IsValid())
			{
				{
					ImVec4 cText = ImVec4(0, 0, 0, 0);
					GetItemColor(Item, cText);
					ImGui::PushStyleColor(ImGuiCol_Text, cText);
					ImGui::Bullet();
					ImGui::PopStyleColor();
				}
				ImGui::SameLine();
				ImGui::Text(*FindOrGetColumnText(ColumnID_Type));
			}
		}

		// @Column(cpptype): CPP Type
		if (ImGui::TableNextColumn())
			ImGui::Text(*FindOrGetColumnText(ColumnID_Cpptype));

		// @Column(class): Owner Class
		if (ImGui::TableNextColumn())
			ImGui::Text(*FindOrGetColumnText(ColumnID_Class));

		// @Column(category): Metadata Category
		if (ImGui::TableNextColumn())
			ImGui::Text(*FindOrGetColumnText(ColumnID_Category));

		// @Column(address): Adress
		if (ImGui::TableNextColumn())
		{
			if (State.DrawHoveredAddress && Item.Ptr == State.HoveredAddress)
				ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), *FindOrGetColumnText(ColumnID_Address));
			else
				ImGui::Text(*FindOrGetColumnText(ColumnID_Address));

			if (ImGui::IsItemHovered())
			{
				State.AddressWasHovered = true;
				State.HoveredAddress = Item.Ptr;
			}
		}

		// @Column(size): Size
		if (ImGui::TableNextColumn())
			ImGui::Text(*FindOrGetColumnText(ColumnID_Size));

		// Close Button
		if (ImGui::TableNextColumn())
			if (IsTopWatchItem)
				if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0)))
					State.WatchItemGotDeleted = true;
	}

	// Draw leaf properties.
	if (NodeState.IsOpen)
	{
		bool PushAddressesStack = State.ForceToggleNodeOpenClose || State.ForceInlineChildItems;
		if (PushAddressesStack)
			State.VisitedPropertiesStack.Push(NodeState.ItemInfo);
		TMem.PushMarker();

		DrawItemChildren(State, Item, CurrentMemberPath, StackIndex);

		TMem.PopMarker();
		if (PushAddressesStack)
			State.VisitedPropertiesStack.Pop(false);
	}

	if (ItemCanBeOpened)
		CurrentMemberPath.Pop(false);

	EndTreeNode(NodeState, State);

	if (IsTopWatchItem)
		ImGui::PopID();
}

void FPropertyWatcher::DrawItemChildren(TreeState& State, PropertyItem& Item,
                                        TInlineComponentArray<FAView>& CurrentMemberPath, int StackIndex)
{
	check(Item.Ptr); // Do we need this check here? Can't remember.

	if (Item.Prop &&
		(Item.Prop->IsA(FWeakObjectProperty::StaticClass()) ||
			Item.Prop->IsA(FLazyObjectProperty::StaticClass()) ||
			Item.Prop->IsA(FSoftObjectProperty::StaticClass()))) {
		UObject* Obj = 0;
		bool IsValid = GetObjFromObjPointerProp(Item, Obj);
		if (IsValid) {
			return DrawItemChildren(State, MakeObjectItem(Obj), CurrentMemberPath, StackIndex + 1);
		}
	}

	bool ItemIsObjectProp = Item.Prop && Item.Prop->IsA(FObjectProperty::StaticClass());
	bool ItemIsObject = ItemIsObjectProp || Item.Type == PointerType::Object;

	// Members.
	{
		TArray<PropertyItem> Members;
		Item.GetMembers(&Members);

		SectionHelper SectionHelper;
		if (State.EnableClassCategoriesOnObjectItems && ItemIsObject) {
			for (auto& Member : Members)
				SectionHelper.Add(((FField*)(Member).Prop)->Owner.GetFName());

			SectionHelper.Init();
		}

		if (!SectionHelper.Enabled) {
			for (auto It : Members)
				DrawItemRow(State, It, CurrentMemberPath, StackIndex + 1);

		} else {
			for (int SectionIndex = 0; SectionIndex < SectionHelper.GetSectionCount(); SectionIndex++) {
				int MemberStartIndex, MemberEndIndex;
				auto CurrentSectionName = SectionHelper.GetSectionInfo(SectionIndex, MemberStartIndex, MemberEndIndex);

				TreeNodeState NodeState = {};
				NodeState.OverrideNoTreePush = true;
				BeginSection(CurrentSectionName, NodeState, State, StackIndex, SectionIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);

				if (NodeState.IsOpen)
					for (int MemberIndex = MemberStartIndex; MemberIndex < MemberEndIndex; MemberIndex++)
						DrawItemRow(State, Members[MemberIndex], CurrentMemberPath, StackIndex + 1);

				EndSection(NodeState, State);
			}
		}
	}

	// Functions.
	if (State.ListFunctionsOnObjectItems && Item.Ptr && ItemIsObject) {
		TArray<UFunction*> Functions = GetObjectFunctionList((UObject*)Item.Ptr);

		if (Functions.Num()) {
			TreeNodeState FunctionSection = {};
			BeginSection("Functions", FunctionSection, State, StackIndex, 0);

			if (FunctionSection.IsOpen) {
				SectionHelper SectionHelper;
				if (State.EnableClassCategoriesOnObjectItems) {
					for (auto& Function : Functions)
						SectionHelper.Add(Function->GetOuterUClass()->GetFName());

					SectionHelper.Init();
				}

				if (!SectionHelper.Enabled) {
					for (auto It : Functions)
						DrawItemRow(State, MakeFunctionItem(Item.Ptr, It), CurrentMemberPath, StackIndex + 1);

				} else {
					for (int SectionIndex = 0; SectionIndex < SectionHelper.GetSectionCount(); SectionIndex++) {
						int MemberStartIndex, MemberEndIndex;
						auto CurrentSectionName = SectionHelper.GetSectionInfo(SectionIndex, MemberStartIndex, MemberEndIndex);

						TreeNodeState NodeState = {};
						NodeState.OverrideNoTreePush = true;
						BeginSection(CurrentSectionName, NodeState, State, StackIndex, SectionIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);

						if (NodeState.IsOpen)
							for (int MemberIndex = MemberStartIndex; MemberIndex < MemberEndIndex; MemberIndex++)
								DrawItemRow(State, MakeFunctionItem(Item.Ptr, Functions[MemberIndex]), CurrentMemberPath, StackIndex + 1);

						EndSection(NodeState, State);
					}
				}
			}

			EndSection(FunctionSection, State);
		}
	}
}

bool FPropertyWatcher::ItemHasMetaData(PropertyItem& Item)
{
	if (!Item.Prop)
		return false;

	bool Result = false;
#if MetaData_Available
	if (const TMap<FName, FString>* MetaData = Item.Prop->GetMetaDataMap())
		Result = (bool)MetaData->Num();
#endif

	return Result;
}

// -------------------------------------------------------------------------------------------

void FPropertyWatcher::SetTableRowBackgroundByStackIndex(int StackIndex)
{
	if (StackIndex == 0)
		return;

	// Cache colors.
	static ImU32 Colors[4] = {};
	static bool Init = true;
	if (Init)
	{
		Init = false;
		ImVec4 c = {0, 0, 0, 0.05f};
		for (int i = 0; i < 4; i++)
		{
			float h = (i / 4.0f) + 0.065; // Start with orange, cycle 4 colors.
			ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, c.x, c.y, c.z);
			Colors[i] = ImGui::GetColorU32(c);
		}
	}

	ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, Colors[(StackIndex - 1) % 4]);
}

int GetDigitKeyDownAsInt()
{
	for (int i = 0; i < 10; i++)
	{
		ImGuiKey DigitKeyCode = (ImGuiKey)(ImGuiKey_0 + i);
		if (ImGui::IsKeyDown(DigitKeyCode))
			return i;
	}
	return 0;
}

bool FPropertyWatcher::BeginTreeNode(const char* NameID, const char* DisplayName, TreeNodeState& NodeState,
                                     TreeState& State, int StackIndex, int ExtraFlags)
{
	bool IsNameNodeVisible = State.IsCurrentItemVisible();
	bool ItemIsInlined = false;

	if (NodeState.HasBranches)
	{
		ItemIsInlined = State.ForceInlineChildItems && (StackIndex <= State.InlineStackIndexLimit);

		if (State.ForceInlineChildItems && State.ItemIsInfiniteLooping(NodeState.ItemInfo))
			ItemIsInlined = false;
	}

	if (ItemIsInlined)
	{
		NodeState.IsOpen = true;
		ImGui::TreePush(NameID);
		ImGui::Unindent();
		NodeState.ItemIsInlined = true;
	}
	else
	{
		// Start new row. Column index before this should be LastColumnIndex + 1.
		ImGui::TableNextColumn();

		if (IsNameNodeVisible)
		{
			ImGui::AlignTextToFramePadding();
			SetTableRowBackgroundByStackIndex(
				NodeState.VisualStackIndex != -1 ? NodeState.VisualStackIndex : StackIndex);
		}
		bool PushTextColor = IsNameNodeVisible && NodeState.PushTextColor;
		if (PushTextColor)
			ImGui::PushStyleColor(ImGuiCol_Text, NodeState.TextColor);

		bool NodeStateChanged = false;

		if (NodeState.HasBranches)
		{
			// If force open mode is active we change the state of the node if needed.
			if (State.ForceToggleNodeOpenClose)
			{
				auto StateStorage = ImGui::GetStateStorage();
				auto ID = ImGui::GetID(NameID);
				bool IsOpen = (bool)StateStorage->GetInt(ID);

				// Tree node state should change.
				if (State.ForceToggleNodeMode != IsOpen)
				{
					bool StateChangeAllowed = true;

					// Checks when trying to toggle open node.
					if (State.ForceToggleNodeMode)
					{
						// Address already visited.
						if (State.ItemIsInfiniteLooping(NodeState.ItemInfo))
							StateChangeAllowed = false;

						// Stack depth limit reached.
						if (StackIndex > State.ForceToggleNodeStackIndexLimit)
							StateChangeAllowed = false;
					}

					if (StateChangeAllowed)
					{
						ImGui::SetNextItemOpen(State.ForceToggleNodeMode);
						NodeStateChanged = true;
					}
				}
			}

			int Flags = ExtraFlags | ImGuiTreeNodeFlags_NavLeftJumpsBackHere;
			if (NodeState.OverrideNoTreePush)
				Flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;

			const char* DisplayText = IsNameNodeVisible ? DisplayName : "";
			ImGui::PushID(NameID);
			NodeState.IsOpen = ImGui::TreeNodeEx(NameID, Flags, DisplayText);
			ImGui::PopID();

			{
				NodeState.ActivatedForceToggleNodeOpenClose = false;

				// Start force toggle mode.
				if (ImGui::IsItemToggledOpen() && ImGui::IsKeyDown(ImGuiMod_Shift) && !State.ForceToggleNodeOpenClose)
				{
					NodeState.ActivatedForceToggleNodeOpenClose = true;
					int StackLimitOffset = GetDigitKeyDownAsInt();
					State.EnableForceToggleNode(NodeState.IsOpen,
					                            StackIndex + (StackLimitOffset == 0 ? 10 : StackLimitOffset));
				}

				// If we forced this node closed we have to draw it's children for one frame so they can be forced closed as well.
				// The goal is to close everything that's visually open.
				if (State.IsForceToggleNodeActive(StackIndex) && (NodeState.ActivatedForceToggleNodeOpenClose ||
					NodeStateChanged) && !NodeState.IsOpen)
				{
					if (!(Flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
						ImGui::TreePush(NameID);

					NodeState.IsOpen = true;
				}
			}
		}
		else
		{
			const char* DisplayText = IsNameNodeVisible ? DisplayName : "";
			ImGui::TreeNodeEx(DisplayText, ExtraFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
		}

		if (PushTextColor)
			ImGui::PopStyleColor(1);
	}

	return NodeState.IsOpen;
}

void FPropertyWatcher::TreeNodeSetInline(TreeNodeState& NodeState, TreeState& State, int CurrentMemberPathLength,
                                         int StackIndex, int InlineStackDepth)
{
	NodeState.InlineChildren = true;

	State.ForceInlineChildItems = true;
	// @Note: Should we enable inifinite inlining again in the future?
	//State.InlineStackIndexLimit = InlineStackDepth == 0 ? StackIndex + 100 : StackIndex + InlineStackDepth;
	State.InlineStackIndexLimit = StackIndex + InlineStackDepth;

	State.InlineMemberPathIndexOffset = CurrentMemberPathLength;
	State.VisitedPropertiesStack.Empty();
}

void FPropertyWatcher::EndTreeNode(TreeNodeState& NodeState, TreeState& State)
{
	if (NodeState.ItemIsInlined)
	{
		ImGui::TreePop();
		ImGui::Indent();
	}
	else if (NodeState.IsOpen && !NodeState.OverrideNoTreePush)
		ImGui::TreePop();

	if (NodeState.ActivatedForceToggleNodeOpenClose)
		State.DisableForceToggleNode();

	if (NodeState.InlineChildren)
		State.ForceInlineChildItems = false;
}

bool FPropertyWatcher::BeginSection(FAView Name, TreeNodeState& NodeState, TreeState& State, int StackIndex,
                                    int ExtraFlags)
{
	bool NodeOpenCloseLogicIsEnabled = true;

	bool ItemIsVisible = State.IsCurrentItemVisible();

	NodeState.HasBranches = true;

	if (ItemIsVisible)
	{
		NodeState.PushTextColor = true;
		NodeState.TextColor = ImVec4(1, 1, 1, 0.5f);
		NodeState.VisualStackIndex = StackIndex + 1;

		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0));
	}

	ExtraFlags |= ImGuiTreeNodeFlags_Framed;
	bool IsOpen = BeginTreeNode(*Name, ItemIsVisible ? *TMem.Printf("(%s)", *Name) : "", NodeState, State, StackIndex,
	                            ExtraFlags);

	// Nothing else is drawn in the row so we skip to the next one.
	ImGui::TableSetColumnIndex(ImGui::TableGetColumnCount() - 1);

	if (ItemIsVisible)
		ImGui::PopStyleColor(1);

	return IsOpen;
}

void FPropertyWatcher::EndSection(TreeNodeState& NodeState, TreeState& State)
{
	EndTreeNode(NodeState, State);
}

TArray<FName> FPropertyWatcher::GetClassFunctionList(UClass* Class)
{
	TArray<FName> FunctionNames;

#if WITH_EDITOR
	Class->GenerateFunctionList(FunctionNames);
#else
	{
		// Hack to get at function data in shipping builds.
		struct TempStruct {
			TMap<FName, UFunction*> FuncMap;
			TMap<FName, UFunction*> SuperFuncMap;
			FRWLock SuperFuncMapLock;
			TArray<FImplementedInterface> Interfaces;
		};

		int Offset = offsetof(UClass, Interfaces) - offsetof(TempStruct, Interfaces);
		auto FuncMap = (TMap<FName, UFunction*>*)(((char*)Class) + Offset);
		if (FuncMap)
			FuncMap->GenerateKeyArray(FunctionNames);
	}
#endif

	return FunctionNames;
}

TArray<UFunction*> FPropertyWatcher::GetObjectFunctionList(UObject* Obj)
{
	TArray<UFunction*> Functions;

	UClass* Class = Obj->GetClass();
	if (!Class)
		return Functions;

	UClass* TempClass = Class;
	do
	{
		TArray<FName> FunctionNames = GetClassFunctionList(TempClass);
		for (auto& Name : FunctionNames)
		{
			UFunction* Function = Class->FindFunctionByName(Name);
			if (!Function)
				continue;
			Functions.Push(Function);
		}
		TempClass = TempClass->GetSuperClass();
	}
	while (TempClass->GetSuperClass());

	return Functions;
}

FAView FPropertyWatcher::GetItemMetadataCategory(PropertyItem& Item)
{
	FAView Category = "";
#if WITH_EDITORONLY_DATA
	if (Item.Prop)
	{
		if (const TMap<FName, FString>* MetaData = Item.Prop->GetMetaDataMap())
		{
			if (const FString* Value = MetaData->Find("Category"))
				Category = TMem.SToA(*((FString*)Value));
		}
	}
#endif
	return Category;
}

bool FPropertyWatcher::GetItemColor(PropertyItem& Item, ImVec4& Color)
{
	FLinearColor lColor = {};
	lColor.R = -1;

	// Copied from GraphEditorSettings.cpp
	static FLinearColor DefaultPinTypeColor(0.750000f, 0.6f, 0.4f, 1.0f); // light brown
	static FLinearColor ExecutionPinTypeColor(1.0f, 1.0f, 1.0f, 1.0f); // white
	static FLinearColor BooleanPinTypeColor(0.300000f, 0.0f, 0.0f, 1.0f); // maroon
	static FLinearColor BytePinTypeColor(0.0f, 0.160000f, 0.131270f, 1.0f); // dark green
	static FLinearColor ClassPinTypeColor(0.1f, 0.0f, 0.5f, 1.0f); // deep purple (violet
	static FLinearColor IntPinTypeColor(0.013575f, 0.770000f, 0.429609f, 1.0f); // green-blue
	static FLinearColor Int64PinTypeColor(0.413575f, 0.770000f, 0.429609f, 1.0f);
	static FLinearColor FloatPinTypeColor(0.357667f, 1.0f, 0.060000f, 1.0f); // bright green
	static FLinearColor DoublePinTypeColor(0.039216f, 0.666667f, 0.0f, 1.0f); // darker green
	static FLinearColor RealPinTypeColor(0.039216f, 0.666667f, 0.0f, 1.0f); // darker green
	static FLinearColor NamePinTypeColor(0.607717f, 0.224984f, 1.0f, 1.0f); // lilac
	static FLinearColor DelegatePinTypeColor(1.0f, 0.04f, 0.04f, 1.0f); // bright red
	static FLinearColor ObjectPinTypeColor(0.0f, 0.4f, 0.910000f, 1.0f); // sharp blue
	static FLinearColor SoftObjectPinTypeColor(0.3f, 1.0f, 1.0f, 1.0f);
	static FLinearColor SoftClassPinTypeColor(1.0f, 0.3f, 1.0f, 1.0f);
	static FLinearColor InterfacePinTypeColor(0.8784f, 1.0f, 0.4f, 1.0f); // pale green
	static FLinearColor StringPinTypeColor(1.0f, 0.0f, 0.660537f, 1.0f); // bright pink
	static FLinearColor TextPinTypeColor(0.8f, 0.2f, 0.4f, 1.0f); // salmon (light pink
	static FLinearColor StructPinTypeColor(0.0f, 0.1f, 0.6f, 1.0f); // deep blue
	static FLinearColor WildcardPinTypeColor(0.220000f, 0.195800f, 0.195800f, 1.0f); // dark gray
	static FLinearColor VectorPinTypeColor(1.0f, 0.591255f, 0.016512f, 1.0f); // yellow
	static FLinearColor RotatorPinTypeColor(0.353393f, 0.454175f, 1.0f, 1.0f); // periwinkle
	static FLinearColor TransformPinTypeColor(1.0f, 0.172585f, 0.0f, 1.0f); // orange
	static FLinearColor IndexPinTypeColor(0.013575f, 0.770000f, 0.429609f, 1.0f); // green-blue

	if (!Item.Prop)
		return false;

	if (Item.Prop->IsA(FBoolProperty::StaticClass())) lColor = BooleanPinTypeColor;
	else if (Item.Prop->IsA(FByteProperty::StaticClass())) lColor = BytePinTypeColor;
	else if (Item.Prop->IsA(FClassProperty::StaticClass())) lColor = ClassPinTypeColor;
	else if (Item.Prop->IsA(FIntProperty::StaticClass())) lColor = IntPinTypeColor;
	else if (Item.Prop->IsA(FInt64Property::StaticClass())) lColor = Int64PinTypeColor;
	else if (Item.Prop->IsA(FFloatProperty::StaticClass())) lColor = FloatPinTypeColor;
	else if (Item.Prop->IsA(FDoubleProperty::StaticClass())) lColor = DoublePinTypeColor;
		//else if (Item.Prop->IsA(FRealproperty::StaticClass()))    lColor = ;
	else if (Item.Prop->IsA(FNameProperty::StaticClass())) lColor = NamePinTypeColor;
	else if (Item.Prop->IsA(FDelegateProperty::StaticClass())) lColor = DelegatePinTypeColor;
	else if (Item.Prop->IsA(FObjectProperty::StaticClass())) lColor = ObjectPinTypeColor;
	else if (Item.Prop->IsA(FSoftClassProperty::StaticClass())) lColor = SoftClassPinTypeColor;
	else if (Item.Prop->IsA(FInterfaceProperty::StaticClass())) lColor = InterfacePinTypeColor;
	else if (Item.Prop->IsA(FStrProperty::StaticClass())) lColor = StringPinTypeColor;
	else if (Item.Prop->IsA(FTextProperty::StaticClass())) lColor = TextPinTypeColor;

	else if (FStructProperty* StructProp = CastField<FStructProperty>(Item.Prop))
	{
		FString Extended;
		FString StructType = StructProp->GetCPPType(&Extended, 0);
		if (StructType == "FVector") lColor = VectorPinTypeColor;
		else if (StructType == "FRotator") lColor = RotatorPinTypeColor;
		else if (StructType == "FTransform") lColor = TransformPinTypeColor;
		else lColor = StructPinTypeColor;
	}

	bool ColorGotSet = lColor.R != -1;
	if (ColorGotSet)
	{
		FColor c = lColor.ToFColor(true);
		Color = {c.R / 255.0f, c.G / 255.0f, c.B / 255.0f, c.A / 255.0f};
		return true;
	}

	return false;
}

bool FPropertyWatcher::GetObjFromObjPointerProp(PropertyItem& Item, UObject*& Object)
{
	if (Item.Prop &&
		(Item.Prop->IsA(FWeakObjectProperty::StaticClass()) ||
			Item.Prop->IsA(FLazyObjectProperty::StaticClass()) ||
			Item.Prop->IsA(FSoftObjectProperty::StaticClass())))
	{
		bool IsValid = false;
		UObject* Obj = 0;
		if (Item.Prop->IsA(FWeakObjectProperty::StaticClass()))
		{
			IsValid = ((TWeakObjectPtr<UObject>*)Item.Ptr)->IsValid();
			Obj = ((TWeakObjectPtr<UObject>*)Item.Ptr)->Get();
		}
		else if (Item.Prop->IsA(FLazyObjectProperty::StaticClass()))
		{
			IsValid = ((TLazyObjectPtr<UObject>*)Item.Ptr)->IsValid();
			Obj = ((TLazyObjectPtr<UObject>*)Item.Ptr)->Get();
		}
		else if (Item.Prop->IsA(FSoftObjectProperty::StaticClass()))
		{
			IsValid = ((TSoftObjectPtr<UObject>*)Item.Ptr)->IsValid();
			Obj = ((TSoftObjectPtr<UObject>*)Item.Ptr)->Get();
		}
		Object = Obj;
		return IsValid;
	}

	return false;
}

// -------------------------------------------------------------------------------------------

// @Todo: Improve.
const char* SearchBoxHelpText =
	"Multiple search terms can be entered.\n"
	"\n"
	"Operators are: \n"
	"	AND -> & or whitespace\n"
	"	OR  -> |\n"
	"	NOT -> !\n"
	"	\n"
	"	Example: \n"
	"		(a !b) | !(c & d)\n"
	"\n"
	"Modifiers for search terms are:\n"
	"	Exact -> +word\n"
	"	Regex -> regex: or reg: or r:\n"
	"	Value Comparisons -> =value, >value, <value, >=value, <=value\n"
	"\n"
	"Specify table column entries like this:\n"
	"	name:, value:, metadata:, type:, cpptype:, class:, category:, address:, size:\n"
	"\n"
	"	(name: is default, so the search term \"varName\" searches the property name column.)\n"
	"\n"
	"	Example: \n"
	"		intVar (value:>=3 | metadata:actor) size:>=10 size:<=100 cpptype:+int8\n"
	;

// @Todo: Improve.
const char* HelpText =
	"Drag an item somewhere to add it to the watch list.\n"
	"\n"
	"Shift click on a node -> Open/Close all.\n"
	"Shift click + digit on a node -> Specify how many layers to open.\n"
	"  Usefull since doing open all on an actor for example can open a whole lot of things.\n"
	"\n"
	"Right click on an item to inline it.\n"
	;


#if defined(__clang__)
#pragma clang diagnostic pop
#endif