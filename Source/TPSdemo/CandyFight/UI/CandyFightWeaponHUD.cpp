#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFight/CandyFightGameMode.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

TSharedRef<SWidget> UCandyFightWeaponHUD::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
        WeaponName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("WeaponName")));
        AmmoText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AmmoText")));
        StatusText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("StatusText")));
        ReloadProgress = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ReloadProgress")));
        HealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("HealthBar")));
        HealthText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("HealthText")));
        CountdownText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CountdownText")));
        GoldText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("GoldText")));
    }
    return Super::RebuildWidget();
}

void UCandyFightWeaponHUD::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    Tree->RootWidget = Canvas;
    const auto Panel = [&](const TCHAR* Name, FVector2D Anchor, FVector2D Position, FVector2D Size)
    {
        auto* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.TintColor = FLinearColor::White;
        Border->SetBrush(Brush);
        Border->SetBrushColor(FLinearColor(.025f, .035f, .05f, .85f));
        Border->SetPadding(FMargin(16.f));
        auto* Slot = Canvas->AddChildToCanvas(Border);
        Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
        Slot->SetAlignment(Anchor);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        return Border;
    };
    const auto Text = [&](const TCHAR* Name, int32 Size, const TCHAR* Preview)
    {
        auto* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Font = Block->GetFont();
        Font.Size = Size;
        Block->SetFont(Font);
        Block->SetColorAndOpacity(FSlateColor(FLinearColor(.95f, .97f, 1.f)));
        Block->SetText(FText::FromString(Preview));
        return Block;
    };
    const auto Bar = [&](const TCHAR* Name, FLinearColor Color, float Percent, float Height)
    {
        auto* Progress = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
        FProgressBarStyle Style;
        Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
        Style.BackgroundImage.TintColor = FLinearColor(.08f, .10f, .13f);
        Style.FillImage.DrawAs = ESlateBrushDrawType::Box;
        Style.FillImage.TintColor = FLinearColor::White;
        Progress->SetWidgetStyle(Style);
        Progress->SetBarFillStyle(EProgressBarFillStyle::Scale);
        Progress->SetFillColorAndOpacity(Color);
        Progress->SetPercent(Percent);
        auto* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(FString(Name) + TEXT("Size")));
        Box->SetHeightOverride(Height);
        Box->SetContent(Progress);
        return Box;
    };
    auto* Weapon = Panel(TEXT("WeaponPanel"), FVector2D(1., 1.), FVector2D(-28., -28.), FVector2D(320., 174.));
    auto* WeaponBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WeaponContents"));
    Weapon->SetContent(WeaponBox);
    WeaponBox->AddChildToVerticalBox(Text(TEXT("WeaponName"), 18, TEXT("自动步枪")));
    WeaponBox->AddChildToVerticalBox(Text(TEXT("AmmoText"), 28, TEXT("30 / 30")))->SetPadding(FMargin(0.f, 3.f));
    auto* Status = Text(TEXT("StatusText"), 14, TEXT("R 装填 · 空弹自动装填"));
    Status->SetAutoWrapText(true);
    Status->SetColorAndOpacity(FSlateColor(FLinearColor(.65f, .7f, .76f)));
    WeaponBox->AddChildToVerticalBox(Status);
    WeaponBox->AddChildToVerticalBox(Bar(TEXT("ReloadProgress"), FLinearColor(.25f, .8f, .7f), .65f, 6.f))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

    auto* Health = Panel(TEXT("HealthPanel"), FVector2D(0., 1.), FVector2D(28., -28.), FVector2D(300., 110.));
    auto* HealthBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HealthContents"));
    Health->SetContent(HealthBox);
    HealthBox->AddChildToVerticalBox(Text(TEXT("HealthLabel"), 14, TEXT("生命")));
    HealthBox->AddChildToVerticalBox(Text(TEXT("HealthText"), 22, TEXT("100 / 100")))->SetPadding(FMargin(0.f, 3.f));
    HealthBox->AddChildToVerticalBox(Bar(TEXT("HealthBar"), FLinearColor(.85f, .08f, .12f), 1.f, 12.f))->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));

    auto* Timer = Panel(TEXT("TimerPanel"), FVector2D(.5, 0.), FVector2D(0., 28.), FVector2D(160., 64.));
    Timer->SetPadding(FMargin(8.f));
    Timer->SetHorizontalAlignment(HAlign_Center);
    Timer->SetVerticalAlignment(VAlign_Center);
    auto* Countdown = Text(TEXT("CountdownText"), 32, TEXT("04:00"));
    Countdown->SetJustification(ETextJustify::Center);
    Timer->SetContent(Countdown);
    AddCurrencyLayout(Tree);
}

