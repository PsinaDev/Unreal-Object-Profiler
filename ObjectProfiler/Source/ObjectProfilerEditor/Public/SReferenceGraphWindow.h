//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "ObjectProfilerTypes.h"

struct FReferenceTreeItem : public TSharedFromThis<FReferenceTreeItem>
{
	FString ObjectPath;
	FString ObjectName;
	FString ReferenceType;
	bool bIsHardReference = true;
	bool bIsIncoming = true;
	TArray<TSharedPtr<FReferenceTreeItem>> Children;
	TWeakPtr<FReferenceTreeItem> Parent;
	bool bIsExpanded = false;
	int32 Depth = 0;
};

class SReferenceGraphWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReferenceGraphWindow) {}
		SLATE_ARGUMENT(TSharedPtr<FObjectClassStats>, ClassStats)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> OnGenerateRowForTree(TSharedPtr<FReferenceTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildrenForTree(TSharedPtr<FReferenceTreeItem> Item, TArray<TSharedPtr<FReferenceTreeItem>>& OutChildren);
	
	void RefreshReferences();
	void BuildReferenceTree(UObject* Object, TSharedPtr<FReferenceTreeItem> ParentItem, bool bIncoming, int32 MaxDepth);
	
	FReply OnRefreshClicked();
	void OnInstanceSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	
	TSharedPtr<STreeView<TSharedPtr<FReferenceTreeItem>>> ReferenceTreeView;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> InstanceComboBox;
	
	TArray<TSharedPtr<FReferenceTreeItem>> RootItems;
	TArray<TSharedPtr<FString>> InstanceList;
	TSharedPtr<FString> SelectedInstance;
	
	TSharedPtr<FObjectClassStats> ClassStats;
	int32 MaxReferenceDepth = 3;
};

class SReferenceTreeRow : public SMultiColumnTableRow<TSharedPtr<FReferenceTreeItem>>
{
public:
	SLATE_BEGIN_ARGS(SReferenceTreeRow) {}
		SLATE_ARGUMENT(TSharedPtr<FReferenceTreeItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FReferenceTreeItem> Item;
};