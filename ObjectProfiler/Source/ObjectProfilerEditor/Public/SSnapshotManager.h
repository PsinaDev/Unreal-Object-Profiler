//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "ObjectProfilerTypes.h"

DECLARE_DELEGATE_TwoParams(FOnCompareSnapshots, int32, int32);
DECLARE_DELEGATE_OneParam(FOnViewDelta, int32);

class SSnapshotManager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSnapshotManager) {}
	SLATE_EVENT(FOnCompareSnapshots, OnCompareSnapshots)
	SLATE_EVENT(FOnViewDelta, OnViewDelta)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);
	virtual ~SSnapshotManager();
	
	void RefreshList();

private:
	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FObjectSnapshot> Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	FReply OnTakeSnapshotClicked();
	FReply OnDeleteSnapshotClicked();
	FReply OnClearAllClicked();
	FReply OnSaveClicked();
	FReply OnLoadClicked();
	FReply OnCompareClicked();
	FReply OnViewDeltaClicked();
	
	void OnSnapshotNameCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnSelectionChanged(TSharedPtr<FObjectSnapshot> Item, ESelectInfo::Type SelectInfo);
	void OnSnapshotTakenCallback(const FObjectSnapshot& Snapshot);
	
	TSharedPtr<SListView<TSharedPtr<FObjectSnapshot>>> SnapshotListView;
	TSharedPtr<SEditableTextBox> SnapshotNameBox;
	
	TArray<TSharedPtr<FObjectSnapshot>> SnapshotItems;
	TArray<TSharedPtr<FObjectSnapshot>> SelectedSnapshots;
	FString NewSnapshotName;
	
	FOnCompareSnapshots OnCompareSnapshotsDelegate;
	FOnViewDelta OnViewDeltaDelegate;
	
	FDelegateHandle SnapshotTakenHandle;
};

class SSnapshotListRow : public SMultiColumnTableRow<TSharedPtr<FObjectSnapshot>>
{
public:
	SLATE_BEGIN_ARGS(SSnapshotListRow) {}
	SLATE_ARGUMENT(TSharedPtr<FObjectSnapshot>, Item)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FObjectSnapshot> Item;
};