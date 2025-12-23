//Copyright PsinaDev 2025.

#include "ObjectProfilerCommands.h"

#define LOCTEXT_NAMESPACE "ObjectProfiler"

void FObjectProfilerCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenProfilerWindow,
		"Object Profiler",
		"Open the Object Profiler window to analyze UObject instances in memory",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::O)
	);
	
	UI_COMMAND(
		TakeQuickSnapshot,
		"Quick Snapshot",
		"Take a quick snapshot of current object counts",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::S)
	);
	
	UI_COMMAND(
		ForceGarbageCollection,
		"Force GC",
		"Force garbage collection",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::G)
	);
	
	UI_COMMAND(
		ToggleRealTimeMode,
		"Toggle Real-Time",
		"Toggle real-time monitoring mode",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::R)
	);
}

#undef LOCTEXT_NAMESPACE