void UCandyFightWeaponHUD::AddCurrencyLayout(UWidgetTree* Tree)
{
    auto* Canvas = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
    if (!Canvas || Tree->FindWidget(TEXT("GoldText"))) return;
    auto* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("CurrencyPanel"));
    Panel->SetBrushColor(FLinearColor(.025f,.035f,.05f,.85f)); Panel->SetPadding(FMargin(12.f));
    auto* Slot = Canvas->AddChildToCanvas(Panel); Slot->SetAnchors(FAnchors(1,0)); Slot->SetAlignment(FVector2D(1,0));
    Slot->SetPosition(FVector2D(-28,28)); Slot->SetSize(FVector2D(180,52));
    auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("GoldText"));
    auto Font = Text->GetFont(); Font.Size = 20; Text->SetFont(Font);
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(1.f,.8f,.25f))); Text->SetText(FText::FromString(TEXT("金币 0")));
    Panel->SetContent(Text);
}
void UCandyFightWeaponHUD::SetGoldDisplay(int32 Gold)
{
    if (GoldText) { FFormatNamedArguments Args; Args.Add(TEXT("Gold"),Gold); GoldText->SetText(FText::Format(GoldFormat,Args)); }
}
void UCandyFightWeaponHUD::RefreshGoldSource()
{
    auto* Player = Combat ? Cast<ACandyFightCharacter>(Combat->GetOwner()) : nullptr;
    auto* Wallet = Player ? Player->GetPlayerState<ACandyFightPlayerState>() : nullptr;
    if (GoldSource.Get() != Wallet)
    {
        if (GoldSource.IsValid()) GoldSource->OnGoldChanged.RemoveDynamic(this,&ThisClass::SetGoldDisplay);
        GoldSource = Wallet;
        if (Wallet) Wallet->OnGoldChanged.AddUniqueDynamic(this,&ThisClass::SetGoldDisplay);
    }
    SetGoldDisplay(Wallet ? Wallet->Gold : 0);
}

FText UCandyFightWeaponHUD::FormatCountdown(float Seconds)
{
    const int32 WholeSeconds = FMath::IsFinite(Seconds) ? FMath::CeilToInt(FMath::Clamp(Seconds, 0.f, 2147483520.f)) : 0;
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), WholeSeconds / 60, WholeSeconds % 60));
}

void UCandyFightWeaponHUD::SetHealthDisplay(float Current, float Max)
{
    DisplayMaxHealth = FMath::IsFinite(Max) ? FMath::Max(0.f, Max) : 0.f;
    DisplayHealth = FMath::IsFinite(Current) ? FMath::Clamp(Current, 0.f, DisplayMaxHealth) : 0.f;
    RefreshHealth();
}

void UCandyFightWeaponHUD::RefreshHealth()
{
    if (HealthBar) HealthBar->SetPercent(DisplayMaxHealth > 0.f ? DisplayHealth / DisplayMaxHealth : 0.f);
    if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), DisplayHealth, DisplayMaxHealth)));
}

void UCandyFightWeaponHUD::RefreshCountdown()
{
    if (!CountdownSource.IsValid() && GetWorld()) CountdownSource = GetWorld()->GetAuthGameMode<ACandyFightGameMode>();
    if (!CountdownText) return;
    if (!CountdownSource.IsValid())
    {
        CountdownText->SetText(FText::FromString(TEXT("--:--")));
        LastDisplayedSecond = INDEX_NONE;
        return;
    }
    const float Remaining = CountdownSource->GetRemainingCountdownSeconds();
    const int32 Second = FMath::CeilToInt(FMath::Clamp(Remaining, 0.f, 2147483520.f));
    if (Second != LastDisplayedSecond)
    {
        CountdownText->SetText(FormatCountdown(Remaining));
        LastDisplayedSecond = Second;
    }
}

