//Copyright PsinaDev 2025.

#include "ObjectProfilerCore.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectArray.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimationAsset.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "Engine/Blueprint.h"
#include "Subsystems/Subsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

TArray<FObjectSnapshot> FObjectProfilerCore::SnapshotHistory;
TMap<FString, TSharedPtr<FObjectClassStats>> FObjectProfilerCore::PreviousStats;
FObjectProfilerCore::FAsyncCollectionState FObjectProfilerCore::AsyncState;
bool FObjectProfilerCore::bCancellationRequested = false;

FTSTicker::FDelegateHandle FObjectProfilerCore::RealTimeTickerHandle;
float FObjectProfilerCore::RealTimeIntervalSeconds = 1.0f;
bool FObjectProfilerCore::bRealTimeMonitoringActive = false;
double FObjectProfilerCore::LastRealTimeUpdateTime = 0.0;
int32 FObjectProfilerCore::LeakDetectionThreshold = 5;

FOnRealTimeUpdate FObjectProfilerCore::RealTimeUpdateDelegate;
FOnSnapshotTaken FObjectProfilerCore::SnapshotTakenDelegate;

FCriticalSection FObjectProfilerCore::IncrementalDataLock;
TMap<UClass*, FObjectProfilerCore::FIncrementalClassData> FObjectProfilerCore::IncrementalClassCounts;
bool FObjectProfilerCore::bIncrementalTrackingActive = false;
FObjectProfilerCore::FProfilerCreateListener* FObjectProfilerCore::CreateListener = nullptr;
FObjectProfilerCore::FProfilerDeleteListener* FObjectProfilerCore::DeleteListener = nullptr;

