#include "CandyFight/UI/CandyFightCrosshairWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// SObjectWidget paints Designer children before NativePaint. Wrap the children too,
// so both paths use the constrained game view (not the full viewport/letterbox).
class SCandyFightCrosshairClip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCandyFightCrosshairClip) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UCandyFightCrosshairWidget>, Owner)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args)
	{
		Owner = Args._Owner;
		ChildSlot[Args._Content.Widget];
	}
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
	{
		FSlateRect Clip;
		const bool bClip = Owner.IsValid() && Owner->GetAimClipRect(Args, Clip);
		if (bClip) Elements.PushClip(FSlateClippingZone(Clip));
		const int32 Result = SCompoundWidget::OnPaint(Args, Geometry, CullingRect, Elements, Layer, Style, bEnabled);
		if (bClip) Elements.PopClip();
		return Result;
	}
private:
	TWeakObjectPtr<UCandyFightCrosshairWidget> Owner;
};

TSharedRef<SWidget> UCandyFightCrosshairWidget::RebuildWidget()
{
	ClipWidget = SNew(SCandyFightCrosshairClip).Owner(this)[Super::RebuildWidget()];
	return ClipWidget.ToSharedRef();
}

void UCandyFightCrosshairWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ClipWidget.Reset();
}

void UCandyFightCrosshairWidget::SetAimViewRect(const FIntRect& InRect)
{
	if (AimViewRect == InRect) return;
	AimViewRect = InRect;
	if (ClipWidget) ClipWidget->Invalidate(EInvalidateWidgetReason::Paint);
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool UCandyFightCrosshairWidget::GetAimClipRect(const FPaintArgs& Args, FSlateRect& OutRect) const
{
	if (AimViewRect.Width() <= 0 || AimViewRect.Height() <= 0) return false;
	FVector2D Min, Max;
	USlateBlueprintLibrary::ScreenToWidgetAbsolute(this, FVector2D(AimViewRect.Min), Min);
	USlateBlueprintLibrary::ScreenToWidgetAbsolute(this, FVector2D(AimViewRect.Max), Max);
	const FVector2D DesktopOffset(Args.GetWindowToDesktopTransform());
	Min -= DesktopOffset;
	Max -= DesktopOffset;
	OutRect = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	return !Min.ContainsNaN() && !Max.ContainsNaN() && Max.X > Min.X && Max.Y > Min.Y;
}

UCandyFightCrosshairWidget::UCandyFightCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SpreadBindings = {
		{TEXT("LeftLine"), FVector2D(-1.0, 0.0)},
		{TEXT("LeftOutline"), FVector2D(-1.0, 0.0)},
		{TEXT("RightLine"), FVector2D(1.0, 0.0)},
		{TEXT("RightOutline"), FVector2D(1.0, 0.0)},
		{TEXT("TopLine"), FVector2D(0.0, -1.0)},
		{TEXT("TopOutline"), FVector2D(0.0, -1.0)},
		{TEXT("BottomLine"), FVector2D(0.0, 1.0)},
		{TEXT("BottomOutline"), FVector2D(0.0, 1.0)}
	};
}

