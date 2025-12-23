//Copyright PsinaDev 2025.

#include "SReferenceGraphWindow.h"
#include "ObjectProfilerCore.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ObjectProfiler"

namespace ReferenceColumns
{
	static const FName ObjectName("ObjectName");
	static const FName ReferenceType("ReferenceType");
	static const FName ObjectPath("ObjectPath");
}

namespace ReferenceLayoutConstants
{
	constexpr float ComboBoxMinWidth = 150.0f;
	constexpr float ComboBoxMaxWidth = 400.0f;
	constexpr float DepthSpinBoxWidth = 60.0f;
	constexpr float ButtonMinWidth = 70.0f;
	constexpr float ControlPadding = 4.0f;
	constexpr float LabelPadding = 8.0f;
}

void SReferenceGraphWindow::Construct(const FArguments& InArgs)
{
	ClassStats = InArgs._ClassStats;
	
	if (ClassStats.IsValid())
	{
		for (const FString& Sample : ClassStats->SampleObjectNames)
		{
			InstanceList.Add(MakeShared<FString>(Sample));
		}
		
		if (ClassStats->GetClass())
		{
			TArray<FString> AllInstances = FObjectProfilerCore::GetInstancesOfClass(ClassStats->GetClass());
			for (const FString& Instance : AllInstances)
			{
				int32 PipeIndex;
				if (Instance.FindChar(TEXT('|'), PipeIndex))
				{
					FString CleanPath = Instance.Left(PipeIndex).TrimEnd();
					bool bAlreadyExists = false;
					for (const auto& Existing : InstanceList)
					{
						if (*Existing == CleanPath)
						{
							bAlreadyExists = true;
							break;
						}
					}
					if (!bAlreadyExists)
					{
						InstanceList.Add(MakeShared<FString>(CleanPath));
					}
				}
			}
		}
	}
	
	if (InstanceList.Num() > 0)
	{
		SelectedInstance = InstanceList[0];
	}

	TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(ReferenceColumns::ObjectName)
			.DefaultLabel(LOCTEXT("ObjectNameColumn", "Object"))
			.FillWidth(0.4f)
		+ SHeaderRow::Column(ReferenceColumns::ReferenceType)
			.DefaultLabel(LOCTEXT("ReferenceTypeColumn", "Type"))
			.FillWidth(0.2f)
		+ SHeaderRow::Column(ReferenceColumns::ObjectPath)
			.DefaultLabel(LOCTEXT("ObjectPathColumn", "Path"))
			.FillWidth(0.4f);

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(400.0f)
		.Clipping(EWidgetClipping::ClipToBoundsAlways)
		[
			SNew(SVerticalBox)
			
			// Toolbar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(ReferenceLayoutConstants::LabelPadding)
			[
				SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(SHorizontalBox)
					
					// Instance label
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, ReferenceLayoutConstants::ControlPadding, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectInstance", "Instance:"))
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
					]
					
					// Instance combo
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.MaxWidth(ReferenceLayoutConstants::ComboBoxMaxWidth)
					.Padding(0, 0, ReferenceLayoutConstants::LabelPadding, 0)
					[
						SNew(SBox)
						.MinDesiredWidth(ReferenceLayoutConstants::ComboBoxMinWidth)
						[
							SAssignNew(InstanceComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&InstanceList)
							.InitiallySelectedItem(SelectedInstance)
							.OnSelectionChanged(this, &SReferenceGraphWindow::OnInstanceSelected)
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
							{
								FString DisplayText = *Item;
								int32 LastDot;
								if (DisplayText.FindLastChar(TEXT('.'), LastDot))
								{
									DisplayText = DisplayText.RightChop(LastDot + 1);
								}
								return SNew(SBox)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
									[
										SNew(STextBlock)
										.Text(FText::FromString(DisplayText))
										.ToolTipText(FText::FromString(*Item))
										.Clipping(EWidgetClipping::ClipToBoundsAlways)
									];
							})
							.Content()
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									if (SelectedInstance.IsValid())
									{
										FString DisplayName = *SelectedInstance;
										int32 LastDot;
										if (DisplayName.FindLastChar(TEXT('.'), LastDot))
										{
											DisplayName = DisplayName.RightChop(LastDot + 1);
										}
										return FText::FromString(DisplayName);
									}
									return LOCTEXT("NoInstance", "Select instance...");
								})
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
							]
						]
					]
					
					// Depth label
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, ReferenceLayoutConstants::ControlPadding, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MaxDepth", "Depth:"))
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
					]
					
					// Depth spinbox
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, ReferenceLayoutConstants::LabelPadding, 0)
					[
						SNew(SBox)
						.MinDesiredWidth(ReferenceLayoutConstants::DepthSpinBoxWidth)
						.MaxDesiredWidth(ReferenceLayoutConstants::DepthSpinBoxWidth)
						[
							SNew(SSpinBox<int32>)
							.MinValue(1)
							.MaxValue(10)
							.Value(MaxReferenceDepth)
							.OnValueChanged_Lambda([this](int32 NewValue)
							{
								MaxReferenceDepth = NewValue;
							})
						]
					]
					
					// Refresh button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredWidth(ReferenceLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("Refresh", "Refresh"))
							.OnClicked(this, &SReferenceGraphWindow::OnRefreshClicked)
						]
					]
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			
			// Tree view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(ReferenceLayoutConstants::ControlPadding)
			[
				SAssignNew(ReferenceTreeView, STreeView<TSharedPtr<FReferenceTreeItem>>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SReferenceGraphWindow::OnGenerateRowForTree)
				.OnGetChildren(this, &SReferenceGraphWindow::OnGetChildrenForTree)
				.HeaderRow(HeaderRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]
	];
	
	RefreshReferences();
}

