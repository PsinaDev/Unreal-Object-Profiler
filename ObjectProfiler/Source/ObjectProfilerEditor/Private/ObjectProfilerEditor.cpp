//Copyright PsinaDev 2025.

#include "ObjectProfilerEditor.h"
#include "ObjectProfilerCommands.h"
#include "ObjectProfilerCore.h"
#include "SObjectProfilerWindow.h"

#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ObjectProfilerEditor"

const FName FObjectProfilerEditorModule::ProfilerTabName("ObjectProfilerTab");

void FObjectProfilerEditorModule::StartupModule()
{
	FObjectProfilerCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FObjectProfilerCommands::Get().OpenProfilerWindow,
		FExecuteAction::CreateRaw(this, &FObjectProfilerEditorModule::OpenProfilerWindow),
		FCanExecuteAction());
	
	PluginCommands->MapAction(
		FObjectProfilerCommands::Get().TakeQuickSnapshot,
		FExecuteAction::CreateRaw(this, &FObjectProfilerEditorModule::TakeQuickSnapshot),
		FCanExecuteAction());
	
	PluginCommands->MapAction(
		FObjectProfilerCommands::Get().ForceGarbageCollection,
		FExecuteAction::CreateRaw(this, &FObjectProfilerEditorModule::ForceGarbageCollection),
		FCanExecuteAction());
	
	PluginCommands->MapAction(
		FObjectProfilerCommands::Get().ToggleRealTimeMode,
		FExecuteAction::CreateRaw(this, &FObjectProfilerEditorModule::ToggleRealTimeMode),
		FCanExecuteAction());

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ProfilerTabName,
		FOnSpawnTab::CreateRaw(this, &FObjectProfilerEditorModule::OnSpawnProfilerTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Object Profiler"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Analyze UObject instances in memory"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FObjectProfilerEditorModule::RegisterMenus));
}

void FObjectProfilerEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FObjectProfilerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ProfilerTabName);
}

void FObjectProfilerEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("Misc");
		
		Section.AddMenuEntryWithCommandList(
			FObjectProfilerCommands::Get().OpenProfilerWindow,
			PluginCommands,
			LOCTEXT("MenuEntry", "Object Profiler"),
			LOCTEXT("MenuEntryTooltip", "Open Object Profiler window (Alt+Shift+O)")
		);
	}

	{
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("Instrumentation");
		
		Section.AddMenuEntryWithCommandList(
			FObjectProfilerCommands::Get().OpenProfilerWindow,
			PluginCommands,
			LOCTEXT("ToolsMenuEntry", "Object Profiler"),
			LOCTEXT("ToolsMenuEntryTooltip", "Analyze UObject instances in memory")
		);
	}
}

void FObjectProfilerEditorModule::OpenProfilerWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ProfilerTabName);
}

void FObjectProfilerEditorModule::TakeQuickSnapshot()
{
	FObjectProfilerCore::TakeSnapshot(FString::Printf(TEXT("Quick_%s"), *FDateTime::Now().ToString(TEXT("%H%M%S"))));
	
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Quick snapshot taken via hotkey"));
}

void FObjectProfilerEditorModule::ForceGarbageCollection()
{
	FObjectProfilerCore::ForceGarbageCollection();
}

void FObjectProfilerEditorModule::ToggleRealTimeMode()
{
	if (FObjectProfilerCore::IsRealTimeMonitoringActive())
	{
		FObjectProfilerCore::StopRealTimeMonitoring();
		UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Real-time monitoring stopped via hotkey"));
	}
	else
	{
		FObjectProfilerCore::StartRealTimeMonitoring(1.0f);
		UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Real-time monitoring started via hotkey"));
	}
}

TSharedRef<SDockTab> FObjectProfilerEditorModule::OnSpawnProfilerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SObjectProfilerWindow> ProfilerWindow = SNew(SObjectProfilerWindow);
	ProfilerWindowPtr = ProfilerWindow;
	
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabLabel", "Object Profiler"))
		[
			ProfilerWindow
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FObjectProfilerEditorModule, ObjectProfilerEditor)