void UCandyFightCrosshairWidget::SetScreenPosition(const FVector2D& InScreenPosition)
{
	ScreenPosition = InScreenPosition;
	UpdateVisualPosition();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UCandyFightCrosshairWidget::SetSpread(float InSpread)
{
	bDistanceDisplayActive = false;
	DistanceState.Reset();
	ApplySpreadValue(InSpread);
}

void UCandyFightCrosshairWidget::RecordAimProjection(const FMatrix& ViewProjection, uint64 Frame)
{
	AimViewProjection = ViewProjection;
	AimProjectionFrame = Frame;
}

void UCandyFightCrosshairWidget::UpdateWeaponSpread(const FVector& Muzzle, const FVector& Aim,
	float HalfAngleDegrees, bool bValidAim, double Now, uint64 Frame)
{
	if (!bEnableDistanceSpread)
	{
		SetSpread(HalfAngleDegrees / 10.f);
		return;
	}
	double Radius = 0.;
	if (bValidAim && AimProjectionFrame == Frame)
	{
		if (!CandyFightDistanceSpread::ProjectRadius(AimViewProjection, AimViewRect, Muzzle, Aim, HalfAngleDegrees, Radius)) Radius = 0.;
	}
	const double Min = CandyFightDistanceSpread::TargetGap(0., DistanceMinGap, DistanceMaxGap, 1.);
	const double Max = CandyFightDistanceSpread::TargetGap(TNumericLimits<double>::Max(), DistanceMinGap, DistanceMaxGap, 1.);
	const double Target = CandyFightDistanceSpread::TargetGap(Radius, DistanceMinGap, DistanceMaxGap, DistanceSpreadMultiplier);
	CurrentDistanceGap = FMath::Clamp(DistanceState.Update(Target, DistanceSpreadSmoothSeconds, Now, Frame), Min, Max);
	bDistanceDisplayActive = true;
	ApplySpreadValue(Max > Min ? (CurrentDistanceGap - Min) / (Max - Min) : 0.);
}

void UCandyFightCrosshairWidget::ApplySpreadValue(float InSpread)
{
	const float NewSpread = FMath::IsFinite(InSpread) ? FMath::Clamp(InSpread, 0.0f, 1.0f) : 0.0f;
	const bool bSpreadChanged = Spread != NewSpread;
	Spread = NewSpread;
	UpdateSpreadVisuals();
	if (bSpreadChanged)
	{
		OnCrosshairSpreadChanged(Spread);
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UCandyFightCrosshairWidget::UpdateSpreadVisuals()
{
	if (!bApplyDesignerSpread)
	{
		for (const auto& Entry : InitialSpreadTranslations)
		{
			if (UWidget* Widget = Entry.Key.Get())
			{
				Widget->SetRenderTranslation(Entry.Value);
			}
		}
		return;
	}

	const float Distance = FMath::IsFinite(VisualSpreadDistance) ? FMath::Max(0.0f, VisualSpreadDistance) : 0.0f;
	const float Dpi = GetWorld() ? FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_SMALL_NUMBER) : 1.f;
	// Designer coordinates are authored relative to BaseGap. Preserve the layout,
	// line sizes and custom transforms; only move each arm and its outline together.
	const float Outward = bDistanceDisplayActive
		? CandyFightDistanceSpread::DesignerOffset(CurrentDistanceGap, BaseGap, Dpi, bPixelPerfectVisual)
		: Distance * Spread;
	for (const FCandyFightCrosshairSpreadBinding& Binding : SpreadBindings)
	{
		UWidget* Widget = GetWidgetFromName(Binding.WidgetName);
		if (!Widget || Widget == CrosshairVisual || Binding.Direction.ContainsNaN())
		{
			continue;
		}
		const TWeakObjectPtr<UWidget> Key(Widget);
		FVector2D* InitialTranslation = InitialSpreadTranslations.Find(Key);
		if (!InitialTranslation)
		{
			InitialTranslation = &InitialSpreadTranslations.Add(Key, Widget->GetRenderTransform().Translation);
		}
		FVector2D Offset = Binding.Direction * Outward;
		if (bPixelPerfectVisual)
		{
			Offset.X = FMath::RoundToDouble(Offset.X);
			Offset.Y = FMath::RoundToDouble(Offset.Y);
		}
		Widget->SetRenderTranslation(*InitialTranslation + Offset);
	}
}

float UCandyFightCrosshairWidget::GetMaxDrawExtent() const
{
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	const float NativeGap = bDistanceDisplayActive ? CurrentDistanceGap / FMath::Max(ViewportScale, UE_SMALL_NUMBER) : FMath::Max(BaseGap, MaxSpreadGap);
	const float NativeExtent = bDrawNativeCrosshair
		? FMath::Max(0.0f, NativeGap) + FMath::Max(0.0f, LineLength)
		+ FMath::Max(0.0f, FMath::Max(LineThickness, OutlineThickness)) * 0.5f
		: 0.0f;
	// Compatibility size query only; clipping is independent of this extent.
	float SpreadExtent = 0.0f;
	if (bApplyDesignerSpread && FMath::IsFinite(VisualSpreadDistance))
	{
		for (const FCandyFightCrosshairSpreadBinding& Binding : SpreadBindings)
		{
			if (!Binding.Direction.ContainsNaN())
			{
				const double Distance = bDistanceDisplayActive
					? FMath::Abs(CurrentDistanceGap / (bPixelPerfectVisual ? 1.f : FMath::Max(ViewportScale, UE_SMALL_NUMBER)) - FMath::Max(0.f, BaseGap))
					: FMath::Max(0.f, VisualSpreadDistance);
				SpreadExtent = FMath::Max(SpreadExtent, static_cast<float>(Binding.Direction.Size() * Distance));
			}
		}
	}
	const float VisualExtent = CrosshairVisual ? FMath::Max(0.0f, VisualMaxDrawExtent) + SpreadExtent : 0.0f;
	return FMath::Max(NativeExtent * ViewportScale,
		bPixelPerfectVisual ? VisualExtent : VisualExtent * ViewportScale);
}

void UCandyFightCrosshairWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	Spread = FMath::IsFinite(Spread) ? FMath::Clamp(Spread, 0.0f, 1.0f) : 0.0f;
	UpdateSpreadVisuals();
}

void UCandyFightCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DistanceState.Reset();
	bDistanceDisplayActive = false;
	if (UCanvasPanelSlot* VisualSlot = CrosshairVisual ? Cast<UCanvasPanelSlot>(CrosshairVisual->Slot) : nullptr)
	{
		VisualSlot->SetAnchors(FAnchors(0.5f));
		VisualSlot->SetAlignment(FVector2D(0.5f));
		VisualSlot->SetPosition(FVector2D::ZeroVector);
		CrosshairVisual->SetRenderTransformPivot(FVector2D(0.5f));
	}
	UpdateSpreadVisuals();
	OnCrosshairSpreadChanged(Spread);
	UpdateVisualPosition();
}

