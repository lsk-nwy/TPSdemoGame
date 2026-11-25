#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightWeaponHUD.generated.h"

class UCandyFightCombatComponent;
class UTextBlock;
class UProgressBar;
class ACandyFightGameMode;
class ACandyFightCharacter;
class ACandyFightPlayerState;

UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightWeaponHUD : public UUserWidget
{
    GENERATED_BODY()
public:
    void Connect(UCandyFightCombatComponent* Source);
    /** Display only; does not change the character's health. */
    UFUNCTION(BlueprintCallable, Category="HUD|Health")
    void SetHealthDisplay(float Current, float Max);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Health", meta=(ClampMin="0", Units="s"))
    float DamageFlashDuration = .15f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Health")
    FLinearColor DamageFlashColor = FLinearColor(1.f, .55f, .6f, 1.f);

    // Shared by the native fallback and the one-time Designer asset setup.
    static void BuildDefaultLayout(UWidgetTree* Tree);
    static void AddCurrencyLayout(UWidgetTree* Tree);
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD|Gold") TObjectPtr<UTextBlock> GoldText;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Gold") FText GoldFormat = NSLOCTEXT("CandyFight", "Gold", "金币 {Gold}");
    static FText FormatCountdown(float Seconds);
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Weapon HUD") TObjectPtr<UTextBlock> WeaponName;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Weapon HUD") TObjectPtr<UTextBlock> AmmoText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Weapon HUD") TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Weapon HUD") TObjectPtr<UProgressBar> ReloadProgress;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD|Health") TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD|Health") TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="HUD|Countdown") TObjectPtr<UTextBlock> CountdownText;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    friend class FCandyFightPlayerHealthHUDTest;
    UPROPERTY(Transient) TObjectPtr<UCandyFightCombatComponent> Combat;
    TWeakObjectPtr<ACandyFightCharacter> HealthSource;
    TWeakObjectPtr<ACandyFightPlayerState> GoldSource;
    void RefreshGoldSource();
    UFUNCTION() void SetGoldDisplay(int32 Gold);
    FLinearColor HealthBarOriginalColor;
    double HealthFlashEndTime = 0.;
    bool bHealthFlashActive = false;
    void UpdateHealthFlash();
    void RestoreHealthBarColor();
    UFUNCTION() void HandlePlayerDamaged(float AppliedDamage);
    TWeakObjectPtr<ACandyFightGameMode> CountdownSource;
    float DisplayHealth = 100.f;
    float DisplayMaxHealth = 100.f;
    int32 LastDisplayedSecond = INDEX_NONE;
    void RefreshHealth();
    void RefreshCountdown();
    UFUNCTION() void Refresh();
};