void FObjectProfilerCore::FProfilerCreateListener::NotifyUObjectCreated(const UObjectBase* Object, int32 Index)
{
	if (!Object)
	{
		return;
	}
	
	const EObjectFlags Flags = Object->GetFlags();
	if (Flags & (RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	
	UClass* ObjClass = Object->GetClass();
	if (!ObjClass)
	{
		return;
	}
	
	FScopeLock Lock(&IncrementalDataLock);
	FIncrementalClassData& Data = IncrementalClassCounts.FindOrAdd(ObjClass);
	Data.Count++;
	Data.bDirty = true;
}

void FObjectProfilerCore::FProfilerCreateListener::OnUObjectArrayShutdown()
{
	GUObjectArray.RemoveUObjectCreateListener(this);
}

void FObjectProfilerCore::FProfilerDeleteListener::NotifyUObjectDeleted(const UObjectBase* Object, int32 Index)
{
	if (!Object)
	{
		return;
	}
	
	UClass* ObjClass = Object->GetClass();
	if (!ObjClass)
	{
		return;
	}
	
	FScopeLock Lock(&IncrementalDataLock);
	if (FIncrementalClassData* Data = IncrementalClassCounts.Find(ObjClass))
	{
		Data->Count--;
		Data->bDirty = true;
		
		if (Data->Count <= 0)
		{
			IncrementalClassCounts.Remove(ObjClass);
		}
	}
}

void FObjectProfilerCore::FProfilerDeleteListener::OnUObjectArrayShutdown()
{
	GUObjectArray.RemoveUObjectDeleteListener(this);
}

void FObjectProfilerCore::StartIncrementalTracking()
{
	if (bIncrementalTrackingActive)
	{
		return;
	}
	
	{
		FScopeLock Lock(&IncrementalDataLock);
		IncrementalClassCounts.Empty();
		IncrementalClassCounts.Reserve(2000);
		
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Obj = *It;
			if (!IsValid(Obj) || Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
			{
				continue;
			}
			
			UClass* ObjClass = Obj->GetClass();
			if (ObjClass)
			{
				IncrementalClassCounts.FindOrAdd(ObjClass).Count++;
			}
		}
	}
	
	CreateListener = new FProfilerCreateListener();
	DeleteListener = new FProfilerDeleteListener();
	
	GUObjectArray.AddUObjectCreateListener(CreateListener);
	GUObjectArray.AddUObjectDeleteListener(DeleteListener);
	
	bIncrementalTrackingActive = true;
	
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Incremental tracking started with %d classes"), IncrementalClassCounts.Num());
}

void FObjectProfilerCore::StopIncrementalTracking()
{
	if (!bIncrementalTrackingActive)
	{
		return;
	}
	
	if (CreateListener)
	{
		GUObjectArray.RemoveUObjectCreateListener(CreateListener);
		delete CreateListener;
		CreateListener = nullptr;
	}
	
	if (DeleteListener)
	{
		GUObjectArray.RemoveUObjectDeleteListener(DeleteListener);
		delete DeleteListener;
		DeleteListener = nullptr;
	}
	
	{
		FScopeLock Lock(&IncrementalDataLock);
		IncrementalClassCounts.Empty();
	}
	
	bIncrementalTrackingActive = false;
	
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Incremental tracking stopped"));
}

bool FObjectProfilerCore::IsIncrementalTrackingActive()
{
	return bIncrementalTrackingActive;
}

TArray<TSharedPtr<FObjectClassStats>> FObjectProfilerCore::GetIncrementalStats()
{
	TArray<TSharedPtr<FObjectClassStats>> Result;
	const double CurrentTime = FPlatformTime::Seconds();
	
	FScopeLock Lock(&IncrementalDataLock);
	
	Result.Reserve(IncrementalClassCounts.Num());
	
	for (auto& Pair : IncrementalClassCounts)
	{
		UClass* ObjClass = Pair.Key;
		FIncrementalClassData& Data = Pair.Value;
		
		if (!ObjClass || Data.Count <= 0)
		{
			continue;
		}
		
		TSharedPtr<FObjectClassStats> Stats = MakeShared<FObjectClassStats>();
		Stats->ClassName = ObjClass->GetName();
		Stats->ClassFName = ObjClass->GetFName();
		Stats->ClassPtr = ObjClass;
		Stats->InstanceCount = Data.Count;
		Stats->bSizeAvailable = false;
		
		if (Data.CachedCategory == EObjectCategory::Unknown && Data.bDirty)
		{
			Data.CachedCategory = CategorizeClass(ObjClass);
			Data.CachedSource = GetObjectSource(ObjClass);
			Data.CachedModuleName = GetModuleName(ObjClass);
			Data.bDirty = false;
		}
		
		Stats->Category = Data.CachedCategory;
		Stats->Source = Data.CachedSource;
		Stats->ModuleName = Data.CachedModuleName;
		
		if (const TSharedPtr<FObjectClassStats>* PrevStats = PreviousStats.Find(Stats->ClassName))
		{
			Stats->DeltaCount = Stats->InstanceCount - (*PrevStats)->InstanceCount;
			Stats->History = (*PrevStats)->History;
			Stats->TotalSizeBytes = (*PrevStats)->TotalSizeBytes;
			Stats->AverageSizeBytes = (*PrevStats)->AverageSizeBytes;
			Stats->bSizeAvailable = (*PrevStats)->bSizeAvailable;
		}
		
		Stats->AddHistoryPoint(Stats->InstanceCount, Stats->TotalSizeBytes, CurrentTime);
		Stats->UpdateLeakDetection(LeakDetectionThreshold);
		Stats->CalculateRateOfChange();
		
		PreviousStats.Add(Stats->ClassName, Stats);
		Result.Add(Stats);
	}
	
	return Result;
}

bool FObjectProfilerCore::IsSafeForResourceSizeQuery(const UObject* Obj)
{
	if (!Obj)
	{
		return false;
	}

	if (Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return false;
	}

	const UClass* ObjClass = Obj->GetClass();
	if (!ObjClass)
	{
		return false;
	}

	if (ObjClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (Obj->IsA(UField::StaticClass()) ||
		Obj->IsA(UStruct::StaticClass()) ||
		Obj->IsA(UPackage::StaticClass()))
	{
		return false;
	}

	return true;
}

EObjectCategory FObjectProfilerCore::CategorizeClass(const UClass* InClass)
{
	if (!InClass)
	{
		return EObjectCategory::Unknown;
	}

	if (InClass->IsChildOf(AActor::StaticClass()))
	{
		return EObjectCategory::Actor;
	}
	if (InClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return EObjectCategory::Component;
	}
	if (InClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return EObjectCategory::Widget;
	}
	if (InClass->IsChildOf(UTexture::StaticClass()))
	{
		return EObjectCategory::Texture;
	}
	if (InClass->IsChildOf(UStaticMesh::StaticClass()) || InClass->IsChildOf(USkeletalMesh::StaticClass()))
	{
		return EObjectCategory::Mesh;
	}
	if (InClass->IsChildOf(UMaterialInterface::StaticClass()))
	{
		return EObjectCategory::Material;
	}
	if (InClass->IsChildOf(USoundBase::StaticClass()))
	{
		return EObjectCategory::Audio;
	}
	if (InClass->IsChildOf(UAnimationAsset::StaticClass()))
	{
		return EObjectCategory::Animation;
	}
	if (InClass->IsChildOf(UBlueprint::StaticClass()))
	{
		return EObjectCategory::Blueprint;
	}
	if (InClass->IsChildOf(UDataAsset::StaticClass()))
	{
		return EObjectCategory::DataAsset;
	}
	if (InClass->IsChildOf(USubsystem::StaticClass()))
	{
		return EObjectCategory::Subsystem;
	}
	if (InClass->IsChildOf(UGameInstance::StaticClass()))
	{
		return EObjectCategory::GameInstance;
	}

	const UPackage* Package = InClass->GetOutermost();
	if (Package)
	{
		const FString PackageName = Package->GetName();
		if (PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/Content/")))
		{
			return EObjectCategory::Asset;
		}
	}

	return EObjectCategory::Other;
}

FString FObjectProfilerCore::GetModuleName(const UClass* InClass)
{
	if (!InClass)
	{
		return TEXT("Unknown");
	}

	const UPackage* Package = InClass->GetOutermost();
	if (!Package)
	{
		return TEXT("Unknown");
	}

	const FString PackageName = Package->GetName();
	
	if (PackageName.StartsWith(TEXT("/Script/")))
	{
		FString ModuleName = PackageName.RightChop(8);
		int32 SlashIndex;
		if (ModuleName.FindChar(TEXT('/'), SlashIndex))
		{
			ModuleName = ModuleName.Left(SlashIndex);
		}
		return ModuleName;
	}
	
	if (PackageName.StartsWith(TEXT("/Game/")))
	{
		return TEXT("Game");
	}
	
	if (PackageName.StartsWith(TEXT("/Engine/")))
	{
		return TEXT("Engine");
	}

	const int32 FirstSlash = PackageName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
	if (FirstSlash != INDEX_NONE)
	{
		return PackageName.Mid(1, FirstSlash - 1);
	}

	return TEXT("Other");
}

EObjectSource FObjectProfilerCore::GetObjectSource(const UClass* InClass)
{
	if (!InClass)
	{
		return EObjectSource::Unknown;
	}

	const FString ClassName = InClass->GetName();
	const FString ModuleName = GetModuleName(InClass);
	
	if (ClassName.StartsWith(TEXT("K2Node_")) || 
		ClassName.StartsWith(TEXT("UK2Node")) ||
		ClassName.StartsWith(TEXT("BPGC_")) ||
		ClassName.Contains(TEXT("BlueprintGeneratedClass")) ||
		ClassName.StartsWith(TEXT("Function")) ||
		ClassName.StartsWith(TEXT("UFunction")) ||
		ClassName.StartsWith(TEXT("DelegateFunction")) ||
		ModuleName == TEXT("BlueprintGraph") ||
		ModuleName == TEXT("KismetCompiler") ||
		ModuleName == TEXT("Kismet"))
	{
		return EObjectSource::BlueprintVM;
	}
	
	if (ModuleName == TEXT("UnrealEd") ||
		ModuleName == TEXT("EditorStyle") ||
		ModuleName == TEXT("EditorFramework") ||
		ModuleName == TEXT("LevelEditor") ||
		ModuleName == TEXT("ContentBrowser") ||
		ModuleName == TEXT("PropertyEditor") ||
		ModuleName == TEXT("DetailCustomizations") ||
		ModuleName == TEXT("GraphEditor") ||
		ModuleName == TEXT("Sequencer") ||
		ModuleName == TEXT("Persona") ||
		ModuleName == TEXT("Blutility") ||
		ModuleName.EndsWith(TEXT("Editor")) ||
		ModuleName.Contains(TEXT("Editor")))
	{
		return EObjectSource::EngineEditor;
	}
	
	if (ModuleName == TEXT("Core") ||
		ModuleName == TEXT("CoreUObject") ||
		ModuleName == TEXT("CoreOnline"))
	{
		return EObjectSource::EngineCore;
	}
	
	if (ModuleName == TEXT("Engine") ||
		ModuleName == TEXT("Slate") ||
		ModuleName == TEXT("SlateCore") ||
		ModuleName == TEXT("UMG") ||
		ModuleName == TEXT("InputCore") ||
		ModuleName == TEXT("RenderCore") ||
		ModuleName == TEXT("RHI") ||
		ModuleName == TEXT("Renderer") ||
		ModuleName == TEXT("PhysicsCore") ||
		ModuleName == TEXT("Chaos") ||
		ModuleName == TEXT("NavigationSystem") ||
		ModuleName == TEXT("AIModule") ||
		ModuleName == TEXT("GameplayTasks") ||
		ModuleName == TEXT("GameplayAbilities") ||
		ModuleName == TEXT("EnhancedInput") ||
		ModuleName == TEXT("Niagara") ||
		ModuleName == TEXT("MovieScene") ||
		ModuleName == TEXT("LevelSequence"))
	{
		return EObjectSource::EngineRuntime;
	}
	
	const UPackage* Package = InClass->GetOutermost();
	if (Package)
	{
		const FString PackageName = Package->GetName();
		
		if (PackageName.StartsWith(TEXT("/Game/")) || 
			PackageName.StartsWith(TEXT("/Content/")))
		{
			return EObjectSource::GameContent;
		}
		
		if (PackageName.Contains(TEXT("/Plugins/")))
		{
			return EObjectSource::Plugin;
		}
		
		if (PackageName.StartsWith(TEXT("/Script/")))
		{
			FString ScriptModule = PackageName.RightChop(8);
			int32 SlashIdx;
			if (ScriptModule.FindChar(TEXT('/'), SlashIdx))
			{
				ScriptModule = ScriptModule.Left(SlashIdx);
			}
			
			if (ScriptModule.StartsWith(TEXT("Protocol")) || 
				ScriptModule == TEXT("Game") ||
				(!ScriptModule.StartsWith(TEXT("Engine")) && 
				 !ScriptModule.StartsWith(TEXT("Core")) && 
				 !ScriptModule.StartsWith(TEXT("Unreal")) &&
				 !ScriptModule.StartsWith(TEXT("Editor")) &&
				 !ScriptModule.Contains(TEXT("Editor"))))
			{
				return EObjectSource::GameCode;
			}
		}
	}
	
	return EObjectSource::Unknown;
}

FString FObjectProfilerCore::GetSourceDisplayName(EObjectSource Source)
{
	switch (Source)
	{
	case EObjectSource::EngineCore:
		return TEXT("Engine Core");
	case EObjectSource::EngineEditor:
		return TEXT("Engine Editor");
	case EObjectSource::EngineRuntime:
		return TEXT("Engine Runtime");
	case EObjectSource::BlueprintVM:
		return TEXT("Blueprint VM");
	case EObjectSource::GameCode:
		return TEXT("Game Code");
	case EObjectSource::GameContent:
		return TEXT("Game Content");
	case EObjectSource::Plugin:
		return TEXT("Plugin");
	case EObjectSource::ThirdParty:
		return TEXT("Third Party");
	default:
		return TEXT("Unknown");
	}
}

TArray<TSharedPtr<FObjectClassStats>> FObjectProfilerCore::CollectObjectStats(int32 MaxSamplesPerClass)
{
	TMap<UClass*, TSharedPtr<FObjectClassStats>> StatsMap;
	const double CurrentTime = FPlatformTime::Seconds();

	int32 TotalObjects = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		TotalObjects++;
	}

	FScopedSlowTask SlowTask(TotalObjects, FText::FromString(TEXT("Collecting object statistics...")));
	SlowTask.MakeDialog(true);

	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}

		SlowTask.EnterProgressFrame(1);

		UObject* Obj = *It;
		if (!IsValid(Obj))
		{
			continue;
		}

		if (Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		UClass* ObjClass = Obj->GetClass();

		TSharedPtr<FObjectClassStats>& StatsPtr = StatsMap.FindOrAdd(ObjClass);
		
		if (!StatsPtr.IsValid())
		{
			StatsPtr = MakeShared<FObjectClassStats>();
			StatsPtr->ClassName = ObjClass->GetName();
			StatsPtr->ClassFName = ObjClass->GetFName();
			StatsPtr->ClassPtr = ObjClass;
			StatsPtr->Category = CategorizeClass(ObjClass);
			StatsPtr->Source = GetObjectSource(ObjClass);
			StatsPtr->ModuleName = GetModuleName(ObjClass);
			StatsPtr->bSizeAvailable = false;
		}
		
		FObjectClassStats& Stats = *StatsPtr;
		Stats.InstanceCount++;

		const bool bSafeForThisObject = IsSafeForResourceSizeQuery(Obj);
		
		if (bSafeForThisObject)
		{
			Stats.TotalSizeBytes += Obj->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
			Stats.bSizeAvailable = true;
		}

		if (Stats.SampleObjectNames.Num() < MaxSamplesPerClass)
		{
			Stats.SampleObjectNames.Add(Obj->GetPathName());
		}
	}

	TArray<TSharedPtr<FObjectClassStats>> Result;
	StatsMap.GenerateValueArray(Result);

	for (auto& Stats : Result)
	{
		if (Stats->InstanceCount > 0 && Stats->bSizeAvailable && Stats->TotalSizeBytes > 0)
		{
			Stats->AverageSizeBytes = Stats->TotalSizeBytes / Stats->InstanceCount;
		}
		
		Stats->AddHistoryPoint(Stats->InstanceCount, Stats->TotalSizeBytes, CurrentTime);
		
		if (const TSharedPtr<FObjectClassStats>* PrevStats = PreviousStats.Find(Stats->ClassName))
		{
			Stats->DeltaCount = Stats->InstanceCount - (*PrevStats)->InstanceCount;
			Stats->DeltaSizeBytes = Stats->TotalSizeBytes - (*PrevStats)->TotalSizeBytes;
			
			Stats->History = (*PrevStats)->History;
			Stats->AddHistoryPoint(Stats->InstanceCount, Stats->TotalSizeBytes, CurrentTime);
			Stats->UpdateLeakDetection(LeakDetectionThreshold);
			Stats->CalculateRateOfChange();
		}
		
		PreviousStats.Add(Stats->ClassName, Stats);
	}

	return Result;
}

void FObjectProfilerCore::CollectObjectStatsAsync(
	int32 MaxSamplesPerClass,
	FOnObjectStatsCollected OnComplete,
	FOnCollectionProgress OnProgress)
{
	if (AsyncState.Phase != EAsyncPhase::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("ObjectProfiler: Async collection already in progress"));
		return;
	}

	AsyncState.Reset();
	bCancellationRequested = false;
	
	AsyncState.MaxSamplesPerClass = MaxSamplesPerClass;
	AsyncState.OnComplete = OnComplete;
	AsyncState.OnProgress = OnProgress;
	AsyncState.Phase = EAsyncPhase::Enumerating;
	
	AsyncState.TotalObjectsEstimate = GetTotalObjectCount();
	AsyncState.ObjectsToProcess.Reserve(AsyncState.TotalObjectsEstimate);

	AsyncState.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&FObjectProfilerCore::OnAsyncTick),
		0.0f
	);
}