void UCandyFightCrosshairWidget::UpdateVisualPosition()
{
	UCanvasPanelSlot* VisualSlot = CrosshairVisual ? Cast<UCanvasPanelSlot>(CrosshairVisual->Slot) : nullptr;
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	if (!VisualSlot || ViewportSize.IsNearlyZero())
	{
		return;
	}

	FVector2D VisualScreenPosition = ScreenPosition;
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	const float SafeViewportScale = FMath::Max(ViewportScale, UE_SMALL_NUMBER);
	const float VisualScale = bPixelPerfectVisual ? 1.0f / SafeViewportScale : 1.0f;
	if (bPixelPerfectVisual)
	{
		VisualScreenPosition.X = FMath::RoundToDouble(VisualScreenPosition.X);
		VisualScreenPosition.Y = FMath::RoundToDouble(VisualScreenPosition.Y);
	}

	// The root Canvas fills the viewport, whose local units are physical pixels divided by DPI scale.
	const FVector2D Translation = (VisualScreenPosition - ViewportSize * 0.5f) / SafeViewportScale;
	const FWidgetTransform& CurrentTransform = CrosshairVisual->GetRenderTransform();
	if (!CurrentTransform.Translation.Equals(Translation))
	{
		CrosshairVisual->SetRenderTranslation(Translation);
	}
	const FVector2D Scale(VisualScale);
	if (!CurrentTransform.Scale.Equals(Scale))
	{
		CrosshairVisual->SetRenderScale(Scale);
	}
}

void UCandyFightCrosshairWidget::SetAimAssistDebug(const FCandyFightAimAssistDebug& Debug)
{
	AimAssistDebug = Debug;
	InvalidateLayoutAndVolatility();
}

