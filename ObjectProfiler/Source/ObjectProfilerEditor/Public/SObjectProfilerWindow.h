//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "ObjectProfilerCore.h"
#include "ObjectProfilerTypes.h"

class SSparkline;

class SObjectProfilerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SObjectProfilerWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SObjectProfilerWindow() override;

private:
	TSharedRef<ITableRow> OnGenerateRowForTree(TSharedPtr<FProfilerTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildrenForTree(TSharedPtr<FProfilerTreeItem> Item, TArray<TSharedPtr<FProfilerTreeItem>>& OutChildren);
	
	void OnSortColumnHeader(EColumnSortPriority::Type SortPriority, const FName& ColumnId, EColumnSortMode::Type NewSortMode);
	EColumnSortMode::Type GetSortModeForColumn(FName ColumnId) const;
	void SortData();

	FReply OnRefreshClicked();
	FReply OnCancelClicked();
	FReply OnForceGCClicked();
	FReply OnExportClicked();
	FReply OnFindInContentBrowserClicked();
	FReply OnShowReferencesClicked();

	void OnFilterTextChanged(const FText& NewText);
	void OnTreeDoubleClick(TSharedPtr<FProfilerTreeItem> Item);
	void OnTreeSelectionChanged(TSharedPtr<FProfilerTreeItem> Item, ESelectInfo::Type SelectInfo);
	
	void OnViewModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo);
	void OnGroupModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo);
	void OnIntervalChanged(TSharedPtr<FString> NewInterval, ESelectInfo::Type SelectInfo);
	void OnCategoryFilterChanged(TSharedPtr<FString> NewCategory, ESelectInfo::Type SelectInfo);
	void OnSizeFilterChanged(TSharedPtr<FString> NewSize, ESelectInfo::Type SelectInfo);
	
	void OnShowOnlyLeakingChanged(ECheckBoxState NewState);
	void OnShowOnlyHotChanged(ECheckBoxState NewState);

	void RefreshDataAsync();
	void OnAsyncCollectionComplete(TArray<TSharedPtr<FObjectClassStats>> Results);
	void OnAsyncCollectionProgress(float Progress);
	void OnRealTimeUpdate(const TArray<TSharedPtr<FObjectClassStats>>& Results);
	
	void OnCompareSnapshots(int32 IndexA, int32 IndexB);
	void OnViewDelta(int32 SnapshotIndex);
	
	void ApplyFilter();
	void RebuildTreeView();
	void UpdateStatusBar();
	
	EVisibility GetProgressBarVisibility() const;
	EVisibility GetCancelButtonVisibility() const;
	TOptional<float> GetProgressBarPercent() const;
	bool IsNotLoading() const;
	bool CanFindInContentBrowser() const;
	bool HasSelection() const;

	TSharedRef<SWidget> GenerateViewModeComboContent(TSharedPtr<FString> Item);
	TSharedRef<SWidget> GenerateGroupModeComboContent(TSharedPtr<FString> Item);
	TSharedRef<SWidget> GenerateIntervalComboContent(TSharedPtr<FString> Item);
	TSharedRef<SWidget> GenerateCategoryComboContent(TSharedPtr<FString> Item);
	TSharedRef<SWidget> GenerateSizeFilterComboContent(TSharedPtr<FString> Item);
	TSharedRef<SWidget> GenerateSourceComboContent(TSharedPtr<FString> Item);
	
	FText GetViewModeText() const;
	FText GetGroupModeText() const;
	FText GetIntervalText() const;
	FText GetCategoryText() const;
	FText GetSizeFilterText() const;
	FText GetSourceText() const;
	
	void OnSourceFilterChanged(TSharedPtr<FString> NewSource, ESelectInfo::Type SelectInfo);

private:
	TSharedPtr<STreeView<TSharedPtr<FProfilerTreeItem>>> TreeView;
	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SEditableTextBox> FilterBox;
	TSharedPtr<SProgressBar> ProgressBar;
	
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ViewModeCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> GroupModeCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> IntervalCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> CategoryFilterCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SizeFilterCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SourceFilterCombo;

	TArray<TSharedPtr<FObjectClassStats>> AllStats;
	TArray<TSharedPtr<FObjectClassStats>> FilteredStats;
	TArray<TSharedPtr<FProfilerTreeItem>> TreeItems;
	TSharedPtr<FProfilerTreeItem> SelectedTreeItem;

	TArray<TSharedPtr<FString>> ViewModeOptions;
	TArray<TSharedPtr<FString>> GroupModeOptions;
	TArray<TSharedPtr<FString>> IntervalOptions;
	TArray<TSharedPtr<FString>> CategoryOptions;
	TArray<TSharedPtr<FString>> SizeFilterOptions;
	TArray<TSharedPtr<FString>> SourceOptions;
	
	TSharedPtr<FString> CurrentViewMode;
	TSharedPtr<FString> CurrentGroupMode;
	TSharedPtr<FString> CurrentInterval;
	TSharedPtr<FString> CurrentCategory;
	TSharedPtr<FString> CurrentSizeFilter;
	TSharedPtr<FString> CurrentSource;

	FName CurrentSortColumn = "InstanceCount";
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Descending;
	
	FProfilerFilterSettings FilterSettings;
	EProfilerViewMode ViewMode = EProfilerViewMode::Normal;
	EProfilerGroupMode GroupMode = EProfilerGroupMode::None;
	
	float CurrentProgress = 0.0f;
	int32 CurrentSnapshotIndex = -1;
	
	FDelegateHandle RealTimeUpdateHandle;
};

class SObjectProfilerTreeRow : public SMultiColumnTableRow<TSharedPtr<FProfilerTreeItem>>
{
public:
	SLATE_BEGIN_ARGS(SObjectProfilerTreeRow) {}
		SLATE_ARGUMENT(TSharedPtr<FProfilerTreeItem>, Item)
		SLATE_ARGUMENT(EProfilerViewMode, ViewMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FProfilerTreeItem> Item;
	EProfilerViewMode ViewMode = EProfilerViewMode::Normal;
	TSharedPtr<SSparkline> Sparkline;
};