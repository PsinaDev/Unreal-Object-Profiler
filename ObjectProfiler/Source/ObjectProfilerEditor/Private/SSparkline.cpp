//Copyright PsinaDev 2025.

#include "SSparkline.h"
#include "Rendering/DrawElements.h"

void SSparkline::Construct(const FArguments& InArgs)
{
	Values = InArgs._Values;
	LineColor = InArgs._LineColor;
	FillColor = InArgs._FillColor;
	BackgroundColor = InArgs._BackgroundColor;
	bShowFill = InArgs._bShowFill;
	bShowLine = InArgs._bShowLine;
	LineThickness = InArgs._LineThickness;
}

void SSparkline::SetValues(const TArray<float>& InValues)
{
	Values = InValues;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SSparkline::SetValuesFromHistory(const TArray<FObjectHistoryPoint>& History, bool bUseCount)
{
	Values.Empty();
	
	if (History.Num() == 0)
	{
		return;
	}
	
	for (const FObjectHistoryPoint& Point : History)
	{
		if (bUseCount)
		{
			Values.Add(static_cast<float>(Point.InstanceCount));
		}
		else
		{
			Values.Add(static_cast<float>(Point.TotalSizeBytes));
		}
	}
	
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SSparkline::SetLineColor(const FLinearColor& InColor)
{
	LineColor = InColor;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SSparkline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(60.0f, 20.0f);
}

int32 SSparkline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float Width = LocalSize.X;
	const float Height = LocalSize.Y;
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FAppStyle::GetBrush("WhiteBrush"),
		DrawEffects,
		BackgroundColor
	);
	
	if (Values.Num() < 2)
	{
		return LayerId;
	}
	
	float MinValue = Values[0];
	float MaxValue = Values[0];
	for (float Value : Values)
	{
		MinValue = FMath::Min(MinValue, Value);
		MaxValue = FMath::Max(MaxValue, Value);
	}
	
	const float Range = MaxValue - MinValue;
	const float Padding = 2.0f;
	const float DrawWidth = Width - Padding * 2;
	const float DrawHeight = Height - Padding * 2;
	
	auto NormalizeValue = [&](float Value) -> float
	{
		if (Range < 0.001f)
		{
			return 0.5f;
		}
		return (Value - MinValue) / Range;
	};
	
	const float StepX = DrawWidth / (Values.Num() - 1);
	
	if (bShowFill)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		
		for (int32 i = 0; i < Values.Num(); ++i)
		{
			float X = Padding + i * StepX;
			float NormY = NormalizeValue(Values[i]);
			float Y = Padding + DrawHeight * (1.0f - NormY);
			
			FSlateVertex TopVertex;
			TopVertex.Position = FVector2f(X, Y);
			TopVertex.Color = FillColor.ToFColor(true);
			TopVertex.TexCoords[0] = 0;
			TopVertex.TexCoords[1] = 0;
			TopVertex.TexCoords[2] = 0;
			TopVertex.TexCoords[3] = 0;
			
			FSlateVertex BottomVertex;
			BottomVertex.Position = FVector2f(X, Height - Padding);
			BottomVertex.Color = FillColor.ToFColor(true);
			BottomVertex.TexCoords[0] = 0;
			BottomVertex.TexCoords[1] = 0;
			BottomVertex.TexCoords[2] = 0;
			BottomVertex.TexCoords[3] = 0;
			
			Vertices.Add(TopVertex);
			Vertices.Add(BottomVertex);
			
			if (i > 0)
			{
				int32 BaseIdx = (i - 1) * 2;
				Indices.Add(BaseIdx);
				Indices.Add(BaseIdx + 1);
				Indices.Add(BaseIdx + 2);
				
				Indices.Add(BaseIdx + 1);
				Indices.Add(BaseIdx + 3);
				Indices.Add(BaseIdx + 2);
			}
		}
		
		if (Vertices.Num() > 0 && Indices.Num() > 0)
		{
			const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
			FSlateResourceHandle ResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
			
			FSlateDrawElement::MakeCustomVerts(
				OutDrawElements,
				LayerId + 1,
				ResourceHandle,
				Vertices,
				Indices,
				nullptr,
				0,
				0,
				DrawEffects
			);
		}
	}
	
	if (bShowLine)
	{
		TArray<FVector2D> Points;
		Points.Reserve(Values.Num());
		
		for (int32 i = 0; i < Values.Num(); ++i)
		{
			float X = Padding + i * StepX;
			float NormY = NormalizeValue(Values[i]);
			float Y = Padding + DrawHeight * (1.0f - NormY);
			Points.Add(FVector2D(X, Y));
		}
		
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			DrawEffects,
			LineColor,
			true,
			LineThickness
		);
	}
	
	return LayerId + 2;
}