void UCandyFightWeaponHUD::Connect(UCandyFightCombatComponent* Source)
{
    RestoreHealthBarColor();
    if (HealthSource.IsValid())
    {
        HealthSource->OnHealthChanged.RemoveDynamic(this, &ThisClass::SetHealthDisplay);
        HealthSource->OnPlayerDamaged.RemoveDynamic(this, &ThisClass::HandlePlayerDamaged);
    }
    HealthSource.Reset();
    if (Combat)
    {
        Combat->OnAmmoChanged.RemoveDynamic(this, &ThisClass::Refresh);
        Combat->OnWeaponChanged.RemoveDynamic(this, &ThisClass::Refresh);
        Combat->OnReloadChanged.RemoveDynamic(this, &ThisClass::Refresh);
        Combat->OnElementChanged.RemoveDynamic(this, &ThisClass::Refresh);
    }
    Combat = Source;
    if (Combat)
    {
        Combat->OnAmmoChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
        Combat->OnWeaponChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
        Combat->OnReloadChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
        Combat->OnElementChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
        HealthSource = Cast<ACandyFightCharacter>(Combat->GetOwner());
        if (HealthSource.IsValid())
        {
            HealthSource->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::SetHealthDisplay);
            HealthSource->OnPlayerDamaged.AddUniqueDynamic(this, &ThisClass::HandlePlayerDamaged);
            SetHealthDisplay(HealthSource->Health, HealthSource->MaxHealth);
        }
    }
    Refresh();
    RefreshGoldSource();
}
void UCandyFightWeaponHUD::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    Refresh();
    if (HealthSource.IsValid()) SetHealthDisplay(HealthSource->Health, HealthSource->MaxHealth);
    RefreshHealth();
    RefreshGoldSource();
    LastDisplayedSecond = INDEX_NONE;
    RefreshCountdown();
}
void UCandyFightWeaponHUD::NativeDestruct()
{
    Connect(nullptr);
    Super::NativeDestruct();
}
void UCandyFightWeaponHUD::Refresh()
{
    const auto* Config = Combat ? Combat->GetCurrentConfig() : nullptr;
    if (WeaponName) WeaponName->SetText(Config ? Config->DisplayName : FText::FromString(TEXT("未装备武器")));
    if (AmmoText) AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Combat ? Combat->GetAmmo() : 0, Config ? Config->MagazineCapacity : 0)));
    const bool bReload = Combat && Combat->IsReloading();
    const bool bShellReload = bReload && Config && Config->FireMode == ECandyFightFireMode::Shotgun;
    const TCHAR* Status = Combat && Combat->IsDryFiring() ? TEXT("空弹 · 松开左键或按 R 装填")
        : Combat && Combat->GetReloadPhase() == ECandyFightReloadPhase::ShellStart ? TEXT("装填准备…") : bShellReload
        ? (Combat->GetAmmo() > 0 ? TEXT("逐发装填 · 左键开火打断") : TEXT("逐发装填 · 等待首发"))
        : bReload ? TEXT("装填中…") : TEXT("Q 切元素 · R 装填 · 空弹自动装填");
    const ECandyFightElement Element = Combat ? Combat->GetElement() : ECandyFightElement::None;
    if (StatusText)
    {
        // 元素前缀保证 Designer 版 HUD 不需要新增控件也能看到当前元素。
        const FString Line = Element != ECandyFightElement::None
            ? FString::Printf(TEXT("%s · %s"), *CandyFightElements::GetElementName(Element).ToString(), Status)
            : FString(Status);
        StatusText->SetText(FText::FromString(Line));
        StatusText->SetColorAndOpacity(FSlateColor(Element != ECandyFightElement::None
            ? CandyFightElements::GetElementColor(Element) : FLinearColor(.65f, .7f, .76f)));
    }
    if (ReloadProgress)
    {
        ReloadProgress->SetVisibility(bReload ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        ReloadProgress->SetPercent(bReload ? Combat->GetReloadProgress() : 0.f);
    }
}
void UCandyFightWeaponHUD::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    UpdateHealthFlash();
    RefreshGoldSource();
    RefreshCountdown();
    if (Combat && Combat->IsReloading() && ReloadProgress) ReloadProgress->SetPercent(Combat->GetReloadProgress());
}

void UCandyFightWeaponHUD::HandlePlayerDamaged(float AppliedDamage)
{
    if (!HealthBar || !GetWorld() || AppliedDamage <= 0.f) return;
    if (!FMath::IsFinite(DamageFlashDuration) || DamageFlashDuration <= 0.f)
    {
        RestoreHealthBarColor();
        return;
    }
    // Repeated hits extend the flash without replacing the Designer-authored base color.
    if (!bHealthFlashActive) HealthBarOriginalColor = HealthBar->GetFillColorAndOpacity();
    bHealthFlashActive = true;
    HealthFlashEndTime = GetWorld()->GetTimeSeconds() + DamageFlashDuration;
    HealthBar->SetFillColorAndOpacity(DamageFlashColor);
}

void UCandyFightWeaponHUD::UpdateHealthFlash()
{
    if (bHealthFlashActive && (!HealthSource.IsValid() || !GetWorld() ||
        GetWorld()->GetTimeSeconds() >= HealthFlashEndTime)) RestoreHealthBarColor();
}

void UCandyFightWeaponHUD::RestoreHealthBarColor()
{
    if (bHealthFlashActive && HealthBar) HealthBar->SetFillColorAndOpacity(HealthBarOriginalColor);
    bHealthFlashActive = false;
    HealthFlashEndTime = 0.;
}