void FObjectProfilerCore::CancelAsyncCollection()
{
	bCancellationRequested = true;
}

bool FObjectProfilerCore::IsAsyncCollectionInProgress()
{
	return AsyncState.Phase != EAsyncPhase::Idle;
}

bool FObjectProfilerCore::OnAsyncTick(float DeltaTime)
{
	if (bCancellationRequested)
	{
		AsyncState.Reset();
		bCancellationRequested = false;
		return false;
	}

	switch (AsyncState.Phase)
	{
	case EAsyncPhase::Enumerating:
		TickEnumeration();
		break;
		
	case EAsyncPhase::Processing:
		TickProcessing();
		break;
		
	case EAsyncPhase::Complete:
		FinalizeCollection();
		return false;
		
	default:
		return false;
	}

	return true;
}

void FObjectProfilerCore::TickEnumeration()
{
	const int32 MaxObjectIndex = GUObjectArray.GetObjectArrayNum();
	int32 ProcessedThisTick = 0;
	
	while (AsyncState.EnumerationIndex < MaxObjectIndex && ProcessedThisTick < EnumerationChunkSize)
	{
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(AsyncState.EnumerationIndex);
		AsyncState.EnumerationIndex++;
		
		if (!ObjectItem)
		{
			continue;
		}
		
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
		UObject* Obj = static_cast<UObject*>(ObjectItem->GetObject());
#else
		UObject* Obj = static_cast<UObject*>(ObjectItem->Object);
#endif
		
		if (Obj && IsValid(Obj) && !Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			AsyncState.ObjectsToProcess.Add(Obj);
		}
		
		ProcessedThisTick++;
	}

	if (AsyncState.OnProgress.IsBound() && MaxObjectIndex > 0)
	{
		constexpr float EnumerationWeight = 0.2f;
		const float Progress = EnumerationWeight * 
			(static_cast<float>(AsyncState.EnumerationIndex) / static_cast<float>(MaxObjectIndex));
		AsyncState.OnProgress.Execute(FMath::Clamp(Progress, 0.0f, EnumerationWeight));
	}

	if (AsyncState.EnumerationIndex >= MaxObjectIndex)
	{
		AsyncState.Phase = EAsyncPhase::Processing;
	}
}