TSharedRef<ITableRow> SReferenceGraphWindow::OnGenerateRowForTree(TSharedPtr<FReferenceTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SReferenceTreeRow, OwnerTable)
		.Item(Item);
}

void SReferenceGraphWindow::OnGetChildrenForTree(TSharedPtr<FReferenceTreeItem> Item, TArray<TSharedPtr<FReferenceTreeItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SReferenceGraphWindow::RefreshReferences()
{
	RootItems.Empty();
	
	if (!SelectedInstance.IsValid() || SelectedInstance->IsEmpty())
	{
		ReferenceTreeView->RequestTreeRefresh();
		return;
	}
	
	UObject* TargetObject = FindObject<UObject>(nullptr, **SelectedInstance);
	if (!TargetObject)
	{
		TargetObject = LoadObject<UObject>(nullptr, **SelectedInstance);
	}
	
	if (!TargetObject)
	{
		ReferenceTreeView->RequestTreeRefresh();
		return;
	}
	
	TSharedPtr<FReferenceTreeItem> IncomingRoot = MakeShared<FReferenceTreeItem>();
	IncomingRoot->ObjectName = TEXT("Incoming References (Who references this)");
	IncomingRoot->ObjectPath = TEXT("");
	IncomingRoot->ReferenceType = TEXT("Group");
	IncomingRoot->bIsIncoming = true;
	IncomingRoot->Depth = 0;
	
	BuildReferenceTree(TargetObject, IncomingRoot, true, MaxReferenceDepth);
	
	if (IncomingRoot->Children.Num() > 0)
	{
		RootItems.Add(IncomingRoot);
	}
	
	TSharedPtr<FReferenceTreeItem> OutgoingRoot = MakeShared<FReferenceTreeItem>();
	OutgoingRoot->ObjectName = TEXT("Outgoing References (What this references)");
	OutgoingRoot->ObjectPath = TEXT("");
	OutgoingRoot->ReferenceType = TEXT("Group");
	OutgoingRoot->bIsIncoming = false;
	OutgoingRoot->Depth = 0;
	
	BuildReferenceTree(TargetObject, OutgoingRoot, false, MaxReferenceDepth);
	
	if (OutgoingRoot->Children.Num() > 0)
	{
		RootItems.Add(OutgoingRoot);
	}
	
	ReferenceTreeView->RequestTreeRefresh();
	
	for (const auto& RootItem : RootItems)
	{
		ReferenceTreeView->SetItemExpansion(RootItem, true);
	}
}

void SReferenceGraphWindow::BuildReferenceTree(UObject* Object, TSharedPtr<FReferenceTreeItem> ParentItem, bool bIncoming, int32 MaxDepth)
{
	if (!Object || ParentItem->Depth >= MaxDepth)
	{
		return;
	}
	
	TArray<FReferenceInfo> References;
	
	if (bIncoming)
	{
		References = FObjectProfilerCore::GetReferencesTo(Object);
	}
	else
	{
		References = FObjectProfilerCore::GetReferencesFrom(Object);
	}
	
	TSet<FString> AddedPaths;
	
	for (const FReferenceInfo& RefInfo : References)
	{
		FString RefPath = bIncoming ? RefInfo.ReferencerPath : RefInfo.ReferencerPath;
		
		if (AddedPaths.Contains(RefPath))
		{
			continue;
		}
		AddedPaths.Add(RefPath);
		
		TSharedPtr<FReferenceTreeItem> ChildItem = MakeShared<FReferenceTreeItem>();
		ChildItem->ObjectPath = RefPath;
		
		int32 LastDot;
		if (RefPath.FindLastChar(TEXT('.'), LastDot))
		{
			ChildItem->ObjectName = RefPath.RightChop(LastDot + 1);
		}
		else
		{
			ChildItem->ObjectName = RefPath;
		}
		
		ChildItem->ReferenceType = RefInfo.ReferenceType;
		ChildItem->bIsHardReference = RefInfo.bIsHardReference;
		ChildItem->bIsIncoming = bIncoming;
		ChildItem->Parent = ParentItem;
		ChildItem->Depth = ParentItem->Depth + 1;
		
		ParentItem->Children.Add(ChildItem);
	}
}

FReply SReferenceGraphWindow::OnRefreshClicked()
{
	RefreshReferences();
	return FReply::Handled();
}

void SReferenceGraphWindow::OnInstanceSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	SelectedInstance = Item;
	RefreshReferences();
}

void SReferenceTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FReferenceTreeItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SReferenceTreeRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == ReferenceColumns::ObjectName)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(4.0f * Item->Depth, 2.0f, 4.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->ObjectName))
					.ColorAndOpacity(Item->bIsHardReference ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
				]
			];
	}
	else if (ColumnName == ReferenceColumns::ReferenceType)
	{
		FLinearColor TypeColor = FLinearColor::White;
		if (Item->ReferenceType == TEXT("Internal"))
		{
			TypeColor = FLinearColor(0.3f, 0.8f, 0.3f);
		}
		else if (Item->ReferenceType == TEXT("External"))
		{
			TypeColor = FLinearColor(0.8f, 0.6f, 0.3f);
		}
		else if (Item->ReferenceType == TEXT("Outgoing"))
		{
			TypeColor = FLinearColor(0.3f, 0.6f, 0.8f);
		}
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ReferenceType))
				.ColorAndOpacity(FSlateColor(TypeColor))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ReferenceColumns::ObjectPath)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ObjectPath))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE