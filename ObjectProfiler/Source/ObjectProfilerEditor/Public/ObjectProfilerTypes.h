//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

UENUM()
enum class EObjectCategory : uint8
{
	Unknown,
	Actor,
	Component,
	Asset,
	Widget,
	Animation,
	Audio,
	Material,
	Texture,
	Mesh,
	Blueprint,
	DataAsset,
	Subsystem,
	GameInstance,
	Other
};

UENUM()
enum class EObjectSource : uint8
{
	Unknown,
	EngineCore,
	EngineEditor,
	EngineRuntime,
	BlueprintVM,
	GameCode,
	GameContent,
	Plugin,
	ThirdParty
};

UENUM()
enum class EProfilerViewMode : uint8
{
	Normal,
	Delta,
	RealTime
};

UENUM()
enum class EProfilerGroupMode : uint8
{
	None,
	ByModule,
	ByCategory
};

UENUM()
enum class ESizeFilterMode : uint8
{
	None,
	LessThan1KB,
	Between1KBAnd1MB,
	GreaterThan1MB,
	GreaterThan10MB,
	GreaterThan100MB
};

struct FObjectHistoryPoint
{
	int32 InstanceCount = 0;
	int64 TotalSizeBytes = 0;
	double Timestamp = 0.0;
};

struct FObjectClassStats : public TSharedFromThis<FObjectClassStats>
{
	FString ClassName;
	FName ClassFName;
	FString ModuleName;
	int32 InstanceCount = 0;
	int32 DeltaCount = 0;
	int64 TotalSizeBytes = 0;
	int64 AverageSizeBytes = 0;
	int64 DeltaSizeBytes = 0;
	float RatePerSecond = 0.0f;
	TArray<FString> SampleObjectNames;
	TWeakObjectPtr<UClass> ClassPtr;
	EObjectCategory Category = EObjectCategory::Unknown;
	EObjectSource Source = EObjectSource::Unknown;
	bool bSizeAvailable = true;
	bool bIsLeaking = false;
	bool bIsHot = false;
	int32 ConsecutiveGrowthTicks = 0;
	
	TArray<FObjectHistoryPoint> History;
	static constexpr int32 MaxHistorySize = 60;
	
	UClass* GetClass() const
	{
		return ClassPtr.Get();
	}
	
	bool IsClassValid() const
	{
		return ClassPtr.IsValid();
	}
	
	void AddHistoryPoint(int32 InCount, int64 InSize, double InTimestamp)
	{
		FObjectHistoryPoint Point;
		Point.InstanceCount = InCount;
		Point.TotalSizeBytes = InSize;
		Point.Timestamp = InTimestamp;
		
		History.Add(Point);
		if (History.Num() > MaxHistorySize)
		{
			History.RemoveAt(0);
		}
	}
	
	void UpdateLeakDetection(int32 ThresholdTicks = 5)
	{
		if (History.Num() < 2)
		{
			bIsLeaking = false;
			return;
		}
		
		int32 GrowthCount = 0;
		for (int32 i = 1; i < History.Num(); ++i)
		{
			if (History[i].InstanceCount > History[i - 1].InstanceCount)
			{
				GrowthCount++;
			}
			else
			{
				GrowthCount = 0;
			}
		}
		
		ConsecutiveGrowthTicks = GrowthCount;
		bIsLeaking = GrowthCount >= ThresholdTicks;
	}
	
	void CalculateRateOfChange()
	{
		if (History.Num() < 2)
		{
			RatePerSecond = 0.0f;
			return;
		}
		
		const FObjectHistoryPoint& Oldest = History[0];
		const FObjectHistoryPoint& Newest = History.Last();
		
		const double TimeDelta = Newest.Timestamp - Oldest.Timestamp;
		if (TimeDelta > 0.0)
		{
			RatePerSecond = static_cast<float>(Newest.InstanceCount - Oldest.InstanceCount) / static_cast<float>(TimeDelta);
		}
		else
		{
			RatePerSecond = 0.0f;
		}
		
		bIsHot = FMath::Abs(RatePerSecond) > 10.0f;
	}

	bool operator==(const FObjectClassStats& Other) const
	{
		return ClassName == Other.ClassName;
	}
};

struct FObjectSnapshot
{
	FString Name;
	FDateTime Timestamp;
	TMap<FString, int32> ClassCounts;
	TMap<FString, int64> ClassSizes;
	int32 TotalObjects = 0;
	int64 TotalSize = 0;
	
	FString GetDisplayName() const
	{
		return FString::Printf(TEXT("%s (%s)"), *Name, *Timestamp.ToString(TEXT("%H:%M:%S")));
	}
};

struct FReferenceInfo
{
	FString ObjectPath;
	FString ReferencerPath;
	FString ReferenceType;
	bool bIsHardReference = true;
};

struct FProfilerFilterSettings
{
	FString TextFilter;
	ESizeFilterMode SizeFilter = ESizeFilterMode::None;
	EObjectCategory CategoryFilter = EObjectCategory::Unknown;
	EObjectSource SourceFilter = EObjectSource::Unknown;
	bool bShowOnlyLeaking = false;
	bool bShowOnlyHot = false;
	
	bool IsDefault() const
	{
		return TextFilter.IsEmpty() 
			&& SizeFilter == ESizeFilterMode::None 
			&& CategoryFilter == EObjectCategory::Unknown 
			&& SourceFilter == EObjectSource::Unknown
			&& !bShowOnlyLeaking 
			&& !bShowOnlyHot;
	}
};

struct FFilterPreset
{
	FString Name;
	FProfilerFilterSettings Settings;
};

struct FProfilerTreeItem : public TSharedFromThis<FProfilerTreeItem>
{
	enum class EItemType : uint8
	{
		Root,
		Module,
		Category,
		Class
	};
	
	EItemType Type = EItemType::Class;
	FString DisplayName;
	TSharedPtr<FObjectClassStats> Stats;
	TArray<TSharedPtr<FProfilerTreeItem>> Children;
	TWeakPtr<FProfilerTreeItem> Parent;
	bool bIsExpanded = false;
	
	int32 GetAggregatedInstanceCount() const
	{
		if (Stats.IsValid())
		{
			return Stats->InstanceCount;
		}
		
		int32 Total = 0;
		for (const auto& Child : Children)
		{
			Total += Child->GetAggregatedInstanceCount();
		}
		return Total;
	}
	
	int64 GetAggregatedSize() const
	{
		if (Stats.IsValid())
		{
			return Stats->TotalSizeBytes;
		}
		
		int64 Total = 0;
		for (const auto& Child : Children)
		{
			Total += Child->GetAggregatedSize();
		}
		return Total;
	}
	
	bool HasLeakingChildren() const
	{
		if (Stats.IsValid())
		{
			return Stats->bIsLeaking;
		}
		
		for (const auto& Child : Children)
		{
			if (Child->HasLeakingChildren())
			{
				return true;
			}
		}
		return false;
	}
	
	bool HasHotChildren() const
	{
		if (Stats.IsValid())
		{
			return Stats->bIsHot;
		}
		
		for (const auto& Child : Children)
		{
			if (Child->HasHotChildren())
			{
				return true;
			}
		}
		return false;
	}
};