void FObjectProfilerCore::TickProcessing()
{
	const int32 TotalObjects = AsyncState.ObjectsToProcess.Num();
	const int32 EndIndex = FMath::Min(AsyncState.ProcessingIndex + ProcessingChunkSize, TotalObjects);

	for (int32 i = AsyncState.ProcessingIndex; i < EndIndex; ++i)
	{
		UObject* Obj = AsyncState.ObjectsToProcess[i].Get();
		if (!IsValid(Obj))
		{
			continue;
		}

		UClass* ObjClass = Obj->GetClass();
		if (!ObjClass)
		{
			continue;
		}

		TSharedPtr<FObjectClassStats>& StatsPtr = AsyncState.StatsMap.FindOrAdd(ObjClass);
		
		if (!StatsPtr.IsValid())
		{
			StatsPtr = MakeShared<FObjectClassStats>();
			StatsPtr->ClassName = ObjClass->GetName();
			StatsPtr->ClassFName = ObjClass->GetFName();
			StatsPtr->ClassPtr = ObjClass;
			StatsPtr->Category = CategorizeClass(ObjClass);
			StatsPtr->Source = GetObjectSource(ObjClass);
			StatsPtr->ModuleName = GetModuleName(ObjClass);
			StatsPtr->bSizeAvailable = false;
		}
		
		FObjectClassStats& Stats = *StatsPtr;
		Stats.InstanceCount++;

		if (IsSafeForResourceSizeQuery(Obj))
		{
			Stats.TotalSizeBytes += Obj->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
			Stats.bSizeAvailable = true;
		}

		if (Stats.SampleObjectNames.Num() < AsyncState.MaxSamplesPerClass)
		{
			Stats.SampleObjectNames.Add(Obj->GetPathName());
		}
	}

	AsyncState.ProcessingIndex = EndIndex;

	if (AsyncState.OnProgress.IsBound() && TotalObjects > 0)
	{
		constexpr float EnumerationWeight = 0.2f;
		constexpr float ProcessingWeight = 0.8f;
		const float Progress = EnumerationWeight + ProcessingWeight * 
			(static_cast<float>(AsyncState.ProcessingIndex) / static_cast<float>(TotalObjects));
		AsyncState.OnProgress.Execute(FMath::Clamp(Progress, 0.0f, 1.0f));
	}

	if (AsyncState.ProcessingIndex >= TotalObjects)
	{
		AsyncState.Phase = EAsyncPhase::Complete;
	}
}

void FObjectProfilerCore::FinalizeCollection()
{
	const double CurrentTime = FPlatformTime::Seconds();
	TArray<TSharedPtr<FObjectClassStats>> Result;
	AsyncState.StatsMap.GenerateValueArray(Result);

	for (auto& Stats : Result)
	{
		if (Stats->InstanceCount > 0 && Stats->bSizeAvailable && Stats->TotalSizeBytes > 0)
		{
			Stats->AverageSizeBytes = Stats->TotalSizeBytes / Stats->InstanceCount;
		}
		
		if (const TSharedPtr<FObjectClassStats>* PrevStats = PreviousStats.Find(Stats->ClassName))
		{
			Stats->DeltaCount = Stats->InstanceCount - (*PrevStats)->InstanceCount;
			Stats->DeltaSizeBytes = Stats->TotalSizeBytes - (*PrevStats)->TotalSizeBytes;
			
			Stats->History = (*PrevStats)->History;
			Stats->AddHistoryPoint(Stats->InstanceCount, Stats->TotalSizeBytes, CurrentTime);
			Stats->UpdateLeakDetection(LeakDetectionThreshold);
			Stats->CalculateRateOfChange();
		}
		else
		{
			Stats->AddHistoryPoint(Stats->InstanceCount, Stats->TotalSizeBytes, CurrentTime);
		}
		
		PreviousStats.Add(Stats->ClassName, Stats);
	}

	FOnObjectStatsCollected CompletionCallback = AsyncState.OnComplete;
	
	AsyncState.Reset();

	if (CompletionCallback.IsBound())
	{
		CompletionCallback.Execute(Result);
	}
}

TArray<FString> FObjectProfilerCore::GetInstancesOfClass(const UClass* InClass)
{
	TArray<FString> Result;

	if (!InClass)
	{
		return Result;
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (IsValid(Obj) && Obj->GetClass() == InClass)
		{
			FString Entry = FString::Printf(TEXT("%s | Outer: %s | Flags: 0x%08X"),
				*Obj->GetPathName(),
				Obj->GetOuter() ? *Obj->GetOuter()->GetName() : TEXT("None"),
				static_cast<uint32>(Obj->GetFlags()));
			Result.Add(Entry);
		}
	}

	return Result;
}

TArray<FString> FObjectProfilerCore::GetInstancesOfClass(const FString& ClassName)
{
	TArray<FString> Result;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (IsValid(Obj) && Obj->GetClass()->GetName() == ClassName)
		{
			FString Entry = FString::Printf(TEXT("%s | Outer: %s | Flags: 0x%08X"),
				*Obj->GetPathName(),
				Obj->GetOuter() ? *Obj->GetOuter()->GetName() : TEXT("None"),
				static_cast<uint32>(Obj->GetFlags()));
			Result.Add(Entry);
		}
	}

	return Result;
}

void FObjectProfilerCore::TakeSnapshot(const FString& SnapshotName)
{
	FObjectSnapshot Snapshot;
	Snapshot.Name = SnapshotName.IsEmpty() 
		? FString::Printf(TEXT("Snapshot_%d"), SnapshotHistory.Num() + 1) 
		: SnapshotName;
	Snapshot.Timestamp = FDateTime::Now();

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (IsValid(Obj) && !Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			const FString ClassName = Obj->GetClass()->GetName();
			Snapshot.ClassCounts.FindOrAdd(ClassName)++;
			Snapshot.TotalObjects++;
			
			if (IsSafeForResourceSizeQuery(Obj))
			{
				const int64 Size = Obj->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
				Snapshot.ClassSizes.FindOrAdd(ClassName) += Size;
				Snapshot.TotalSize += Size;
			}
		}
	}

	if (SnapshotHistory.Num() >= MaxSnapshotHistory)
	{
		SnapshotHistory.RemoveAt(0);
	}
	
	SnapshotHistory.Add(Snapshot);
	
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Snapshot '%s' taken with %d unique classes, %d total objects"), 
		*Snapshot.Name, Snapshot.ClassCounts.Num(), Snapshot.TotalObjects);
	
	SnapshotTakenDelegate.Broadcast(Snapshot);
}

TArray<TSharedPtr<FObjectClassStats>> FObjectProfilerCore::GetDeltaSinceSnapshot(int32 SnapshotIndex)
{
	TArray<TSharedPtr<FObjectClassStats>> Result;
	
	if (SnapshotHistory.Num() == 0)
	{
		return Result;
	}
	
	const int32 ActualIndex = SnapshotIndex < 0 ? SnapshotHistory.Num() - 1 : SnapshotIndex;
	if (ActualIndex < 0 || ActualIndex >= SnapshotHistory.Num())
	{
		return Result;
	}
	
	const FObjectSnapshot& Snapshot = SnapshotHistory[ActualIndex];
	TMap<FString, int32> CurrentCounts;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (IsValid(Obj) && !Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			CurrentCounts.FindOrAdd(Obj->GetClass()->GetName())++;
		}
	}

	for (const auto& [ClassName, CurrentCount] : CurrentCounts)
	{
		const int32 SnapshotCount = Snapshot.ClassCounts.FindRef(ClassName);
		const int32 Delta = CurrentCount - SnapshotCount;

		if (Delta != 0)
		{
			TSharedPtr<FObjectClassStats> Stats = MakeShared<FObjectClassStats>();
			Stats->ClassName = ClassName;
			Stats->InstanceCount = Delta;
			Stats->DeltaCount = Delta;
			Result.Add(Stats);
		}
	}
	
	for (const auto& [ClassName, SnapshotCount] : Snapshot.ClassCounts)
	{
		if (!CurrentCounts.Contains(ClassName))
		{
			TSharedPtr<FObjectClassStats> Stats = MakeShared<FObjectClassStats>();
			Stats->ClassName = ClassName;
			Stats->InstanceCount = -SnapshotCount;
			Stats->DeltaCount = -SnapshotCount;
			Result.Add(Stats);
		}
	}

	return Result;
}