int32 UCandyFightCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FSlateRect Clip;
	const bool bClip = GetAimClipRect(Args, Clip);
	if (bClip) OutDrawElements.PushClip(FSlateClippingZone(Clip));
	ON_SCOPE_EXIT { if (bClip) OutDrawElements.PopClip(); };
	int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (CandyFightAimAssist::IsDebugEnabled())
	{
		auto Local = [&](const FVector2D& Pixel)
		{
			FVector2D Desktop;
			USlateBlueprintLibrary::ScreenToWidgetAbsolute(this, Pixel, Desktop);
			return AllottedGeometry.AbsoluteToLocal(Desktop - FVector2D(Args.GetWindowToDesktopTransform()));
		};
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_SMALL_NUMBER);
        const FLinearColor Cyan(0.f, 1.f, 1.f);
		for (const auto& Outline : AimAssistDebug.Outlines)
		{
			if (Outline.Points.Num() < 3) continue;
			TArray<FVector2D> Points;
			for (const auto& Point : Outline.Points) Points.Add(Local(Point));
			Points.Add(Points[0]);
			const FLinearColor Color = Outline.bSelected ? FLinearColor::Green : FLinearColor::Yellow;
            if (Outline.bSelected && AimAssistDebug.ReleasePadding > 0.)
                FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(), Points,
                    ESlateDrawEffect::None, Cyan.CopyWithNewOpacity(.1f), true, 2. * AimAssistDebug.ReleasePadding / Scale);
			if (AimAssistDebug.Padding > 0.)
				FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, Color.CopyWithNewOpacity(.15f), true, 2. * AimAssistDebug.Padding / Scale);
			FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2, AllottedGeometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Color, true, 1.f / Scale);
		}
		if (AimAssistDebug.bHasDirections)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 3, AllottedGeometry.ToPaintGeometry(),
				TArray<FVector2D>{Local(AimAssistDebug.Muzzle), Local(AimAssistDebug.RawAim)}, ESlateDrawEffect::None, FLinearColor::Red, true, 1.f / Scale);
			FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 4, AllottedGeometry.ToPaintGeometry(),
				TArray<FVector2D>{Local(AimAssistDebug.Muzzle), Local(AimAssistDebug.CombatAim)}, ESlateDrawEffect::None, FLinearColor::Green, true, 1.f / Scale);
		}
        if (AimAssistDebug.bHasSnapCenter)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 4, AllottedGeometry.ToPaintGeometry(),
                TArray<FVector2D>{Local(AimAssistDebug.RawCursor), Local(AimAssistDebug.DisplayCursor), Local(AimAssistDebug.SnapCenter)},
                ESlateDrawEffect::None, Cyan, true, 1.f / Scale);
        }
		BaseLayer += 5;
	}
	if (!bDrawNativeCrosshair)
	{
		return BaseLayer;
	}
	LayerId = BaseLayer;
	// Screen conversion returns desktop coordinates; paint geometry is window-relative.
	FVector2D DesktopCenter;
	USlateBlueprintLibrary::ScreenToWidgetAbsolute(this, ScreenPosition, DesktopCenter);
	const FVector2D PaintCenter = DesktopCenter - FVector2D(Args.GetWindowToDesktopTransform());
	const FVector2D LocalCenter = AllottedGeometry.AbsoluteToLocal(PaintCenter);
	const float Gap = bDistanceDisplayActive
		? CurrentDistanceGap / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_SMALL_NUMBER)
		: FMath::Max(0.0f, BaseGap + (MaxSpreadGap - BaseGap) * FMath::Clamp(Spread, 0.0f, 1.0f));
	const float Length = FMath::Max(0.0f, LineLength);
	const float InnerThickness = FMath::Max(0.0f, LineThickness);
	const float OuterThickness = FMath::Max(InnerThickness, OutlineThickness);

	const TArray<TPair<FVector2D, FVector2D>> Segments = {
		{LocalCenter + FVector2D(-Gap - Length, 0.0f), LocalCenter + FVector2D(-Gap, 0.0f)},
		{LocalCenter + FVector2D(Gap, 0.0f), LocalCenter + FVector2D(Gap + Length, 0.0f)},
		{LocalCenter + FVector2D(0.0f, -Gap - Length), LocalCenter + FVector2D(0.0f, -Gap)},
		{LocalCenter + FVector2D(0.0f, Gap), LocalCenter + FVector2D(0.0f, Gap + Length)}
	};

	for (const TPair<FVector2D, FVector2D>& Segment : Segments)
	{
		TArray<FVector2D> Points;
		Points.Reserve(2);
		Points.Add(Segment.Key);
		Points.Add(Segment.Value);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, OutlineColor * InWidgetStyle.GetColorAndOpacityTint(), true, OuterThickness);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, CrosshairColor * InWidgetStyle.GetColorAndOpacityTint(), true, InnerThickness);
	}

	return LayerId + 2;
}

#if WITH_EDITOR
EDataValidationResult UCandyFightCrosshairWidget::IsDataValid(FDataValidationContext& Context) const
{
	const auto Parent = Super::IsDataValid(Context);
	if (!FMath::IsFinite(DistanceMinGap) || DistanceMinGap < 0.f ||
		!FMath::IsFinite(DistanceMaxGap) || DistanceMaxGap < DistanceMinGap ||
		!FMath::IsFinite(DistanceSpreadMultiplier) || DistanceSpreadMultiplier < 0.f ||
		!FMath::IsFinite(DistanceSpreadSmoothSeconds) || DistanceSpreadSmoothSeconds < 0.f)
	{
		Context.AddError(FText::FromString(TEXT("准星距离扩散参数必须有限且非负，最大间距不得小于最小间距。")));
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
