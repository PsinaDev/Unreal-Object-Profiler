//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FObjectProfilerCommands : public TCommands<FObjectProfilerCommands>
{
public:
	FObjectProfilerCommands()
		: TCommands<FObjectProfilerCommands>(
			TEXT("ObjectProfiler"),
			NSLOCTEXT("Contexts", "ObjectProfiler", "Object Profiler"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenProfilerWindow;
	TSharedPtr<FUICommandInfo> TakeQuickSnapshot;
	TSharedPtr<FUICommandInfo> ForceGarbageCollection;
	TSharedPtr<FUICommandInfo> ToggleRealTimeMode;
};