TArray<TSharedPtr<FObjectClassStats>> FObjectProfilerCore::CompareTwoSnapshots(int32 IndexA, int32 IndexB)
{
	TArray<TSharedPtr<FObjectClassStats>> Result;
	
	if (IndexA < 0 || IndexA >= SnapshotHistory.Num() || IndexB < 0 || IndexB >= SnapshotHistory.Num())
	{
		return Result;
	}
	
	const FObjectSnapshot& SnapshotA = SnapshotHistory[IndexA];
	const FObjectSnapshot& SnapshotB = SnapshotHistory[IndexB];
	
	TSet<FString> AllClasses;
	for (const auto& [ClassName, Count] : SnapshotA.ClassCounts)
	{
		AllClasses.Add(ClassName);
	}
	for (const auto& [ClassName, Count] : SnapshotB.ClassCounts)
	{
		AllClasses.Add(ClassName);
	}
	
	for (const FString& ClassName : AllClasses)
	{
		const int32 CountA = SnapshotA.ClassCounts.FindRef(ClassName);
		const int32 CountB = SnapshotB.ClassCounts.FindRef(ClassName);
		const int32 Delta = CountB - CountA;
		
		if (Delta != 0)
		{
			TSharedPtr<FObjectClassStats> Stats = MakeShared<FObjectClassStats>();
			Stats->ClassName = ClassName;
			Stats->InstanceCount = Delta;
			Stats->DeltaCount = Delta;
			
			const int64 SizeA = SnapshotA.ClassSizes.FindRef(ClassName);
			const int64 SizeB = SnapshotB.ClassSizes.FindRef(ClassName);
			Stats->DeltaSizeBytes = SizeB - SizeA;
			
			Result.Add(Stats);
		}
	}
	
	return Result;
}

const TArray<FObjectSnapshot>& FObjectProfilerCore::GetSnapshotHistory()
{
	return SnapshotHistory;
}

void FObjectProfilerCore::ClearSnapshotHistory()
{
	SnapshotHistory.Empty();
}

void FObjectProfilerCore::DeleteSnapshot(int32 Index)
{
	if (Index >= 0 && Index < SnapshotHistory.Num())
	{
		SnapshotHistory.RemoveAt(Index);
	}
}

bool FObjectProfilerCore::SaveSnapshotsToFile(const FString& FilePath)
{
	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SnapshotsArray;
	
	for (const FObjectSnapshot& Snapshot : SnapshotHistory)
	{
		TSharedPtr<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
		SnapshotObject->SetStringField(TEXT("Name"), Snapshot.Name);
		SnapshotObject->SetStringField(TEXT("Timestamp"), Snapshot.Timestamp.ToString());
		SnapshotObject->SetNumberField(TEXT("TotalObjects"), Snapshot.TotalObjects);
		SnapshotObject->SetNumberField(TEXT("TotalSize"), static_cast<double>(Snapshot.TotalSize));
		
		TSharedPtr<FJsonObject> ClassCountsObject = MakeShared<FJsonObject>();
		for (const auto& [ClassName, Count] : Snapshot.ClassCounts)
		{
			ClassCountsObject->SetNumberField(ClassName, Count);
		}
		SnapshotObject->SetObjectField(TEXT("ClassCounts"), ClassCountsObject);
		
		TSharedPtr<FJsonObject> ClassSizesObject = MakeShared<FJsonObject>();
		for (const auto& [ClassName, Size] : Snapshot.ClassSizes)
		{
			ClassSizesObject->SetNumberField(ClassName, static_cast<double>(Size));
		}
		SnapshotObject->SetObjectField(TEXT("ClassSizes"), ClassSizesObject);
		
		SnapshotsArray.Add(MakeShared<FJsonValueObject>(SnapshotObject));
	}
	
	RootObject->SetArrayField(TEXT("Snapshots"), SnapshotsArray);
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	
	return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool FObjectProfilerCore::LoadSnapshotsFromFile(const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}
	
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}
	
	const TArray<TSharedPtr<FJsonValue>>* SnapshotsArray;
	if (!RootObject->TryGetArrayField(TEXT("Snapshots"), SnapshotsArray))
	{
		return false;
	}
	
	for (const TSharedPtr<FJsonValue>& SnapshotValue : *SnapshotsArray)
	{
		const TSharedPtr<FJsonObject>* SnapshotObject;
		if (!SnapshotValue->TryGetObject(SnapshotObject))
		{
			continue;
		}
		
		FObjectSnapshot Snapshot;
		Snapshot.Name = (*SnapshotObject)->GetStringField(TEXT("Name"));
		
		const FString TimestampStr = (*SnapshotObject)->GetStringField(TEXT("Timestamp"));
		FDateTime::Parse(TimestampStr, Snapshot.Timestamp);
		
		Snapshot.TotalObjects = (*SnapshotObject)->GetIntegerField(TEXT("TotalObjects"));
		Snapshot.TotalSize = static_cast<int64>((*SnapshotObject)->GetNumberField(TEXT("TotalSize")));
		
		const TSharedPtr<FJsonObject>* ClassCountsObject;
		if ((*SnapshotObject)->TryGetObjectField(TEXT("ClassCounts"), ClassCountsObject))
		{
			for (const auto& [Key, Value] : (*ClassCountsObject)->Values)
			{
				Snapshot.ClassCounts.Add(Key, static_cast<int32>(Value->AsNumber()));
			}
		}
		
		const TSharedPtr<FJsonObject>* ClassSizesObject;
		if ((*SnapshotObject)->TryGetObjectField(TEXT("ClassSizes"), ClassSizesObject))
		{
			for (const auto& [Key, Value] : (*ClassSizesObject)->Values)
			{
				Snapshot.ClassSizes.Add(Key, static_cast<int64>(Value->AsNumber()));
			}
		}
		
		if (SnapshotHistory.Num() < MaxSnapshotHistory)
		{
			SnapshotHistory.Add(Snapshot);
		}
	}
	
	return true;
}

