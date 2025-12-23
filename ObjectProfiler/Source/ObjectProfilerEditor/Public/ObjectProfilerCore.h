//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/UObjectArray.h"
#include "ObjectProfilerTypes.h"

DECLARE_DELEGATE_OneParam(FOnObjectStatsCollected, TArray<TSharedPtr<FObjectClassStats>>);
DECLARE_DELEGATE_OneParam(FOnCollectionProgress, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRealTimeUpdate, const TArray<TSharedPtr<FObjectClassStats>>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSnapshotTaken, const FObjectSnapshot&);

class OBJECTPROFILEREDITOR_API FObjectProfilerCore
{
public:
	static TArray<TSharedPtr<FObjectClassStats>> CollectObjectStats(int32 MaxSamplesPerClass = 5);
	
	static void CollectObjectStatsAsync(
		int32 MaxSamplesPerClass,
		FOnObjectStatsCollected OnComplete,
		FOnCollectionProgress OnProgress = FOnCollectionProgress());
	
	static void CancelAsyncCollection();
	static bool IsAsyncCollectionInProgress();
	
	static TArray<FString> GetInstancesOfClass(const UClass* InClass);
	static TArray<FString> GetInstancesOfClass(const FString& ClassName);
	
	static void TakeSnapshot(const FString& SnapshotName = TEXT(""));
	static TArray<TSharedPtr<FObjectClassStats>> GetDeltaSinceSnapshot(int32 SnapshotIndex = -1);
	static TArray<TSharedPtr<FObjectClassStats>> CompareTwoSnapshots(int32 IndexA, int32 IndexB);
	
	static const TArray<FObjectSnapshot>& GetSnapshotHistory();
	static void ClearSnapshotHistory();
	static void DeleteSnapshot(int32 Index);
	static bool SaveSnapshotsToFile(const FString& FilePath);
	static bool LoadSnapshotsFromFile(const FString& FilePath);
	
	static void ForceGarbageCollection();
	static int32 GetTotalObjectCount();
	
	static FString FormatBytes(int64 Bytes);
	static EObjectCategory CategorizeClass(const UClass* InClass);
	static EObjectSource GetObjectSource(const UClass* InClass);
	static FString GetModuleName(const UClass* InClass);
	static FString GetSourceDisplayName(EObjectSource Source);
	
	static void StartRealTimeMonitoring(float IntervalSeconds);
	static void StopRealTimeMonitoring();
	static bool IsRealTimeMonitoringActive();
	static void SetRealTimeInterval(float IntervalSeconds);
	static float GetRealTimeInterval();
	
	static FOnRealTimeUpdate& OnRealTimeUpdate();
	static FOnSnapshotTaken& OnSnapshotTaken();
	
	static TArray<FReferenceInfo> GetReferencesTo(const UObject* Object);
	static TArray<FReferenceInfo> GetReferencesFrom(const UObject* Object);
	static TArray<FReferenceInfo> GetReferenceChain(const UObject* Object);
	
	static TMap<EObjectCategory, int64> GetMemoryBreakdownByCategory();
	static TMap<FString, int64> GetMemoryBreakdownByModule();
	
	static void SetLeakDetectionThreshold(int32 ConsecutiveTicks);
	static int32 GetLeakDetectionThreshold();
	
	static void StartIncrementalTracking();
	static void StopIncrementalTracking();
	static bool IsIncrementalTrackingActive();
	static TArray<TSharedPtr<FObjectClassStats>> GetIncrementalStats();
	
	static TArray<TSharedPtr<FProfilerTreeItem>> BuildTreeView(
		const TArray<TSharedPtr<FObjectClassStats>>& Stats,
		EProfilerGroupMode GroupMode);

private:
	struct FIncrementalClassData
	{
		int32 Count = 0;
		EObjectCategory CachedCategory = EObjectCategory::Unknown;
		EObjectSource CachedSource = EObjectSource::Unknown;
		FString CachedModuleName;
		bool bDirty = true;
	};
	
	class FProfilerCreateListener : public FUObjectArray::FUObjectCreateListener
	{
	public:
		virtual void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;
		virtual void OnUObjectArrayShutdown() override;
	};
	
	class FProfilerDeleteListener : public FUObjectArray::FUObjectDeleteListener
	{
	public:
		virtual void NotifyUObjectDeleted(const UObjectBase* Object, int32 Index) override;
		virtual void OnUObjectArrayShutdown() override;
	};

	enum class EAsyncPhase : uint8
	{
		Idle,
		Enumerating,
		Processing,
		Complete
	};

	struct FAsyncCollectionState
	{
		EAsyncPhase Phase = EAsyncPhase::Idle;
		
		TArray<TWeakObjectPtr<UObject>> ObjectsToProcess;
		TMap<UClass*, TSharedPtr<FObjectClassStats>> StatsMap;
		
		int32 EnumerationIndex = 0;
		int32 ProcessingIndex = 0;
		int32 TotalObjectsEstimate = 0;
		int32 MaxSamplesPerClass = 5;
		
		FOnObjectStatsCollected OnComplete;
		FOnCollectionProgress OnProgress;
		
		FTSTicker::FDelegateHandle TickerHandle;
		
		void Reset()
		{
			Phase = EAsyncPhase::Idle;
			ObjectsToProcess.Empty();
			StatsMap.Empty();
			EnumerationIndex = 0;
			ProcessingIndex = 0;
			TotalObjectsEstimate = 0;
			OnComplete.Unbind();
			OnProgress.Unbind();
		}
	};

	static bool OnAsyncTick(float DeltaTime);
	static void TickEnumeration();
	static void TickProcessing();
	static void FinalizeCollection();
	
	static bool OnRealTimeTick(float DeltaTime);
	static void UpdateRealTimeStats();
	static void OnRealTimeCollectionComplete(TArray<TSharedPtr<FObjectClassStats>> Results);
	
	static bool IsSafeForResourceSizeQuery(const UObject* Obj);
	
	static TArray<FObjectSnapshot> SnapshotHistory;
	static TMap<FString, TSharedPtr<FObjectClassStats>> PreviousStats;
	static FAsyncCollectionState AsyncState;
	static bool bCancellationRequested;
	
	static FTSTicker::FDelegateHandle RealTimeTickerHandle;
	static float RealTimeIntervalSeconds;
	static bool bRealTimeMonitoringActive;
	static double LastRealTimeUpdateTime;
	
	static int32 LeakDetectionThreshold;
	
	static FOnRealTimeUpdate RealTimeUpdateDelegate;
	static FOnSnapshotTaken SnapshotTakenDelegate;
	
	static FCriticalSection IncrementalDataLock;
	static TMap<UClass*, FIncrementalClassData> IncrementalClassCounts;
	static bool bIncrementalTrackingActive;
	static FProfilerCreateListener* CreateListener;
	static FProfilerDeleteListener* DeleteListener;
	
	static constexpr int32 EnumerationChunkSize = 1000;
	static constexpr int32 ProcessingChunkSize = 200;
	static constexpr int32 MaxSnapshotHistory = 50;
};