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

void FPropertyWatcher::TreeState::EnableForceToggleNode(bool Mode, int StackIndexLimit)
{
    ForceToggleNodeOpenClose = true;
    ForceToggleNodeMode = Mode;
    ForceToggleNodeStackIndexLimit = StackIndexLimit;

    VisitedPropertiesStack.Empty();
}

bool FPropertyWatcher::TreeState::ItemIsInfiniteLooping(VisitedPropertyInfo& PropertyInfo)
{
    if (!PropertyInfo.Address)
        return false;

    int Count = 0;
    for (auto& It : VisitedPropertiesStack)
    {
        if (It.Compare(PropertyInfo))
        {
            Count++;
            if (Count == 3) // @Todo: Think about what's appropriate.
                return true;
        }
    }
    return false;
}