void FObjectProfilerCore::ForceGarbageCollection()
{
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Forced GC complete"));
}

int32 FObjectProfilerCore::GetTotalObjectCount()
{
	int32 Count = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (!(*It)->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			Count++;
		}
	}
	return Count;
}

FString FObjectProfilerCore::FormatBytes(int64 Bytes)
{
	if (Bytes < 0)
	{
		return TEXT("N/A");
	}
	else if (Bytes == 0)
	{
		return TEXT("0 B");
	}
	else if (Bytes < 1024)
	{
		return FString::Printf(TEXT("%lld B"), Bytes);
	}
	else if (Bytes < 1024 * 1024)
	{
		return FString::Printf(TEXT("%.2f KB"), Bytes / 1024.0);
	}
	else if (Bytes < 1024LL * 1024 * 1024)
	{
		return FString::Printf(TEXT("%.2f MB"), Bytes / (1024.0 * 1024.0));
	}
	else
	{
		return FString::Printf(TEXT("%.2f GB"), Bytes / (1024.0 * 1024.0 * 1024.0));
	}
}

void FObjectProfilerCore::StartRealTimeMonitoring(float IntervalSeconds)
{
	if (bRealTimeMonitoringActive)
	{
		StopRealTimeMonitoring();
	}
	
	StartIncrementalTracking();
	
	RealTimeIntervalSeconds = FMath::Max(0.1f, IntervalSeconds);
	bRealTimeMonitoringActive = true;
	LastRealTimeUpdateTime = FPlatformTime::Seconds();
	
	RealTimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&FObjectProfilerCore::OnRealTimeTick),
		RealTimeIntervalSeconds
	);
	
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Real-time monitoring started (interval: %.1fs, incremental: %s)"), 
		RealTimeIntervalSeconds, bIncrementalTrackingActive ? TEXT("yes") : TEXT("no"));
}

void FObjectProfilerCore::StopRealTimeMonitoring()
{
	if (RealTimeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RealTimeTickerHandle);
		RealTimeTickerHandle.Reset();
	}
	
	StopIncrementalTracking();
	
	bRealTimeMonitoringActive = false;
	UE_LOG(LogTemp, Log, TEXT("ObjectProfiler: Real-time monitoring stopped"));
}

bool FObjectProfilerCore::IsRealTimeMonitoringActive()
{
	return bRealTimeMonitoringActive;
}

void FObjectProfilerCore::SetRealTimeInterval(float IntervalSeconds)
{
	RealTimeIntervalSeconds = FMath::Max(0.1f, IntervalSeconds);
	
	if (bRealTimeMonitoringActive)
	{
		StopRealTimeMonitoring();
		StartRealTimeMonitoring(RealTimeIntervalSeconds);
	}
}

float FObjectProfilerCore::GetRealTimeInterval()
{
	return RealTimeIntervalSeconds;
}

FOnRealTimeUpdate& FObjectProfilerCore::OnRealTimeUpdate()
{
	return RealTimeUpdateDelegate;
}

FOnSnapshotTaken& FObjectProfilerCore::OnSnapshotTaken()
{
	return SnapshotTakenDelegate;
}

bool FObjectProfilerCore::OnRealTimeTick(float DeltaTime)
{
	if (!bRealTimeMonitoringActive)
	{
		return false;
	}
	
	UpdateRealTimeStats();
	return true;
}

void FObjectProfilerCore::UpdateRealTimeStats()
{
	if (bIncrementalTrackingActive)
	{
		TArray<TSharedPtr<FObjectClassStats>> Results = GetIncrementalStats();
		LastRealTimeUpdateTime = FPlatformTime::Seconds();
		RealTimeUpdateDelegate.Broadcast(Results);
		return;
	}
	
	if (!IsAsyncCollectionInProgress())
	{
		CollectObjectStatsAsync(
			3,
			FOnObjectStatsCollected::CreateStatic(&FObjectProfilerCore::OnRealTimeCollectionComplete),
			FOnCollectionProgress()
		);
	}
}

void FObjectProfilerCore::OnRealTimeCollectionComplete(TArray<TSharedPtr<FObjectClassStats>> Results)
{
	LastRealTimeUpdateTime = FPlatformTime::Seconds();
	RealTimeUpdateDelegate.Broadcast(Results);
}

TArray<FReferenceInfo> FObjectProfilerCore::GetReferencesTo(const UObject* Object)
{
	TArray<FReferenceInfo> Result;
	
	if (!Object)
	{
		return Result;
	}

	TArray<FReferencerInformation> InternalRefs;
	TArray<FReferencerInformation> ExternalRefs;
	
	const_cast<UObject*>(Object)->RetrieveReferencers(&InternalRefs, &ExternalRefs);
	
	auto ProcessRefs = [&Result, Object](const TArray<FReferencerInformation>& Refs, bool bInternal)
	{
		for (const FReferencerInformation& RefInfo : Refs)
		{
			if (RefInfo.Referencer && RefInfo.Referencer != Object)
			{
				FReferenceInfo Info;
				Info.ObjectPath = Object->GetPathName();
				Info.ReferencerPath = RefInfo.Referencer->GetPathName();
				Info.ReferenceType = bInternal ? TEXT("Internal") : TEXT("External");
				Info.bIsHardReference = true;
				Result.Add(Info);
			}
		}
	};
	
	ProcessRefs(InternalRefs, true);
	ProcessRefs(ExternalRefs, false);
	
	return Result;
}

