//Copyright PsinaDev 2025.

#include "SObjectProfilerWindow.h"
#include "SSparkline.h"
#include "SSnapshotManager.h"
#include "SReferenceGraphWindow.h"
#include "ObjectProfilerCore.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/FileHelper.h"
#include "DesktopPlatformModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#define LOCTEXT_NAMESPACE "ObjectProfiler"

namespace ObjectProfilerColumns
{
	static const FName ClassName("ClassName");
	static const FName InstanceCount("InstanceCount");
	static const FName TotalSize("TotalSize");
	static const FName AvgSize("AvgSize");
	static const FName Delta("Delta");
	static const FName Rate("Rate");
	static const FName History("History");
	static const FName Module("Module");
	static const FName Category("Category");
	static const FName Source("Source");
}

namespace ProfilerLayoutConstants
{
	constexpr float ComboBoxMinWidth = 80.0f;
	constexpr float ComboBoxMaxWidth = 200.0f;
	constexpr float FilterBoxMinWidth = 100.0f;
	constexpr float FilterBoxMaxWidth = 250.0f;
	constexpr float ButtonMinWidth = 70.0f;
	constexpr float LeftPanelMinSize = 500.0f;
	constexpr float RightPanelMinSize = 250.0f;
	constexpr float LabelPadding = 4.0f;
	constexpr float ControlPadding = 2.0f;
}

void SObjectProfilerWindow::Construct(const FArguments& InArgs)
{
	ViewModeOptions.Add(MakeShared<FString>(TEXT("Normal")));
	ViewModeOptions.Add(MakeShared<FString>(TEXT("Delta")));
	ViewModeOptions.Add(MakeShared<FString>(TEXT("Real-Time")));
	CurrentViewMode = ViewModeOptions[0];
	
	GroupModeOptions.Add(MakeShared<FString>(TEXT("None")));
	GroupModeOptions.Add(MakeShared<FString>(TEXT("By Module")));
	GroupModeOptions.Add(MakeShared<FString>(TEXT("By Category")));
	CurrentGroupMode = GroupModeOptions[0];
	
	IntervalOptions.Add(MakeShared<FString>(TEXT("0.5s")));
	IntervalOptions.Add(MakeShared<FString>(TEXT("1s")));
	IntervalOptions.Add(MakeShared<FString>(TEXT("2s")));
	IntervalOptions.Add(MakeShared<FString>(TEXT("5s")));
	IntervalOptions.Add(MakeShared<FString>(TEXT("10s")));
	CurrentInterval = IntervalOptions[1];
	
	CategoryOptions.Add(MakeShared<FString>(TEXT("All")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Actors")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Components")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Assets")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Widgets")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Textures")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Meshes")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Materials")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Audio")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Animation")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Blueprints")));
	CategoryOptions.Add(MakeShared<FString>(TEXT("Other")));
	CurrentCategory = CategoryOptions[0];
	
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("Any Size")));
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("< 1 KB")));
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("1 KB - 1 MB")));
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("> 1 MB")));
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("> 10 MB")));
	SizeFilterOptions.Add(MakeShared<FString>(TEXT("> 100 MB")));
	CurrentSizeFilter = SizeFilterOptions[0];
	
	SourceOptions.Add(MakeShared<FString>(TEXT("All Sources")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Engine Core")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Engine Runtime")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Engine Editor")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Blueprint VM")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Game Code")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Game Content")));
	SourceOptions.Add(MakeShared<FString>(TEXT("Plugin")));
	CurrentSource = SourceOptions[0];

	HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(ObjectProfilerColumns::ClassName)
			.DefaultLabel(LOCTEXT("ClassNameColumn", "Class Name"))
			.FillWidth(0.25f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::ClassName)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::InstanceCount)
			.DefaultLabel(LOCTEXT("InstanceCountColumn", "Instances"))
			.FillWidth(0.1f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::InstanceCount)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::Delta)
			.DefaultLabel(LOCTEXT("DeltaColumn", "Delta"))
			.FillWidth(0.08f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::Delta)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::Rate)
			.DefaultLabel(LOCTEXT("RateColumn", "Rate/s"))
			.FillWidth(0.08f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::Rate)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::TotalSize)
			.DefaultLabel(LOCTEXT("TotalSizeColumn", "Total Size"))
			.FillWidth(0.1f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::TotalSize)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::AvgSize)
			.DefaultLabel(LOCTEXT("AvgSizeColumn", "Avg Size"))
			.FillWidth(0.1f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::AvgSize)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::History)
			.DefaultLabel(LOCTEXT("HistoryColumn", "History"))
			.FillWidth(0.1f)
		+ SHeaderRow::Column(ObjectProfilerColumns::Module)
			.DefaultLabel(LOCTEXT("ModuleColumn", "Module"))
			.FillWidth(0.1f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::Module)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::Source)
			.DefaultLabel(LOCTEXT("SourceColumn", "Source"))
			.FillWidth(0.1f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::Source)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader)
		+ SHeaderRow::Column(ObjectProfilerColumns::Category)
			.DefaultLabel(LOCTEXT("CategoryColumn", "Category"))
			.FillWidth(0.09f)
			.SortMode(this, &SObjectProfilerWindow::GetSortModeForColumn, ObjectProfilerColumns::Category)
			.OnSort(this, &SObjectProfilerWindow::OnSortColumnHeader);

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(ProfilerLayoutConstants::LeftPanelMinSize + ProfilerLayoutConstants::RightPanelMinSize)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(4.0f)
			
			+ SSplitter::Slot()
			.Value(0.75f)
			.MinSize(ProfilerLayoutConstants::LeftPanelMinSize)
			[
				SNew(SVerticalBox)
				
				// Toolbar row 1 - Action buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					[
						SNew(SHorizontalBox)
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							[
								SNew(SButton)
								.Text(LOCTEXT("Refresh", "Refresh"))
								.ToolTipText(LOCTEXT("RefreshTooltip", "Refresh object statistics"))
								.OnClicked(this, &SObjectProfilerWindow::OnRefreshClicked)
								.IsEnabled(this, &SObjectProfilerWindow::IsNotLoading)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							.Visibility(this, &SObjectProfilerWindow::GetCancelButtonVisibility)
							[
								SNew(SButton)
								.Text(LOCTEXT("Cancel", "Cancel"))
								.ToolTipText(LOCTEXT("CancelTooltip", "Cancel ongoing collection"))
								.OnClicked(this, &SObjectProfilerWindow::OnCancelClicked)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							[
								SNew(SButton)
								.Text(LOCTEXT("ForceGC", "Force GC"))
								.ToolTipText(LOCTEXT("ForceGCTooltip", "Force garbage collection"))
								.OnClicked(this, &SObjectProfilerWindow::OnForceGCClicked)
								.IsEnabled(this, &SObjectProfilerWindow::IsNotLoading)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							[
								SNew(SButton)
								.Text(LOCTEXT("Export", "Export CSV"))
								.ToolTipText(LOCTEXT("ExportTooltip", "Export data to CSV file"))
								.OnClicked(this, &SObjectProfilerWindow::OnExportClicked)
								.IsEnabled(this, &SObjectProfilerWindow::IsNotLoading)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							[
								SNew(SButton)
								.Text(LOCTEXT("FindInCB", "Find in CB"))
								.ToolTipText(LOCTEXT("FindInCBTooltip", "Find selected class in Content Browser"))
								.OnClicked(this, &SObjectProfilerWindow::OnFindInContentBrowserClicked)
								.IsEnabled(this, &SObjectProfilerWindow::CanFindInContentBrowser)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ButtonMinWidth)
							[
								SNew(SButton)
								.Text(LOCTEXT("References", "References"))
								.ToolTipText(LOCTEXT("ReferencesTooltip", "Show reference graph for selected class"))
								.OnClicked(this, &SObjectProfilerWindow::OnShowReferencesClicked)
								.IsEnabled(this, &SObjectProfilerWindow::HasSelection)
							]
						]
						
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
					]
				]
				
				// Toolbar row 2 - Mode, Interval, Group, Filter
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					[
						SNew(SHorizontalBox)
						
						// Mode
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Mode", "Mode:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ComboBoxMinWidth)
							.MaxDesiredWidth(ProfilerLayoutConstants::ComboBoxMaxWidth)
							[
								SAssignNew(ViewModeCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&ViewModeOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnViewModeChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateViewModeComboContent)
								.InitiallySelectedItem(CurrentViewMode)
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetViewModeText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Interval
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::LabelPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Interval", "Interval:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(50.0f)
							.MaxDesiredWidth(80.0f)
							[
								SAssignNew(IntervalCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&IntervalOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnIntervalChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateIntervalComboContent)
								.InitiallySelectedItem(CurrentInterval)
								.IsEnabled_Lambda([this]() { return ViewMode == EProfilerViewMode::RealTime; })
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetIntervalText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Group
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::LabelPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Group", "Group:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ComboBoxMinWidth)
							.MaxDesiredWidth(ProfilerLayoutConstants::ComboBoxMaxWidth)
							[
								SAssignNew(GroupModeCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&GroupModeOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnGroupModeChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateGroupModeComboContent)
								.InitiallySelectedItem(CurrentGroupMode)
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetGroupModeText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Filter text
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::LabelPadding, 0.0f, ProfilerLayoutConstants::ControlPadding, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Filter", "Filter:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.MaxWidth(ProfilerLayoutConstants::FilterBoxMaxWidth)
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::FilterBoxMinWidth)
							[
								SAssignNew(FilterBox, SEditableTextBox)
								.OnTextChanged(this, &SObjectProfilerWindow::OnFilterTextChanged)
							]
						]
					]
				]
				
				// Toolbar row 3 - Category, Size, Source, Leaking, Hot
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					[
						SNew(SHorizontalBox)
						
						// Category
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Category", "Category:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ComboBoxMinWidth)
							.MaxDesiredWidth(ProfilerLayoutConstants::ComboBoxMaxWidth)
							[
								SAssignNew(CategoryFilterCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&CategoryOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnCategoryFilterChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateCategoryComboContent)
								.InitiallySelectedItem(CurrentCategory)
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetCategoryText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Size
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::LabelPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Size", "Size:"))
							.Clipping(EWidgetClipping::ClipToBoundsAlways)
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ComboBoxMinWidth)
							.MaxDesiredWidth(ProfilerLayoutConstants::ComboBoxMaxWidth)
							[
								SAssignNew(SizeFilterCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&SizeFilterOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnSizeFilterChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateSizeFilterComboContent)
								.InitiallySelectedItem(CurrentSizeFilter)
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetSizeFilterText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Source
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SBox)
							.MinDesiredWidth(ProfilerLayoutConstants::ComboBoxMinWidth)
							.MaxDesiredWidth(130.0f)
							[
								SAssignNew(SourceFilterCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&SourceOptions)
								.OnSelectionChanged(this, &SObjectProfilerWindow::OnSourceFilterChanged)
								.OnGenerateWidget(this, &SObjectProfilerWindow::GenerateSourceComboContent)
								.InitiallySelectedItem(CurrentSource)
								.Content()
								[
									SNew(STextBlock)
									.Text(this, &SObjectProfilerWindow::GetSourceText)
									.Clipping(EWidgetClipping::ClipToBoundsAlways)
								]
							]
						]
						
						// Leaking checkbox
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::LabelPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding, ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this]() { return FilterSettings.bShowOnlyLeaking ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged(this, &SObjectProfilerWindow::OnShowOnlyLeakingChanged)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ShowLeaking", "Leaking"))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)))
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
							]
						]
						
						// Hot checkbox
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(ProfilerLayoutConstants::ControlPadding)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this]() { return FilterSettings.bShowOnlyHot ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged(this, &SObjectProfilerWindow::OnShowOnlyHotChanged)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ShowHot", "Hot"))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f)))
								.Clipping(EWidgetClipping::ClipToBoundsAlways)
							]
						]
						
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
					]
				]
				
				// Progress bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 2.0f)
				[
					SNew(SBox)
					.MinDesiredHeight(4.0f)
					.Visibility(this, &SObjectProfilerWindow::GetProgressBarVisibility)
					[
						SAssignNew(ProgressBar, SProgressBar)
						.Percent(this, &SObjectProfilerWindow::GetProgressBarPercent)
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
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FProfilerTreeItem>>)
					.TreeItemsSource(&TreeItems)
					.OnGenerateRow(this, &SObjectProfilerWindow::OnGenerateRowForTree)
					.OnGetChildren(this, &SObjectProfilerWindow::OnGetChildrenForTree)
					.OnMouseButtonDoubleClick(this, &SObjectProfilerWindow::OnTreeDoubleClick)
					.OnSelectionChanged(this, &SObjectProfilerWindow::OnTreeSelectionChanged)
					.HeaderRow(HeaderRow)
					.SelectionMode(ESelectionMode::Single)
				]
				
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
				]
				
				// Status bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					[
						SAssignNew(StatusText, STextBlock)
						.Text(LOCTEXT("StatusReady", "Ready. Click Refresh to load data."))
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
					]
				]
			]
			
			+ SSplitter::Slot()
			.Value(0.25f)
			.MinSize(ProfilerLayoutConstants::RightPanelMinSize)
			[
				SNew(SSnapshotManager)
				.OnCompareSnapshots(this, &SObjectProfilerWindow::OnCompareSnapshots)
				.OnViewDelta(this, &SObjectProfilerWindow::OnViewDelta)
			]
		]
	];

	RealTimeUpdateHandle = FObjectProfilerCore::OnRealTimeUpdate().AddSP(this, &SObjectProfilerWindow::OnRealTimeUpdate);
	
	RefreshDataAsync();
}

