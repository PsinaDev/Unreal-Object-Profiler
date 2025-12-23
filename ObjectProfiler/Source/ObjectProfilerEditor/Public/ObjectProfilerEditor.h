//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;
class SDockTab;
class SObjectProfilerWindow;

class FObjectProfilerEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void OpenProfilerWindow();
    void TakeQuickSnapshot();
    void ForceGarbageCollection();
    void ToggleRealTimeMode();

private:
    void RegisterMenus();
    TSharedRef<SDockTab> OnSpawnProfilerTab(const FSpawnTabArgs& SpawnTabArgs);

    TSharedPtr<FUICommandList> PluginCommands;
    TWeakPtr<SObjectProfilerWindow> ProfilerWindowPtr;
	
    static const FName ProfilerTabName;
};