TArray<FReferenceInfo> FObjectProfilerCore::GetReferencesFrom(const UObject* Object)
{
	TArray<FReferenceInfo> Result;
	
	if (!Object)
	{
		return Result;
	}

	TArray<UObject*> ReferencedObjects;
	FReferenceFinder RefFinder(ReferencedObjects, const_cast<UObject*>(Object), false, true, true, false);
	RefFinder.FindReferences(const_cast<UObject*>(Object));
	
	for (UObject* RefObj : ReferencedObjects)
	{
		if (RefObj && RefObj != Object)
		{
			FReferenceInfo Info;
			Info.ObjectPath = Object->GetPathName();
			Info.ReferencerPath = RefObj->GetPathName();
			Info.ReferenceType = TEXT("Outgoing");
			Info.bIsHardReference = true;
			Result.Add(Info);
		}
	}
	
	return Result;
}

TArray<FReferenceInfo> FObjectProfilerCore::GetReferenceChain(const UObject* Object)
{
	TArray<FReferenceInfo> Result;
	
	if (!Object)
	{
		return Result;
	}

	const UObject* Current = Object;
	TSet<const UObject*> Visited;
	int32 Depth = 0;
	constexpr int32 MaxDepth = 10;
	
	while (Current && Depth < MaxDepth)
	{
		if (Visited.Contains(Current))
		{
			break;
		}
		Visited.Add(Current);
		
		UObject* Outer = Current->GetOuter();
		if (Outer)
		{
			FReferenceInfo Info;
			Info.ObjectPath = Current->GetPathName();
			Info.ReferencerPath = Outer->GetPathName();
			Info.ReferenceType = TEXT("Outer");
			Info.bIsHardReference = true;
			Result.Add(Info);
		}
		
		Current = Outer;
		Depth++;
	}
	
	return Result;
}

TMap<EObjectCategory, int64> FObjectProfilerCore::GetMemoryBreakdownByCategory()
{
	TMap<EObjectCategory, int64> Result;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (!IsValid(Obj) || Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		if (IsSafeForResourceSizeQuery(Obj))
		{
			const EObjectCategory Category = CategorizeClass(Obj->GetClass());
			Result.FindOrAdd(Category) += Obj->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
		}
	}

	return Result;
}

TMap<FString, int64> FObjectProfilerCore::GetMemoryBreakdownByModule()
{
	TMap<FString, int64> Result;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (!IsValid(Obj) || Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		if (IsSafeForResourceSizeQuery(Obj))
		{
			const FString ModuleName = GetModuleName(Obj->GetClass());
			Result.FindOrAdd(ModuleName) += Obj->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
		}
	}

	return Result;
}

void FObjectProfilerCore::SetLeakDetectionThreshold(int32 ConsecutiveTicks)
{
	LeakDetectionThreshold = FMath::Max(2, ConsecutiveTicks);
}

int32 FObjectProfilerCore::GetLeakDetectionThreshold()
{
	return LeakDetectionThreshold;
}

TArray<TSharedPtr<FProfilerTreeItem>> FObjectProfilerCore::BuildTreeView(
	const TArray<TSharedPtr<FObjectClassStats>>& Stats,
	EProfilerGroupMode GroupMode)
{
	TArray<TSharedPtr<FProfilerTreeItem>> Result;
	
	if (GroupMode == EProfilerGroupMode::None)
	{
		for (const auto& StatItem : Stats)
		{
			TSharedPtr<FProfilerTreeItem> TreeItem = MakeShared<FProfilerTreeItem>();
			TreeItem->Type = FProfilerTreeItem::EItemType::Class;
			TreeItem->DisplayName = StatItem->ClassName;
			TreeItem->Stats = StatItem;
			Result.Add(TreeItem);
		}
		return Result;
	}
	
	TMap<FString, TSharedPtr<FProfilerTreeItem>> GroupMap;
	
	for (const auto& StatItem : Stats)
	{
		FString GroupName;
		FProfilerTreeItem::EItemType GroupType;
		
		if (GroupMode == EProfilerGroupMode::ByModule)
		{
			GroupName = StatItem->ModuleName;
			GroupType = FProfilerTreeItem::EItemType::Module;
		}
		else
		{
			static const TMap<EObjectCategory, FString> CategoryNames = {
				{EObjectCategory::Unknown, TEXT("Unknown")},
				{EObjectCategory::Actor, TEXT("Actors")},
				{EObjectCategory::Component, TEXT("Components")},
				{EObjectCategory::Asset, TEXT("Assets")},
				{EObjectCategory::Widget, TEXT("Widgets")},
				{EObjectCategory::Animation, TEXT("Animation")},
				{EObjectCategory::Audio, TEXT("Audio")},
				{EObjectCategory::Material, TEXT("Materials")},
				{EObjectCategory::Texture, TEXT("Textures")},
				{EObjectCategory::Mesh, TEXT("Meshes")},
				{EObjectCategory::Blueprint, TEXT("Blueprints")},
				{EObjectCategory::DataAsset, TEXT("Data Assets")},
				{EObjectCategory::Subsystem, TEXT("Subsystems")},
				{EObjectCategory::GameInstance, TEXT("Game Instance")},
				{EObjectCategory::Other, TEXT("Other")}
			};
			
			GroupName = CategoryNames.FindRef(StatItem->Category);
			GroupType = FProfilerTreeItem::EItemType::Category;
		}
		
		TSharedPtr<FProfilerTreeItem>& GroupItem = GroupMap.FindOrAdd(GroupName);
		if (!GroupItem.IsValid())
		{
			GroupItem = MakeShared<FProfilerTreeItem>();
			GroupItem->Type = GroupType;
			GroupItem->DisplayName = GroupName;
		}
		
		TSharedPtr<FProfilerTreeItem> ClassItem = MakeShared<FProfilerTreeItem>();
		ClassItem->Type = FProfilerTreeItem::EItemType::Class;
		ClassItem->DisplayName = StatItem->ClassName;
		ClassItem->Stats = StatItem;
		ClassItem->Parent = GroupItem;
		
		GroupItem->Children.Add(ClassItem);
	}
	
	GroupMap.GenerateValueArray(Result);
	
	Result.Sort([](const TSharedPtr<FProfilerTreeItem>& A, const TSharedPtr<FProfilerTreeItem>& B)
	{
		return A->GetAggregatedInstanceCount() > B->GetAggregatedInstanceCount();
	});
	
	return Result;
}