SObjectProfilerWindow::~SObjectProfilerWindow()
{
	FObjectProfilerCore::CancelAsyncCollection();
	FObjectProfilerCore::StopRealTimeMonitoring();
	FObjectProfilerCore::OnRealTimeUpdate().Remove(RealTimeUpdateHandle);
}

bool SObjectProfilerWindow::IsNotLoading() const
{
	return !FObjectProfilerCore::IsAsyncCollectionInProgress();
}

EVisibility SObjectProfilerWindow::GetProgressBarVisibility() const
{
	return FObjectProfilerCore::IsAsyncCollectionInProgress() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SObjectProfilerWindow::GetCancelButtonVisibility() const
{
	return FObjectProfilerCore::IsAsyncCollectionInProgress() ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SObjectProfilerWindow::GetProgressBarPercent() const
{
	return CurrentProgress;
}

bool SObjectProfilerWindow::CanFindInContentBrowser() const
{
	if (!SelectedTreeItem.IsValid() || !SelectedTreeItem->Stats.IsValid())
	{
		return false;
	}
	
	EObjectCategory Cat = SelectedTreeItem->Stats->Category;
	return Cat == EObjectCategory::Asset || Cat == EObjectCategory::Blueprint || 
		   Cat == EObjectCategory::Texture || Cat == EObjectCategory::Mesh ||
		   Cat == EObjectCategory::Material || Cat == EObjectCategory::Audio ||
		   Cat == EObjectCategory::DataAsset;
}

bool SObjectProfilerWindow::HasSelection() const
{
	return SelectedTreeItem.IsValid() && SelectedTreeItem->Stats.IsValid();
}

TSharedRef<ITableRow> SObjectProfilerWindow::OnGenerateRowForTree(TSharedPtr<FProfilerTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SObjectProfilerTreeRow, OwnerTable)
		.Item(Item)
		.ViewMode(ViewMode);
}

void SObjectProfilerWindow::OnGetChildrenForTree(TSharedPtr<FProfilerTreeItem> Item, TArray<TSharedPtr<FProfilerTreeItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SObjectProfilerWindow::OnSortColumnHeader(EColumnSortPriority::Type SortPriority, const FName& ColumnId, EColumnSortMode::Type NewSortMode)
{
	if (CurrentSortColumn == ColumnId)
	{
		CurrentSortMode = (CurrentSortMode == EColumnSortMode::Ascending) 
			? EColumnSortMode::Descending 
			: EColumnSortMode::Ascending;
	}
	else
	{
		CurrentSortColumn = ColumnId;
		CurrentSortMode = EColumnSortMode::Descending;
	}

	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
}

EColumnSortMode::Type SObjectProfilerWindow::GetSortModeForColumn(FName ColumnId) const
{
	if (CurrentSortColumn == ColumnId)
	{
		return CurrentSortMode;
	}
	return EColumnSortMode::None;
}

void SObjectProfilerWindow::SortData()
{
	const bool bAscending = (CurrentSortMode == EColumnSortMode::Ascending);

	FilteredStats.Sort([this, bAscending](const TSharedPtr<FObjectClassStats>& A, const TSharedPtr<FObjectClassStats>& B)
	{
		int32 Result = 0;

		if (CurrentSortColumn == ObjectProfilerColumns::ClassName)
		{
			Result = A->ClassName.Compare(B->ClassName);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::InstanceCount)
		{
			Result = (A->InstanceCount > B->InstanceCount) ? 1 : ((A->InstanceCount < B->InstanceCount) ? -1 : 0);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::TotalSize)
		{
			Result = (A->TotalSizeBytes > B->TotalSizeBytes) ? 1 : ((A->TotalSizeBytes < B->TotalSizeBytes) ? -1 : 0);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::AvgSize)
		{
			Result = (A->AverageSizeBytes > B->AverageSizeBytes) ? 1 : ((A->AverageSizeBytes < B->AverageSizeBytes) ? -1 : 0);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::Delta)
		{
			Result = (A->DeltaCount > B->DeltaCount) ? 1 : ((A->DeltaCount < B->DeltaCount) ? -1 : 0);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::Rate)
		{
			Result = (A->RatePerSecond > B->RatePerSecond) ? 1 : ((A->RatePerSecond < B->RatePerSecond) ? -1 : 0);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::Module)
		{
			Result = A->ModuleName.Compare(B->ModuleName);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::Source)
		{
			Result = static_cast<int32>(A->Source) - static_cast<int32>(B->Source);
		}
		else if (CurrentSortColumn == ObjectProfilerColumns::Category)
		{
			Result = static_cast<int32>(A->Category) - static_cast<int32>(B->Category);
		}

		return bAscending ? (Result < 0) : (Result > 0);
	});
}

FReply SObjectProfilerWindow::OnRefreshClicked()
{
	if (ViewMode == EProfilerViewMode::RealTime)
	{
		FObjectProfilerCore::StopRealTimeMonitoring();
		ViewMode = EProfilerViewMode::Normal;
		CurrentViewMode = ViewModeOptions[0];
		ViewModeCombo->SetSelectedItem(CurrentViewMode);
	}
	
	CurrentSnapshotIndex = -1;
	RefreshDataAsync();
	return FReply::Handled();
}

FReply SObjectProfilerWindow::OnCancelClicked()
{
	FObjectProfilerCore::CancelAsyncCollection();
	CurrentProgress = 0.0f;
	StatusText->SetText(LOCTEXT("StatusCancelled", "Collection cancelled."));
	return FReply::Handled();
}

FReply SObjectProfilerWindow::OnForceGCClicked()
{
	FObjectProfilerCore::ForceGarbageCollection();
	RefreshDataAsync();
	return FReply::Handled();
}

FReply SObjectProfilerWindow::OnExportClicked()
{
	FString CSVContent = TEXT("ClassName,Module,Category,InstanceCount,Delta,Rate,TotalSizeBytes,AverageSizeBytes,IsLeaking,IsHot\n");
	
	for (const auto& Stats : FilteredStats)
	{
		CSVContent += FString::Printf(TEXT("%s,%s,%d,%d,%d,%.2f,%lld,%lld,%d,%d\n"),
			*Stats->ClassName,
			*Stats->ModuleName,
			static_cast<int32>(Stats->Category),
			Stats->InstanceCount,
			Stats->DeltaCount,
			Stats->RatePerSecond,
			Stats->TotalSizeBytes,
			Stats->AverageSizeBytes,
			Stats->bIsLeaking ? 1 : 0,
			Stats->bIsHot ? 1 : 0);
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Export Object Stats"),
			FPaths::ProjectSavedDir(),
			TEXT("ObjectStats.csv"),
			TEXT("CSV Files (*.csv)|*.csv"),
			EFileDialogFlags::None,
			OutFiles
		);

		if (bOpened && OutFiles.Num() > 0)
		{
			FFileHelper::SaveStringToFile(CSVContent, *OutFiles[0]);
		}
	}

	return FReply::Handled();
}

FReply SObjectProfilerWindow::OnFindInContentBrowserClicked()
{
	if (!SelectedTreeItem.IsValid() || !SelectedTreeItem->Stats.IsValid())
	{
		return FReply::Handled();
	}
	
	TSharedPtr<FObjectClassStats> Stats = SelectedTreeItem->Stats;
	if (Stats->SampleObjectNames.Num() > 0)
	{
		FString AssetPath = Stats->SampleObjectNames[0];
		
		int32 DotIndex;
		if (AssetPath.FindLastChar(TEXT('.'), DotIndex))
		{
			AssetPath = AssetPath.Left(DotIndex);
		}
		
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> AssetDatas;
		
		if (UObject* Asset = FindObject<UObject>(nullptr, *AssetPath))
		{
			FAssetData AssetData(Asset);
			AssetDatas.Add(AssetData);
			ContentBrowserModule.Get().SyncBrowserToAssets(AssetDatas);
		}
	}
	
	return FReply::Handled();
}

FReply SObjectProfilerWindow::OnShowReferencesClicked()
{
	if (!SelectedTreeItem.IsValid() || !SelectedTreeItem->Stats.IsValid())
	{
		return FReply::Handled();
	}
	
	TSharedRef<SWindow> ReferenceWindow = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("ReferencesWindowTitle", "References: {0}"), FText::FromString(SelectedTreeItem->Stats->ClassName)))
		.ClientSize(FVector2D(1000, 600))
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		[
			SNew(SReferenceGraphWindow)
			.ClassStats(SelectedTreeItem->Stats)
		];

	FSlateApplication::Get().AddWindow(ReferenceWindow);
	
	return FReply::Handled();
}

void SObjectProfilerWindow::OnFilterTextChanged(const FText& NewText)
{
	FilterSettings.TextFilter = NewText.ToString();
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnTreeDoubleClick(TSharedPtr<FProfilerTreeItem> Item)
{
	if (!Item.IsValid())
	{
		return;
	}
	
	if (Item->Type != FProfilerTreeItem::EItemType::Class || !Item->Stats.IsValid())
	{
		return;
	}

	TArray<FString> Instances = FObjectProfilerCore::GetInstancesOfClass(Item->Stats->GetClass());
	
	if (Instances.Num() == 0 && !Item->Stats->IsClassValid())
	{
		Instances = FObjectProfilerCore::GetInstancesOfClass(Item->Stats->ClassName);
	}

	TSharedRef<SVerticalBox> ListBox = SNew(SVerticalBox);
	for (const FString& Instance : Instances)
	{
		ListBox->AddSlot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Instance))
			.AutoWrapText(true)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
		];
	}

	TSharedRef<SWindow> InstanceWindow = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("InstancesWindowTitle", "Instances of {0}"), FText::FromString(Item->Stats->ClassName)))
		.ClientSize(FVector2D(900, 600))
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(8.0f)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("InstanceCount", "Found {0} instances of class {1}"), 
					FText::AsNumber(Instances.Num()), 
					FText::FromString(Item->Stats->ClassName)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					ListBox
				]
			]
		];

	FSlateApplication::Get().AddWindow(InstanceWindow);
}

void SObjectProfilerWindow::OnTreeSelectionChanged(TSharedPtr<FProfilerTreeItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedTreeItem = Item;
}

void SObjectProfilerWindow::OnViewModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo)
{
	if (!NewMode.IsValid())
	{
		return;
	}
	
	CurrentViewMode = NewMode;
	
	if (*NewMode == TEXT("Normal"))
	{
		ViewMode = EProfilerViewMode::Normal;
		FObjectProfilerCore::StopRealTimeMonitoring();
		RefreshDataAsync();
	}
	else if (*NewMode == TEXT("Delta"))
	{
		ViewMode = EProfilerViewMode::Delta;
		FObjectProfilerCore::StopRealTimeMonitoring();
		
		if (CurrentSnapshotIndex >= 0)
		{
			AllStats = FObjectProfilerCore::GetDeltaSinceSnapshot(CurrentSnapshotIndex);
		}
		else
		{
			AllStats = FObjectProfilerCore::GetDeltaSinceSnapshot();
		}
		
		ApplyFilter();
		SortData();
		RebuildTreeView();
		TreeView->RequestTreeRefresh();
		UpdateStatusBar();
	}
	else if (*NewMode == TEXT("Real-Time"))
	{
		ViewMode = EProfilerViewMode::RealTime;
		
		float Interval = 1.0f;
		if (CurrentInterval.IsValid())
		{
			FString IntervalStr = *CurrentInterval;
			IntervalStr.RemoveFromEnd(TEXT("s"));
			Interval = FCString::Atof(*IntervalStr);
		}
		
		FObjectProfilerCore::StartRealTimeMonitoring(Interval);
	}
}

void SObjectProfilerWindow::OnGroupModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo)
{
	if (!NewMode.IsValid())
	{
		return;
	}
	
	CurrentGroupMode = NewMode;
	
	if (*NewMode == TEXT("None"))
	{
		GroupMode = EProfilerGroupMode::None;
	}
	else if (*NewMode == TEXT("By Module"))
	{
		GroupMode = EProfilerGroupMode::ByModule;
	}
	else if (*NewMode == TEXT("By Category"))
	{
		GroupMode = EProfilerGroupMode::ByCategory;
	}
	
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
}

void SObjectProfilerWindow::OnIntervalChanged(TSharedPtr<FString> NewInterval, ESelectInfo::Type SelectInfo)
{
	if (!NewInterval.IsValid())
	{
		return;
	}
	
	CurrentInterval = NewInterval;
	
	FString IntervalStr = *NewInterval;
	IntervalStr.RemoveFromEnd(TEXT("s"));
	float Interval = FCString::Atof(*IntervalStr);
	
	if (ViewMode == EProfilerViewMode::RealTime)
	{
		FObjectProfilerCore::SetRealTimeInterval(Interval);
	}
}

void SObjectProfilerWindow::OnCategoryFilterChanged(TSharedPtr<FString> NewCategory, ESelectInfo::Type SelectInfo)
{
	if (!NewCategory.IsValid())
	{
		return;
	}
	
	CurrentCategory = NewCategory;
	
	if (*NewCategory == TEXT("All"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Unknown;
	}
	else if (*NewCategory == TEXT("Actors"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Actor;
	}
	else if (*NewCategory == TEXT("Components"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Component;
	}
	else if (*NewCategory == TEXT("Assets"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Asset;
	}
	else if (*NewCategory == TEXT("Widgets"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Widget;
	}
	else if (*NewCategory == TEXT("Textures"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Texture;
	}
	else if (*NewCategory == TEXT("Meshes"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Mesh;
	}
	else if (*NewCategory == TEXT("Materials"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Material;
	}
	else if (*NewCategory == TEXT("Audio"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Audio;
	}
	else if (*NewCategory == TEXT("Animation"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Animation;
	}
	else if (*NewCategory == TEXT("Blueprints"))
	{
		FilterSettings.CategoryFilter = EObjectCategory::Blueprint;
	}
	else
	{
		FilterSettings.CategoryFilter = EObjectCategory::Other;
	}
	
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnSizeFilterChanged(TSharedPtr<FString> NewSize, ESelectInfo::Type SelectInfo)
{
	if (!NewSize.IsValid())
	{
		return;
	}
	
	CurrentSizeFilter = NewSize;
	
	if (*NewSize == TEXT("Any Size"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::None;
	}
	else if (*NewSize == TEXT("< 1 KB"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::LessThan1KB;
	}
	else if (*NewSize == TEXT("1 KB - 1 MB"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::Between1KBAnd1MB;
	}
	else if (*NewSize == TEXT("> 1 MB"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::GreaterThan1MB;
	}
	else if (*NewSize == TEXT("> 10 MB"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::GreaterThan10MB;
	}
	else if (*NewSize == TEXT("> 100 MB"))
	{
		FilterSettings.SizeFilter = ESizeFilterMode::GreaterThan100MB;
	}
	
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnShowOnlyLeakingChanged(ECheckBoxState NewState)
{
	FilterSettings.bShowOnlyLeaking = (NewState == ECheckBoxState::Checked);
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnShowOnlyHotChanged(ECheckBoxState NewState)
{
	FilterSettings.bShowOnlyHot = (NewState == ECheckBoxState::Checked);
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::RefreshDataAsync()
{
	if (FObjectProfilerCore::IsAsyncCollectionInProgress())
	{
		return;
	}

	CurrentProgress = 0.0f;
	StatusText->SetText(LOCTEXT("StatusLoading", "Loading..."));

	FObjectProfilerCore::CollectObjectStatsAsync(
		5,
		FOnObjectStatsCollected::CreateSP(this, &SObjectProfilerWindow::OnAsyncCollectionComplete),
		FOnCollectionProgress::CreateSP(this, &SObjectProfilerWindow::OnAsyncCollectionProgress)
	);
}

void SObjectProfilerWindow::OnAsyncCollectionComplete(TArray<TSharedPtr<FObjectClassStats>> Results)
{
	CurrentProgress = 1.0f;
	AllStats = MoveTemp(Results);
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnAsyncCollectionProgress(float Progress)
{
	CurrentProgress = Progress;
	
	const int32 Percent = FMath::RoundToInt(Progress * 100.0f);
	StatusText->SetText(FText::Format(LOCTEXT("StatusLoadingProgress", "Loading... {0}%"), FText::AsNumber(Percent)));
}

void SObjectProfilerWindow::OnRealTimeUpdate(const TArray<TSharedPtr<FObjectClassStats>>& Results)
{
	if (ViewMode != EProfilerViewMode::RealTime)
	{
		return;
	}
	
	AllStats = Results;
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::OnCompareSnapshots(int32 IndexA, int32 IndexB)
{
	ViewMode = EProfilerViewMode::Delta;
	CurrentViewMode = ViewModeOptions[1];
	ViewModeCombo->SetSelectedItem(CurrentViewMode);
	
	AllStats = FObjectProfilerCore::CompareTwoSnapshots(IndexA, IndexB);
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	
	const TArray<FObjectSnapshot>& History = FObjectProfilerCore::GetSnapshotHistory();
	if (IndexA < History.Num() && IndexB < History.Num())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("StatusComparing", "Comparing: {0} vs {1} | Classes changed: {2}"),
			FText::FromString(History[IndexA].Name),
			FText::FromString(History[IndexB].Name),
			FText::AsNumber(FilteredStats.Num())));
	}
}

void SObjectProfilerWindow::OnViewDelta(int32 SnapshotIndex)
{
	CurrentSnapshotIndex = SnapshotIndex;
	ViewMode = EProfilerViewMode::Delta;
	CurrentViewMode = ViewModeOptions[1];
	ViewModeCombo->SetSelectedItem(CurrentViewMode);
	
	AllStats = FObjectProfilerCore::GetDeltaSinceSnapshot(SnapshotIndex);
	ApplyFilter();
	SortData();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerWindow::ApplyFilter()
{
	FilteredStats.Empty();

	for (const auto& Stats : AllStats)
	{
		if (!FilterSettings.TextFilter.IsEmpty())
		{
			if (!Stats->ClassName.Contains(FilterSettings.TextFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		
		if (FilterSettings.CategoryFilter != EObjectCategory::Unknown)
		{
			if (Stats->Category != FilterSettings.CategoryFilter)
			{
				continue;
			}
		}
		
		if (FilterSettings.SourceFilter != EObjectSource::Unknown)
		{
			if (Stats->Source != FilterSettings.SourceFilter)
			{
				continue;
			}
		}
		
		if (FilterSettings.bShowOnlyLeaking || FilterSettings.bShowOnlyHot)
		{
			bool bPassesFilter = false;
			
			if (FilterSettings.bShowOnlyLeaking && Stats->bIsLeaking)
			{
				bPassesFilter = true;
			}
			if (FilterSettings.bShowOnlyHot && Stats->bIsHot)
			{
				bPassesFilter = true;
			}
			
			if (!bPassesFilter)
			{
				continue;
			}
		}
		
		switch (FilterSettings.SizeFilter)
		{
		case ESizeFilterMode::LessThan1KB:
			if (Stats->TotalSizeBytes >= 1024)
			{
				continue;
			}
			break;
		case ESizeFilterMode::Between1KBAnd1MB:
			if (Stats->TotalSizeBytes < 1024 || Stats->TotalSizeBytes >= 1024 * 1024)
			{
				continue;
			}
			break;
		case ESizeFilterMode::GreaterThan1MB:
			if (Stats->TotalSizeBytes < 1024 * 1024)
			{
				continue;
			}
			break;
		case ESizeFilterMode::GreaterThan10MB:
			if (Stats->TotalSizeBytes < 10 * 1024 * 1024)
			{
				continue;
			}
			break;
		case ESizeFilterMode::GreaterThan100MB:
			if (Stats->TotalSizeBytes < 100LL * 1024 * 1024)
			{
				continue;
			}
			break;
		default:
			break;
		}
		
		FilteredStats.Add(Stats);
	}
}

void SObjectProfilerWindow::RebuildTreeView()
{
	TreeItems = FObjectProfilerCore::BuildTreeView(FilteredStats, GroupMode);
}

void SObjectProfilerWindow::UpdateStatusBar()
{
	int32 TotalObjects = 0;
	int64 TotalSize = 0;
	int32 LeakingCount = 0;
	int32 HotCount = 0;

	for (const auto& Stats : FilteredStats)
	{
		TotalObjects += Stats->InstanceCount;
		TotalSize += Stats->TotalSizeBytes;
		if (Stats->bIsLeaking) LeakingCount++;
		if (Stats->bIsHot) HotCount++;
	}

	FString StatusStr;
	
	if (ViewMode == EProfilerViewMode::Delta)
	{
		StatusStr = FString::Printf(TEXT("Delta Mode | Classes: %d | Objects changed: %d"),
			FilteredStats.Num(), TotalObjects);
	}
	else if (ViewMode == EProfilerViewMode::RealTime)
	{
		StatusStr = FString::Printf(TEXT("Real-Time (%.1fs) | Classes: %d | Objects: %d | Size: %s"),
			FObjectProfilerCore::GetRealTimeInterval(),
			FilteredStats.Num(), TotalObjects, *FObjectProfilerCore::FormatBytes(TotalSize));
	}
	else
	{
		StatusStr = FString::Printf(TEXT("Classes: %d | Objects: %d | Size: %s"),
			FilteredStats.Num(), TotalObjects, *FObjectProfilerCore::FormatBytes(TotalSize));
	}
	
	if (LeakingCount > 0)
	{
		StatusStr += FString::Printf(TEXT(" | Leaking: %d"), LeakingCount);
	}
	
	if (HotCount > 0)
	{
		StatusStr += FString::Printf(TEXT(" | Hot: %d"), HotCount);
	}

	if (!FilterSettings.IsDefault())
	{
		StatusStr += TEXT(" | Filtered");
	}

	StatusText->SetText(FText::FromString(StatusStr));
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateViewModeComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateGroupModeComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateIntervalComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateCategoryComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateSizeFilterComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

TSharedRef<SWidget> SObjectProfilerWindow::GenerateSourceComboContent(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Clipping(EWidgetClipping::ClipToBoundsAlways);
}

FText SObjectProfilerWindow::GetViewModeText() const
{
	return CurrentViewMode.IsValid() ? FText::FromString(*CurrentViewMode) : FText::GetEmpty();
}

FText SObjectProfilerWindow::GetGroupModeText() const
{
	return CurrentGroupMode.IsValid() ? FText::FromString(*CurrentGroupMode) : FText::GetEmpty();
}

FText SObjectProfilerWindow::GetIntervalText() const
{
	return CurrentInterval.IsValid() ? FText::FromString(*CurrentInterval) : FText::GetEmpty();
}

FText SObjectProfilerWindow::GetCategoryText() const
{
	return CurrentCategory.IsValid() ? FText::FromString(*CurrentCategory) : FText::GetEmpty();
}

FText SObjectProfilerWindow::GetSizeFilterText() const
{
	return CurrentSizeFilter.IsValid() ? FText::FromString(*CurrentSizeFilter) : FText::GetEmpty();
}

FText SObjectProfilerWindow::GetSourceText() const
{
	return CurrentSource.IsValid() ? FText::FromString(*CurrentSource) : FText::GetEmpty();
}

void SObjectProfilerWindow::OnSourceFilterChanged(TSharedPtr<FString> NewSource, ESelectInfo::Type SelectInfo)
{
	CurrentSource = NewSource;
	
	if (CurrentSource.IsValid())
	{
		FString SourceStr = *CurrentSource;
		if (SourceStr == TEXT("All Sources"))
		{
			FilterSettings.SourceFilter = EObjectSource::Unknown;
		}
		else if (SourceStr == TEXT("Engine Core"))
		{
			FilterSettings.SourceFilter = EObjectSource::EngineCore;
		}
		else if (SourceStr == TEXT("Engine Runtime"))
		{
			FilterSettings.SourceFilter = EObjectSource::EngineRuntime;
		}
		else if (SourceStr == TEXT("Engine Editor"))
		{
			FilterSettings.SourceFilter = EObjectSource::EngineEditor;
		}
		else if (SourceStr == TEXT("Blueprint VM"))
		{
			FilterSettings.SourceFilter = EObjectSource::BlueprintVM;
		}
		else if (SourceStr == TEXT("Game Code"))
		{
			FilterSettings.SourceFilter = EObjectSource::GameCode;
		}
		else if (SourceStr == TEXT("Game Content"))
		{
			FilterSettings.SourceFilter = EObjectSource::GameContent;
		}
		else if (SourceStr == TEXT("Plugin"))
		{
			FilterSettings.SourceFilter = EObjectSource::Plugin;
		}
	}
	
	ApplyFilter();
	RebuildTreeView();
	TreeView->RequestTreeRefresh();
	UpdateStatusBar();
}

void SObjectProfilerTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	ViewMode = InArgs._ViewMode;
	SMultiColumnTableRow<TSharedPtr<FProfilerTreeItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SObjectProfilerTreeRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	
	bool bIsGroupRow = (Item->Type != FProfilerTreeItem::EItemType::Class);
	TSharedPtr<FObjectClassStats> Stats = Item->Stats;

	if (ColumnName == ObjectProfilerColumns::ClassName)
	{
		FLinearColor TextColor = FLinearColor::White;
		
		if (bIsGroupRow)
		{
			if (Item->HasLeakingChildren())
			{
				TextColor = FLinearColor(1.0f, 0.3f, 0.3f);
			}
			else if (Item->HasHotChildren())
			{
				TextColor = FLinearColor(1.0f, 0.5f, 0.0f);
			}
		}
		else if (Stats.IsValid())
		{
			if (Stats->bIsLeaking)
			{
				TextColor = FLinearColor(1.0f, 0.3f, 0.3f);
			}
			else if (Stats->bIsHot)
			{
				TextColor = FLinearColor(1.0f, 0.5f, 0.0f);
			}
		}
		
		FString DisplayName = Item->DisplayName;
		if (bIsGroupRow)
		{
			DisplayName = FString::Printf(TEXT("%s (%d)"), *Item->DisplayName, Item->Children.Num());
		}
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(DisplayName))
				.ColorAndOpacity(FSlateColor(TextColor))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	
	if (bIsGroupRow)
	{
		if (ColumnName == ObjectProfilerColumns::InstanceCount)
		{
			return SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(Item->GetAggregatedInstanceCount()))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
					.Margin(FMargin(4.0f, 2.0f))
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
				];
		}
		else if (ColumnName == ObjectProfilerColumns::TotalSize)
		{
			return SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FObjectProfilerCore::FormatBytes(Item->GetAggregatedSize())))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
					.Margin(FMargin(4.0f, 2.0f))
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
				];
		}
		return SNullWidget::NullWidget;
	}
	
	if (!Stats.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == ObjectProfilerColumns::InstanceCount)
	{
		FSlateColor TextColor = FSlateColor(FLinearColor::White);
		if (Stats->InstanceCount > 10000)
		{
			TextColor = FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f));
		}
		else if (Stats->InstanceCount > 1000)
		{
			TextColor = FSlateColor(FLinearColor(1.0f, 0.7f, 0.3f));
		}
		else if (Stats->InstanceCount > 100)
		{
			TextColor = FSlateColor(FLinearColor(1.0f, 1.0f, 0.3f));
		}

		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Stats->InstanceCount))
				.ColorAndOpacity(TextColor)
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::Delta)
	{
		FSlateColor TextColor = FSlateColor(FLinearColor::White);
		FString DeltaText;
		
		if (Stats->DeltaCount > 0)
		{
			DeltaText = FString::Printf(TEXT("+%d"), Stats->DeltaCount);
			TextColor = FSlateColor(FLinearColor(0.3f, 1.0f, 0.3f));
		}
		else if (Stats->DeltaCount < 0)
		{
			DeltaText = FString::Printf(TEXT("%d"), Stats->DeltaCount);
			TextColor = FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f));
		}
		else
		{
			DeltaText = TEXT("0");
			TextColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
		}
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(DeltaText))
				.ColorAndOpacity(TextColor)
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::Rate)
	{
		FSlateColor TextColor = FSlateColor(FLinearColor::White);
		
		if (FMath::Abs(Stats->RatePerSecond) > 10.0f)
		{
			TextColor = Stats->RatePerSecond > 0 
				? FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f))
				: FSlateColor(FLinearColor(0.3f, 0.8f, 1.0f));
		}
		else if (FMath::Abs(Stats->RatePerSecond) < 0.01f)
		{
			TextColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
		}
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%.1f"), Stats->RatePerSecond)))
				.ColorAndOpacity(TextColor)
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::TotalSize)
	{
		FString SizeText = Stats->bSizeAvailable 
			? FObjectProfilerCore::FormatBytes(Stats->TotalSizeBytes)
			: TEXT("N/A");
			
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SizeText))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::AvgSize)
	{
		FString SizeText = Stats->bSizeAvailable 
			? FObjectProfilerCore::FormatBytes(Stats->AverageSizeBytes)
			: TEXT("N/A");
			
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SizeText))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::History)
	{
		if (Stats->History.Num() < 2)
		{
			return SNullWidget::NullWidget;
		}
		
		FLinearColor LineColor = FLinearColor(0.3f, 0.7f, 1.0f);
		if (Stats->bIsLeaking)
		{
			LineColor = FLinearColor(1.0f, 0.3f, 0.3f);
		}
		else if (Stats->bIsHot)
		{
			LineColor = FLinearColor(1.0f, 0.5f, 0.0f);
		}
		
		TSharedPtr<SSparkline> SparklineWidget;
		TSharedRef<SWidget> Result = SNew(SBox)
			.MinDesiredWidth(60.0f)
			.MinDesiredHeight(20.0f)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SAssignNew(SparklineWidget, SSparkline)
				.LineColor(LineColor)
				.FillColor(FLinearColor(LineColor.R, LineColor.G, LineColor.B, 0.2f))
				.bShowFill(true)
				.bShowLine(true)
			];
		
		SparklineWidget->SetValuesFromHistory(Stats->History, true);
		
		return Result;
	}
	else if (ColumnName == ObjectProfilerColumns::Module)
	{
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Stats->ModuleName))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::Source)
	{
		FLinearColor SourceColor = FLinearColor(0.7f, 0.7f, 0.7f);
		
		switch (Stats->Source)
		{
		case EObjectSource::EngineCore:
			SourceColor = FLinearColor(0.4f, 0.6f, 0.8f);
			break;
		case EObjectSource::EngineRuntime:
			SourceColor = FLinearColor(0.5f, 0.7f, 0.9f);
			break;
		case EObjectSource::EngineEditor:
			SourceColor = FLinearColor(0.6f, 0.5f, 0.8f);
			break;
		case EObjectSource::BlueprintVM:
			SourceColor = FLinearColor(0.2f, 0.6f, 1.0f);
			break;
		case EObjectSource::GameCode:
			SourceColor = FLinearColor(0.3f, 0.9f, 0.3f);
			break;
		case EObjectSource::GameContent:
			SourceColor = FLinearColor(0.5f, 1.0f, 0.5f);
			break;
		case EObjectSource::Plugin:
			SourceColor = FLinearColor(1.0f, 0.8f, 0.3f);
			break;
		default:
			break;
		}
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FObjectProfilerCore::GetSourceDisplayName(Stats->Source)))
				.ColorAndOpacity(FSlateColor(SourceColor))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}
	else if (ColumnName == ObjectProfilerColumns::Category)
	{
		static const TMap<EObjectCategory, FString> CategoryNames = {
			{EObjectCategory::Unknown, TEXT("Unknown")},
			{EObjectCategory::Actor, TEXT("Actor")},
			{EObjectCategory::Component, TEXT("Component")},
			{EObjectCategory::Asset, TEXT("Asset")},
			{EObjectCategory::Widget, TEXT("Widget")},
			{EObjectCategory::Animation, TEXT("Animation")},
			{EObjectCategory::Audio, TEXT("Audio")},
			{EObjectCategory::Material, TEXT("Material")},
			{EObjectCategory::Texture, TEXT("Texture")},
			{EObjectCategory::Mesh, TEXT("Mesh")},
			{EObjectCategory::Blueprint, TEXT("Blueprint")},
			{EObjectCategory::DataAsset, TEXT("DataAsset")},
			{EObjectCategory::Subsystem, TEXT("Subsystem")},
			{EObjectCategory::GameInstance, TEXT("GameInst")},
			{EObjectCategory::Other, TEXT("Other")}
		};
		
		return SNew(SBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CategoryNames.FindRef(Stats->Category)))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
				.Margin(FMargin(4.0f, 2.0f))
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE