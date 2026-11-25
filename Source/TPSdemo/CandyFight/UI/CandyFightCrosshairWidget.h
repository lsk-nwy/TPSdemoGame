#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFight/UI/CandyFightDistanceSpread.h"
#include "CandyFight/CandyFightAimAssist.h"
#include "CandyFightCrosshairWidget.generated.h"

/** Names refer to Designer widgets; direction is an outward offset from their authored positions. */
USTRUCT(BlueprintType)
struct FCandyFightCrosshairSpreadBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair")
	FName WidgetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair")
	FVector2D Direction = FVector2D::ZeroVector;
};

/**
 * Screen-space crosshair for CandyFight.
 * Blueprint visuals belong inside a centered Canvas child named CrosshairVisual.
 * Native code moves that group; the Designer and Blueprint events own its appearance.
 */
UCLASS()
class TPSDEMO_API UCandyFightCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCandyFightCrosshairWidget(const FObjectInitializer& ObjectInitializer);

	/** Update the virtual crosshair center in viewport pixels. */
	UFUNCTION(BlueprintCallable, Category="CandyFight|Crosshair")
	void SetScreenPosition(const FVector2D& InScreenPosition);
	void SetAimViewRect(const FIntRect& InRect);
	bool GetAimClipRect(const FPaintArgs& Args, FSlateRect& OutRect) const;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Set normalized spread, where 0 is the base gap and 1 is the maximum gap. */
	UFUNCTION(BlueprintCallable, Category="CandyFight|Crosshair")
	void SetSpread(float InSpread);
	void RecordAimProjection(const FMatrix& ViewProjection, uint64 Frame);
	void SetAimAssistDebug(const FCandyFightAimAssistDebug& Debug);
	void UpdateWeaponSpread(const FVector& Muzzle, const FVector& Aim, float HalfAngleDegrees,
		bool bValidAim, double Now, uint64 Frame);
	/** Display-only settings, stored in the crosshair Widget Blueprint defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Distance Spread")
	bool bEnableDistanceSpread = true;
	/** 最近处的间距（物理像素）；Designer 以 Base Gap 为原始布局间距参考。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Distance Spread", meta=(ClampMin="0"))
	float DistanceMinGap = 6.f;
	/** 最远处的间距上限（物理像素），不小于最小间距。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Distance Spread", meta=(ClampMin="0"))
	float DistanceMaxGap = 96.f;
	/** 散布投影的显示倍率；只改变准星，不改变弹道。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Distance Spread", meta=(ClampMin="0"))
	float DistanceSpreadMultiplier = 1.f;
	/** 达到目标变化约 95% 的游戏时间；0 为立即同步，准星中心不平滑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Distance Spread", meta=(ClampMin="0", Units="s"))
	float DistanceSpreadSmoothSeconds = .06f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Crosshair|State")
	float CurrentDistanceGap = 6.f;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Drawing radius query retained for callers; no longer limits the crosshair center. */
	float GetMaxDrawExtent() const;

	/** Optional four-line fallback. Disable when using Designer visuals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Style")
	bool bDrawNativeCrosshair = true;

	/** Largest visual radius, including spread/animations, in Designer (Slate) units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout", meta=(ClampMin="0"))
	float VisualMaxDrawExtent = 32.0f;

	/** Treat Designer units as physical pixels and snap the visual center to a whole pixel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout")
	bool bPixelPerfectVisual = true;

	/** Add this group directly beneath the full-screen root Canvas in the Designer. */
	UPROPERTY(BlueprintReadOnly, Category="Crosshair|Layout", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CrosshairVisual;

	/** Disable when Blueprint events or animations entirely control the spread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Spread")
	bool bApplyDesignerSpread = true;

	/** Maximum outward movement from the layout authored in the Designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Spread", meta=(ClampMin="0"))
	float VisualSpreadDistance = 18.0f;

	/** Move a line and its outline in the same direction to keep them aligned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Spread")
	TArray<FCandyFightCrosshairSpreadBinding> SpreadBindings;

	/** Called on construction and whenever normalized spread changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="CandyFight|Crosshair")
	void OnCrosshairSpreadChanged(float NewSpread);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout", meta=(ClampMin="0"))
	float BaseGap = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout", meta=(ClampMin="0"))
	float MaxSpreadGap = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout", meta=(ClampMin="0"))
	float LineLength = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Layout", meta=(ClampMin="0"))
	float LineThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Style")
	FLinearColor CrosshairColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Style")
	FLinearColor OutlineColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crosshair|Style", meta=(ClampMin="0"))
	float OutlineThickness = 3.5f;

	UPROPERTY(BlueprintReadOnly, Category="Crosshair|State")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	/** Initial spread; also previews the configured Designer bindings. Runtime changes use SetSpread. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crosshair|Spread", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float Spread = 0.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FCandyFightAimAssistDebug AimAssistDebug;
	FMatrix AimViewProjection = FMatrix::Identity;
	uint64 AimProjectionFrame = MAX_uint64;
	FCandyFightDistanceSpreadState DistanceState;
	bool bDistanceDisplayActive = false;
	void ApplySpreadValue(float InSpread);
	FIntRect AimViewRect = FIntRect(0, 0, 0, 0);
	TSharedPtr<SWidget> ClipWidget;
	TMap<TWeakObjectPtr<UWidget>, FVector2D> InitialSpreadTranslations;
	void UpdateSpreadVisuals();
	void UpdateVisualPosition();
};
