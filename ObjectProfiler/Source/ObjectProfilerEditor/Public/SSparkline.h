//Copyright PsinaDev 2025.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "ObjectProfilerTypes.h"

class SSparkline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSparkline)
		: _LineColor(FLinearColor::White)
		, _FillColor(FLinearColor(0.2f, 0.4f, 0.8f, 0.3f))
		, _BackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.5f))
		, _bShowFill(true)
		, _bShowLine(true)
		, _LineThickness(1.0f)
	{}
	SLATE_ARGUMENT(TArray<float>, Values)
	SLATE_ARGUMENT(FLinearColor, LineColor)
	SLATE_ARGUMENT(FLinearColor, FillColor)
	SLATE_ARGUMENT(FLinearColor, BackgroundColor)
	SLATE_ARGUMENT(bool, bShowFill)
	SLATE_ARGUMENT(bool, bShowLine)
	SLATE_ARGUMENT(float, LineThickness)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);
	
	void SetValues(const TArray<float>& InValues);
	void SetValuesFromHistory(const TArray<FObjectHistoryPoint>& History, bool bUseCount = true);
	void SetLineColor(const FLinearColor& InColor);
	
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
		
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TArray<float> Values;
	FLinearColor LineColor;
	FLinearColor FillColor;
	FLinearColor BackgroundColor;
	bool bShowFill;
	bool bShowLine;
	float LineThickness;
};