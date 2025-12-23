//Copyright PsinaDev 2025.

#include "SSnapshotManager.h"
#include "ObjectProfilerCore.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "ObjectProfiler"

namespace SnapshotColumns
{
	static const FName Name("Name");
	static const FName Timestamp("Timestamp");
	static const FName ObjectCount("ObjectCount");
	static const FName TotalSize("TotalSize");
}

namespace SnapshotLayoutConstants
{
	constexpr float ButtonMinWidth = 65.0f;
	constexpr float NameInputMinWidth = 100.0f;
	constexpr float ControlPadding = 2.0f;
	constexpr float SectionPadding = 4.0f;
}

void SSnapshotManager::Construct(const FArguments& InArgs)
{
	OnCompareSnapshotsDelegate = InArgs._OnCompareSnapshots;
	OnViewDeltaDelegate = InArgs._OnViewDelta;

	TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(SnapshotColumns::Name)
			.DefaultLabel(LOCTEXT("NameColumn", "Name"))
			.FillWidth(0.3f)
		+ SHeaderRow::Column(SnapshotColumns::Timestamp)
			.DefaultLabel(LOCTEXT("TimestampColumn", "Time"))
			.FillWidth(0.25f)
		+ SHeaderRow::Column(SnapshotColumns::ObjectCount)
			.DefaultLabel(LOCTEXT("ObjectCountColumn", "Objects"))
			.FillWidth(0.2f)
		+ SHeaderRow::Column(SnapshotColumns::TotalSize)
			.DefaultLabel(LOCTEXT("TotalSizeColumn", "Size"))
			.FillWidth(0.25f);

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(200.0f)
		.Clipping(EWidgetClipping::ClipToBoundsAlways)
		[
			SNew(SVerticalBox)
			
			// Take snapshot row
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(SnapshotLayoutConstants::SectionPadding)
			[
				SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::NameInputMinWidth)
						[
							SAssignNew(SnapshotNameBox, SEditableTextBox)
							.HintText(LOCTEXT("SnapshotNameHint", "Snapshot name"))
							.OnTextCommitted(this, &SSnapshotManager::OnSnapshotNameCommitted)
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("TakeSnapshot", "Snapshot"))
							.ToolTipText(LOCTEXT("TakeSnapshotTooltip", "Take a new snapshot of current object counts"))
							.OnClicked(this, &SSnapshotManager::OnTakeSnapshotClicked)
						]
					]
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			
			// List view
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(SnapshotLayoutConstants::SectionPadding)
			[
				SAssignNew(SnapshotListView, SListView<TSharedPtr<FObjectSnapshot>>)
				.ListItemsSource(&SnapshotItems)
				.OnGenerateRow(this, &SSnapshotManager::OnGenerateRowForList)
				.OnSelectionChanged(this, &SSnapshotManager::OnSelectionChanged)
				.HeaderRow(HeaderRow)
				.SelectionMode(ESelectionMode::Multi)
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			
			// Action buttons row 1
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(SnapshotLayoutConstants::SectionPadding)
			[
				SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("ViewDelta", "Delta"))
							.ToolTipText(LOCTEXT("ViewDeltaTooltip", "View difference between selected snapshot and current state"))
							.OnClicked(this, &SSnapshotManager::OnViewDeltaClicked)
							.IsEnabled_Lambda([this]() { return SelectedSnapshots.Num() == 1; })
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("Compare", "Compare"))
							.ToolTipText(LOCTEXT("CompareTooltip", "Compare two selected snapshots"))
							.OnClicked(this, &SSnapshotManager::OnCompareClicked)
							.IsEnabled_Lambda([this]() { return SelectedSnapshots.Num() == 2; })
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("Delete", "Delete"))
							.ToolTipText(LOCTEXT("DeleteTooltip", "Delete selected snapshot"))
							.OnClicked(this, &SSnapshotManager::OnDeleteSnapshotClicked)
							.IsEnabled_Lambda([this]() { return SelectedSnapshots.Num() > 0; })
						]
					]
					
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNullWidget::NullWidget
					]
				]
			]
			
			// Action buttons row 2
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(SnapshotLayoutConstants::SectionPadding, 0.0f, SnapshotLayoutConstants::SectionPadding, SnapshotLayoutConstants::SectionPadding)
			[
				SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("Save", "Save"))
							.ToolTipText(LOCTEXT("SaveTooltip", "Save all snapshots to file"))
							.OnClicked(this, &SSnapshotManager::OnSaveClicked)
							.IsEnabled_Lambda([this]() { return SnapshotItems.Num() > 0; })
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("Load", "Load"))
							.ToolTipText(LOCTEXT("LoadTooltip", "Load snapshots from file"))
							.OnClicked(this, &SSnapshotManager::OnLoadClicked)
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(SnapshotLayoutConstants::ControlPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(SnapshotLayoutConstants::ButtonMinWidth)
						[
							SNew(SButton)
							.Text(LOCTEXT("ClearAll", "Clear"))
							.ToolTipText(LOCTEXT("ClearAllTooltip", "Delete all snapshots"))
							.OnClicked(this, &SSnapshotManager::OnClearAllClicked)
							.IsEnabled_Lambda([this]() { return SnapshotItems.Num() > 0; })
						]
					]
					
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNullWidget::NullWidget
					]
				]
			]
		]
	];
	
	RefreshList();
	
	SnapshotTakenHandle = FObjectProfilerCore::OnSnapshotTaken().AddSP(this, &SSnapshotManager::OnSnapshotTakenCallback);
}

SSnapshotManager::~SSnapshotManager()
{
	if (SnapshotTakenHandle.IsValid())
	{
		FObjectProfilerCore::OnSnapshotTaken().Remove(SnapshotTakenHandle);
	}
}

void SSnapshotManager::OnSnapshotTakenCallback(const FObjectSnapshot& Snapshot)
{
	RefreshList();
}

void SSnapshotManager::RefreshList()
{
	SnapshotItems.Empty();
	
	const TArray<FObjectSnapshot>& History = FObjectProfilerCore::GetSnapshotHistory();
	for (const FObjectSnapshot& Snapshot : History)
	{
		SnapshotItems.Add(MakeShared<FObjectSnapshot>(Snapshot));
	}
	
	if (SnapshotListView.IsValid())
	{
		SnapshotListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SSnapshotManager::OnGenerateRowForList(TSharedPtr<FObjectSnapshot> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSnapshotListRow, OwnerTable)
		.Item(Item);
}

void SSnapshotManager::OnSelectionChanged(TSharedPtr<FObjectSnapshot> Item, ESelectInfo::Type SelectInfo)
{
	SelectedSnapshots = SnapshotListView->GetSelectedItems();
}

FReply SSnapshotManager::OnTakeSnapshotClicked()
{
	FObjectProfilerCore::TakeSnapshot(NewSnapshotName);
	NewSnapshotName.Empty();
	if (SnapshotNameBox.IsValid())
	{
		SnapshotNameBox->SetText(FText::GetEmpty());
	}
	RefreshList();
	return FReply::Handled();
}

FReply SSnapshotManager::OnDeleteSnapshotClicked()
{
	const TArray<FObjectSnapshot>& History = FObjectProfilerCore::GetSnapshotHistory();
	
	TArray<int32> IndicesToDelete;
	for (const auto& Selected : SelectedSnapshots)
	{
		for (int32 i = 0; i < History.Num(); ++i)
		{
			if (History[i].Timestamp == Selected->Timestamp && History[i].Name == Selected->Name)
			{
				IndicesToDelete.Add(i);
				break;
			}
		}
	}
	
	IndicesToDelete.Sort([](int32 A, int32 B) { return A > B; });
	
	for (int32 Index : IndicesToDelete)
	{
		FObjectProfilerCore::DeleteSnapshot(Index);
	}
	
	SelectedSnapshots.Empty();
	RefreshList();
	return FReply::Handled();
}

FReply SSnapshotManager::OnClearAllClicked()
{
	FObjectProfilerCore::ClearSnapshotHistory();
	SelectedSnapshots.Empty();
	RefreshList();
	return FReply::Handled();
}

FReply SSnapshotManager::OnSaveClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Save Snapshots"),
			FPaths::ProjectSavedDir(),
			TEXT("ObjectProfilerSnapshots.json"),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFiles
		);

		if (bOpened && OutFiles.Num() > 0)
		{
			FObjectProfilerCore::SaveSnapshotsToFile(OutFiles[0]);
		}
	}
	return FReply::Handled();
}

FReply SSnapshotManager::OnLoadClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Load Snapshots"),
			FPaths::ProjectSavedDir(),
			TEXT(""),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OutFiles
		);

		if (bOpened && OutFiles.Num() > 0)
		{
			FObjectProfilerCore::LoadSnapshotsFromFile(OutFiles[0]);
			RefreshList();
		}
	}
	return FReply::Handled();
}

FReply SSnapshotManager::OnCompareClicked()
{
	if (SelectedSnapshots.Num() != 2)
	{
		return FReply::Handled();
	}
	
	const TArray<FObjectSnapshot>& History = FObjectProfilerCore::GetSnapshotHistory();
	int32 IndexA = INDEX_NONE;
	int32 IndexB = INDEX_NONE;
	
	for (int32 i = 0; i < History.Num(); ++i)
	{
		if (History[i].Timestamp == SelectedSnapshots[0]->Timestamp)
		{
			IndexA = i;
		}
		if (History[i].Timestamp == SelectedSnapshots[1]->Timestamp)
		{
			IndexB = i;
		}
	}
	
	if (IndexA != INDEX_NONE && IndexB != INDEX_NONE && OnCompareSnapshotsDelegate.IsBound())
	{
		OnCompareSnapshotsDelegate.Execute(IndexA, IndexB);
	}
	
	return FReply::Handled();
}

FReply SSnapshotManager::OnViewDeltaClicked()
{
	if (SelectedSnapshots.Num() != 1)
	{
		return FReply::Handled();
	}
	
	const TArray<FObjectSnapshot>& History = FObjectProfilerCore::GetSnapshotHistory();
	
	for (int32 i = 0; i < History.Num(); ++i)
	{
		if (History[i].Timestamp == SelectedSnapshots[0]->Timestamp)
		{
			if (OnViewDeltaDelegate.IsBound())
			{
				OnViewDeltaDelegate.Execute(i);
			}
			break;
		}
	}
	
	return FReply::Handled();
}

void SSnapshotManager::OnSnapshotNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	NewSnapshotName = NewText.ToString();
}

void SSnapshotListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FObjectSnapshot>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SSnapshotListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == SnapshotColumns::Name)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Name))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == SnapshotColumns::Timestamp)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Timestamp.ToString(TEXT("%H:%M:%S"))))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == SnapshotColumns::ObjectCount)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Item->TotalObjects))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == SnapshotColumns::TotalSize)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FObjectProfilerCore::FormatBytes(Item->TotalSize)))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE