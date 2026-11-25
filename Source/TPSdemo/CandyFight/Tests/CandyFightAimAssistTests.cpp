#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/CandyFightAimAssist.h"
#include "CandyFight/CandyFightAimFraming.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightCameraConfig.h"
#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
    FSceneViewProjectionData MakeAimProjection(const FIntRect& Rect = FIntRect(0,0,1920,1080),
        double Yaw = 0., double Pitch = -55.)
    {
        FMinimalViewInfo View;
        View.Rotation = FRotator(Pitch, Yaw, 0.);
        View.Location = FVector(500,0,88) - View.Rotation.Vector() * 2000.;
        View.FOV = 90.f;
        FSceneViewProjectionData Projection;
        Projection.SetViewRectangle(Rect);
        FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, AspectRatio_MaintainXFOV, Rect, Projection);
        Projection.ViewOrigin = View.Location;
        Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
            FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
        return Projection;
    }

    FVector2D Pixel(const FSceneViewProjectionData& Projection, const FVector& Point)
    {
        FVector2D Result;
        FSceneView::ProjectWorldToScreen(Point, Projection.GetConstrainedViewRect(), Projection.ComputeViewProjectionMatrix(), Result);
        return Result;
    }

    struct FAimWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        AActor* Shooter = World->SpawnActor<AActor>();
        UCandyFightWeaponConfig* Config = NewObject<UCandyFightWeaponConfig>();
        FVector Muzzle = FVector(0,0,108);
        FAimWorld()
        {
            Config->WeaponClass = ACandyFightWeapon::StaticClass();
            Config->bEnableAimAssist = true;
            Config->BaseSpread = Config->MaxSpread = 0.f;
        }
        ~FAimWorld() { World->DestroyWorld(false); }
        ACandyFightTarget* Target(const FVector& Location = FVector(1000,0,88))
        {
            return World->SpawnActor<ACandyFightTarget>(Location, FRotator::ZeroRotator);
        }
        ACandyFightEnemyCharacter* Enemy(const FVector& Location = FVector(1000,0,88))
        {
            return World->SpawnActor<ACandyFightEnemyCharacter>(Location, FRotator::ZeroRotator);
        }
        AActor* Blocker(const FVector& Location, const FVector& Extent = FVector(30))
        {
            AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
            auto* Box = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Box);
            Box->SetBoxExtent(Extent);
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Actor;
        }
        bool Resolve(const FSceneViewProjectionData& Projection, const FVector2D& Cursor,
            CandyFightAimAssist::FCandidate& Candidate, AActor* Previous = nullptr)
        {
            return CandyFightAimAssist::Resolve(World, Shooter, nullptr, Muzzle, Projection, Cursor, *Config, Previous, Candidate);
        }
        bool Shoot(const CandyFightAimAssist::FCandidate& Candidate)
        {
            FHitResult Hit;
            FVector End;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightAimAssistTest), false, Shooter);
            UCandyFightCombatComponent::TracePellet(World, Muzzle, (Candidate.Aim - Muzzle).GetSafeNormal2D(), Config->Range, Params, Hit, End);
            return Hit.bBlockingHit && Hit.GetActor() == Candidate.Actor.Get();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistConfigTest, "CandyFight.Aiming.Assist.Configuration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistConfigTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    FString Error;
    TestFalse(TEXT("New definitions opt in"), Config->bEnableAimAssist);
    TestTrue(TEXT("Defaults valid"), Config->ValidateConfiguration(Error));
    Config->AimAssistPaddingPixels = -1.f;
    TestFalse(TEXT("Negative padding rejected even when disabled"), Config->ValidateConfiguration(Error));
    Config->AimAssistPaddingPixels = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN padding rejected"), Config->ValidateConfiguration(Error));
    Config->AimAssistPaddingPixels = 0.f;
    Config->AimAssistSwitchBiasPixels = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Infinite bias rejected"), Config->ValidateConfiguration(Error));
    Config->AimAssistSwitchBiasPixels = -1.f;
    TestFalse(TEXT("Negative bias rejected"), Config->ValidateConfiguration(Error));
    Config->AimAssistSwitchBiasPixels = 0.f;
    TestTrue(TEXT("Zero tolerance and bias valid"), Config->ValidateConfiguration(Error));
    TestEqual(TEXT("Default release padding"), Config->AimAssistReleasePaddingPixels, 24.f);
    TestEqual(TEXT("Default 95 percent snap time"), Config->AimAssistSnapSeconds, .12f);
    for (float Invalid : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        Config->AimAssistSnapSeconds = Invalid;
        TestFalse(TEXT("Invalid snap time rejected"), Config->ValidateConfiguration(Error));
        Config->AimAssistSnapSeconds = 0.f;
        Config->AimAssistReleasePaddingPixels = Invalid;
        TestFalse(TEXT("Invalid release padding rejected"), Config->ValidateConfiguration(Error));
        Config->AimAssistReleasePaddingPixels = 0.f;
    }
    TestTrue(TEXT("Instant snap and no extra hold range valid"), Config->ValidateConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistBodyTest, "CandyFight.Aiming.Assist.BodiesAndProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistBodyTest::RunTest(const FString& Parameters)
{
    FAimWorld Env;
    auto* Target = Env.Target();
    auto* Enemy = Env.Enemy(FVector(1000,300,88));
    for (double Yaw : {0., 73., -90., 180.})
    for (double Pitch : {-35., -55., -75.})
    for (const FIntRect& Rect : {FIntRect(0,0,1920,1080), FIntRect(80,120,1360,840), FIntRect(0,0,960,540)})
    for (AActor* Actor : {static_cast<AActor*>(Target), static_cast<AActor*>(Enemy)})
    {
        const auto Projection = MakeAimProjection(Rect, Yaw, Pitch);
        auto* Body = CandyFightAimAssist::GetEligibleBody(Actor);
        TArray<FVector2D> Hull;
        FVector2D Center, Nearest;
        TestTrue(TEXT("Body projects at any tested yaw/pitch/viewport"), CandyFightAimAssist::ProjectBody(Body, Projection, Hull, Center));
        TestTrue(TEXT("Projected center inside convex silhouette"), CandyFightAimAssist::DistanceToHull(Hull, Center, Nearest) <= .001);
        for (double Height : {16., 88., 160.})
        {
            CandyFightAimAssist::FCandidate Candidate;
            const FVector2D Cursor = Pixel(Projection, FVector(Actor->GetActorLocation().X, Actor->GetActorLocation().Y, Height));
            TestTrue(TEXT("Top/middle/bottom select body"), Env.Resolve(Projection, Cursor, Candidate));
            TestTrue(TEXT("Selected body is the intended actor"), Candidate.Actor.Get() == Actor);
            TestTrue(TEXT("Corrected zero-spread direction hits body"), Env.Shoot(Candidate));
            TestEqual(TEXT("Correction remains at muzzle height"), Candidate.Aim.Z, Env.Muzzle.Z);
        }
    }
    FVector Point;
    TestFalse(TEXT("Enemy below muzzle plane rejected"), CandyFightAimAssist::HorizontalCenter(Enemy->GetCapsuleComponent(), 500., Point));
    TestFalse(TEXT("Box below muzzle plane rejected"), CandyFightAimAssist::HorizontalCenter(Target->HitBox, 500., Point));
    for (const FRotator Rotation : {FRotator(0,35,0), FRotator(35,65,25), FRotator(-50,-20,45)})
    {
        Target->SetActorRotation(Rotation);
        Target->SetActorScale3D(FVector(1.5,.7,1.2));
        Target->HitBox->SetBoxExtent(FVector(55,30,110));
        TestTrue(TEXT("Rotated/scaled box has a horizontal section"), CandyFightAimAssist::HorizontalCenter(Target->HitBox, 120., Point));
        TestEqual(TEXT("Section centroid has requested Z"), Point.Z, 120.);
        const FVector Local = Target->HitBox->GetComponentTransform().InverseTransformPosition(Point);
        TestTrue(TEXT("Section centroid inside local box"), Local.GetAbs().X <= 55.001 && Local.GetAbs().Y <= 30.001 && Local.GetAbs().Z <= 110.001);
        Env.Muzzle.Z = 120.;
        CandyFightAimAssist::FCandidate Candidate;
        const auto Projection = MakeAimProjection();
        TestTrue(TEXT("Rotated/scaled box selectable"), Env.Resolve(Projection, Pixel(Projection, Target->GetActorLocation()), Candidate));
        TestTrue(TEXT("Rotated/scaled box shot hits"), Env.Shoot(Candidate));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistPaddingTest, "CandyFight.Aiming.Assist.PaddingAndLabels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistPaddingTest::RunTest(const FString& Parameters)
{
    FAimWorld Env;
    auto* Target = Env.Target();
    for (const FIntRect& Rect : {FIntRect(0,0,1920,1080), FIntRect(70,90,1350,810), FIntRect(0,0,3840,2160)})
    {
        const auto Projection = MakeAimProjection(Rect);
        TArray<FVector2D> Hull;
        FVector2D Center;
        CandyFightAimAssist::ProjectBody(Target->HitBox, Projection, Hull, Center);
        const FVector2D A = Hull[0], B = Hull[1];
        const FVector2D Edge = (B - A).GetSafeNormal();
        const FVector2D Outward(Edge.Y, -Edge.X);
        const FVector2D Middle = (A + B) * .5;
        CandyFightAimAssist::FCandidate Candidate;
        const double Scale = Rect.Height() / 1080.;
        Env.Config->AimAssistPaddingPixels = 8.f;
        const FVector2D HoldCursor = Middle + Outward * 20. * Scale;
        TestFalse(TEXT("Hold-only region cannot acquire a new target"), Env.Resolve(Projection, HoldCursor, Candidate));
        TestTrue(TEXT("Current target can remain inside enlarged hold region"), Env.Resolve(Projection, HoldCursor, Candidate, Target));
        TestFalse(TEXT("Outside hold region releases current target"), Env.Resolve(Projection, Middle + Outward * 32.1 * Scale, Candidate, Target));
        TestFalse(TEXT("Explicitly excluded target cannot reacquire"), CandyFightAimAssist::Resolve(Env.World, Env.Shooter, nullptr,
            Env.Muzzle, Projection, Center, *Env.Config, nullptr, Candidate, nullptr, Target));
        Env.Config->AimAssistReleasePaddingPixels = 0.f;
        TestFalse(TEXT("Zero release padding uses acquisition boundary"), Env.Resolve(Projection, HoldCursor, Candidate, Target));
        Env.Config->AimAssistReleasePaddingPixels = 24.f;
        TestTrue(TEXT("Inside scaled padding accepted"), Env.Resolve(Projection, Middle + Outward * 7.9 * Scale, Candidate));
        TestFalse(TEXT("Padding candidate is not direct"), Candidate.bDirect);
        TestFalse(TEXT("Outside scaled padding rejected"), Env.Resolve(Projection, Middle + Outward * 8.1 * Scale, Candidate));
        Env.Config->AimAssistPaddingPixels = 0.f;
        TestFalse(TEXT("Zero padding rejects near miss"), Env.Resolve(Projection, Middle + Outward * Scale, Candidate));
        TestTrue(TEXT("Zero padding accepts body"), Env.Resolve(Projection, Center, Candidate));
        Env.Config->AimAssistPaddingPixels = 8.f;
        // Put the label beyond the permitted padding; display components must not enlarge the body.
        Target->Label->SetRelativeLocation(FVector(0,0,300));
        Target->Label->SetWorldSize(250.f);
        TestFalse(TEXT("HP label beyond body padding is not a target"), Env.Resolve(Projection, Pixel(Projection, Target->Label->GetComponentLocation()), Candidate));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistSelectionTest, "CandyFight.Aiming.Assist.StableSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistSelectionTest::RunTest(const FString& Parameters)
{
    FAimWorld Env;
    auto* A = Env.Target();
    auto* B = Env.Enemy(FVector(1000,200,88));
    CandyFightAimAssist::FCandidate First, Second;
    First.Actor = A; First.CenterDistance = 10.; First.CameraDistance = 100.;
    Second.Actor = B; Second.CenterDistance = 9.; Second.CameraDistance = 200.;
    TArray<CandyFightAimAssist::FCandidate> Candidates = {First, Second};
    TestEqual(TEXT("Initial acquisition uses screen distance, independent of type"), CandyFightAimAssist::Select(Candidates, nullptr, 4.), 1);
    TestEqual(TEXT("Small advantage keeps previous target"), CandyFightAimAssist::Select(Candidates, A, 4.), 0);
    Candidates[1].CenterDistance = 5.;
    TestEqual(TEXT("Clear advantage switches"), CandyFightAimAssist::Select(Candidates, A, 4.), 1);
    Candidates[0].bDirect = true;
    TestEqual(TEXT("Direct target beats padding target"), CandyFightAimAssist::Select(Candidates, B, 4.), 0);
    Candidates[1].bDirect = true;
    TestEqual(TEXT("Zero bias uses closest target immediately"), CandyFightAimAssist::Select(Candidates, A, 0.), 1);
    Candidates[1].CenterDistance = 10.;
    TestEqual(TEXT("Initial tie prefers camera-nearer body"), CandyFightAimAssist::Select(Candidates, nullptr, 4.), 0);
    Candidates.RemoveAt(0);
    TestEqual(TEXT("Missing previous target is released"), CandyFightAimAssist::Select(Candidates, A, 4.), 0);
    TestEqual(TEXT("No candidates yields no selection"), CandyFightAimAssist::Select({}, B, 4.), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistOcclusionTest, "CandyFight.Aiming.Assist.OcclusionAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistOcclusionTest::RunTest(const FString& Parameters)
{
    FAimWorld Env;
    auto* Target = Env.Target();
    const auto Projection = MakeAimProjection();
    const FVector2D Cursor = Pixel(Projection, Target->GetActorLocation());
    CandyFightAimAssist::FCandidate Candidate;
    TestTrue(TEXT("Unobstructed body selected"), Env.Resolve(Projection, Cursor, Candidate));
    const FVector Visible = Candidate.VisiblePoint;
    auto* CameraBlocker = Env.Blocker((Projection.ViewOrigin + Visible) * .5);
    FVector Aim;
    TestTrue(TEXT("Camera obstacle does not block horizontal bullet path"), CandyFightAimAssist::HasShotPath(Env.World, Env.Shooter, nullptr, Target, Env.Muzzle, Env.Config->Range, Aim));
    TestFalse(TEXT("Camera obstacle prevents acquisition"), Env.Resolve(Projection, Cursor, Candidate));
    CameraBlocker->Destroy();
    auto* ShotBlocker = Env.Blocker(FVector(500,0,108));
    TestTrue(TEXT("Bullet obstacle does not block camera visibility"), CandyFightAimAssist::HasVisiblePath(Env.World, Env.Shooter, nullptr, Target, Projection.ViewOrigin, Visible));
    TestFalse(TEXT("Visible but unshootable body rejected"), Env.Resolve(Projection, Cursor, Candidate));
    ShotBlocker->Destroy();
    auto* FrontTarget = Env.Target(FVector(500,0,88));
    TestFalse(TEXT("Another body blocks target's bullet path"), CandyFightAimAssist::HasShotPath(Env.World, Env.Shooter, nullptr, Target, Env.Muzzle, Env.Config->Range, Aim));
    FrontTarget->Destroy();
    Env.Config->Range = 900.f;
    TestFalse(TEXT("Beyond range rejected"), Env.Resolve(Projection, Cursor, Candidate));
    Env.Config->Range = 1000.f;
    TestTrue(TEXT("At center range accepted"), Env.Resolve(Projection, Cursor, Candidate));
    auto* MuzzleBlocker = Env.Blocker(Env.Muzzle);
    TestFalse(TEXT("Muzzle inside blocker rejected"), Env.Resolve(Projection, Cursor, Candidate));
    MuzzleBlocker->Destroy();
    Env.Muzzle.Z = 500.;
    TestFalse(TEXT("Height plane missing body rejected"), Env.Resolve(Projection, Cursor, Candidate));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistLifecycleTest, "CandyFight.Aiming.Assist.TargetLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistLifecycleTest::RunTest(const FString& Parameters)
{
    FAimWorld Env;
    auto* Target = Env.Target();
    const auto Projection = MakeAimProjection();
    const FVector2D Cursor = Pixel(Projection, Target->GetActorLocation());
    CandyFightAimAssist::FCandidate Candidate;
    TestTrue(TEXT("Live target selected"), Env.Resolve(Projection, Cursor, Candidate));
    Target->TakeDamage(200.f, FDamageEvent(), nullptr, Env.Shooter);
    TestFalse(TEXT("Defeated hidden target released"), Env.Resolve(Projection, Cursor, Candidate, Target));
    Target->RespawnSeconds = .01f;
    // Existing Tick uses game time; advance only the isolated world's clock.
    Env.World->TimeSeconds = 4.f;
    static_cast<AActor*>(Target)->Tick(0.f);
    TestTrue(TEXT("Existing respawn re-enables acquisition"), Env.Resolve(Projection, Cursor, Candidate));
    Target->SetActorEnableCollision(false);
    TestFalse(TEXT("Collision-disabled target rejected"), Env.Resolve(Projection, Cursor, Candidate));
    Target->SetActorEnableCollision(true);
    Target->SetCanBeDamaged(false);
    TestFalse(TEXT("Invulnerable target rejected"), Env.Resolve(Projection, Cursor, Candidate));
    Target->SetCanBeDamaged(true);
    Env.Config->bEnableAimAssist = false;
    TestFalse(TEXT("Weapon toggle disables correction"), Env.Resolve(Projection, Cursor, Candidate));
    Env.Config->bEnableAimAssist = true;
    Target->Destroy();
    TestFalse(TEXT("Destroyed target not acquired"), Env.Resolve(Projection, Cursor, Candidate));
    auto* Enemy = Env.Enemy();
    Enemy->bDefeated = true;
    TestNull(TEXT("Defeated enemy ineligible"), CandyFightAimAssist::GetEligibleBody(Enemy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistCombatTest, "CandyFight.Aiming.Assist.CombatIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistCombatTest::RunTest(const FString& Parameters)
{
    for (bool bEnemy : {false, true}) for (double Height : {80., 0., -80.})
    {
        FAimWorld Immediate;
        auto* Player = Immediate.World->SpawnActor<ACandyFightCharacter>(FVector(0,0,88), FRotator::ZeroRotator);
        auto* PC = Immediate.World->SpawnActor<APlayerController>();
        PC->Possess(Player);
        auto* Combat = Player->Combat.Get();
        Combat->WeaponConfigs = {Immediate.Config};
        Combat->WeaponStates.SetNum(1);
        Combat->WeaponStates[0].Initialize(*Immediate.Config, 0.);
        if (!TestTrue(TEXT("Immediate-assist weapon equips"), Combat->EquipTestWeapon(1))) continue;
        AActor* Target = bEnemy ? static_cast<AActor*>(Immediate.Enemy(FVector(1000,250,88))) :
            static_cast<AActor*>(Immediate.Target(FVector(1000,250,88)));
        const auto Projection = MakeAimProjection();
        Player->RawAimScreenPosition = Pixel(Projection, Target->GetActorLocation() + FVector(0,0,Height));
        const FVector2D RawPixel = Player->RawAimScreenPosition;
        Player->LastAimUpdateFrame = GFrameCounter;
        Player->UpdateCombatAim(Projection, GFrameCounter, .001f);
        if (!TestTrue(TEXT("Top/middle/bottom acquires enemy or test target immediately"), Player->bAimAssistActive)) continue;
        FVector Aim; uint64 Frame;
        Player->GetCombatAim(Aim, Frame);
        TestTrue(TEXT("First-frame combat aim already reaches horizontal center"),
            FVector2D(Aim).Equals(FVector2D(Target->GetActorLocation()), .001));
        TestEqual(TEXT("Immediate correction leaves raw cursor unchanged"), Player->RawAimScreenPosition, RawPixel);
        const FVector2D Display = Player->AimScreenPosition;
        if (Height != 0.) TestTrue(TEXT("Head/feet crosshair is still interpolating on the first shot"),
            (Display - Pixel(Projection, Aim)).Size() > 1.);
        const float BeforeHealth = bEnemy ? Cast<ACandyFightEnemyCharacter>(Target)->Health : Cast<ACandyFightTarget>(Target)->Health;
        Combat->FireShot(*Player, 0.);
        TestEqual(TEXT("Immediate shot commits"), Combat->TotalShotsFired, 1);
        TestEqual(TEXT("Firing does not advance attraction"), Player->AimScreenPosition, Display);
        Player->GetCombatAim(Aim, Frame);
        FVector Center;
        TestTrue(TEXT("Final muzzle has a valid target section"), CandyFightAimAssist::HorizontalCenter(
            CandyFightAimAssist::GetEligibleBody(Target), Combat->EquippedWeapon->Muzzle->GetComponentLocation().Z, Center));
        TestTrue(TEXT("Shot revalidation updates center for final muzzle height"), Aim.Equals(Center, .001));
        TestTrue(TEXT("Character faces the corrected point before firing"), FVector::DotProduct(Player->GetActorForwardVector(),
            (Aim - Player->GetActorLocation()).GetSafeNormal2D()) > .999);
        if (TestEqual(TEXT("Immediate shot has one base trajectory"), Combat->LastShotEnds.Num(), 1))
            TestTrue(TEXT("Gun aligns with corrected first-frame trajectory"), FVector::DotProduct(
                Combat->EquippedWeapon->GetActorForwardVector(), (Combat->LastShotEnds[0]-Combat->LastShotStart).GetSafeNormal2D()) > .999);
        for (TActorIterator<ACandyFightProjectileShot> It(Immediate.World); It; ++It) It->AdvanceTo(1.);
        const float AfterHealth = bEnemy ? Cast<ACandyFightEnemyCharacter>(Target)->Health : Cast<ACandyFightTarget>(Target)->Health;
        TestTrue(TEXT("First-frame head/middle/feet shot damages enemy or test target"), AfterHealth < BeforeHealth);
    }
    FAimWorld Env;
    auto* Player = Env.World->SpawnActor<ACandyFightCharacter>(FVector(0,0,88), FRotator::ZeroRotator);
    auto* PC = Env.World->SpawnActor<APlayerController>();
    PC->Possess(Player);
    auto* Combat = Player->Combat.Get();
    Combat->WeaponConfigs = {Env.Config};
    Combat->WeaponStates.SetNum(1);
    Combat->WeaponStates[0].Initialize(*Env.Config, 0.);
    TestTrue(TEXT("Player test weapon equipped"), Combat->EquipTestWeapon(1));
    auto* Target = Env.Target(FVector(1000,250,88));
    const auto Projection = MakeAimProjection();
    Player->RawAimScreenPosition = Pixel(Projection, Target->GetActorLocation() + FVector(0,0,80));
    CandyFightAimFraming::IntersectPlane(Projection, Player->RawAimScreenPosition,
        Combat->EquippedWeapon->Muzzle->GetComponentLocation().Z, Player->AimWorldLocation);
    const FVector Raw = Player->AimWorldLocation;
    TestTrue(TEXT("Test player has player controller"), Player->GetController() == PC);
    TestTrue(TEXT("Test weapon enables assist"), Combat->GetCurrentConfig()->bEnableAimAssist);
    CandyFightAimAssist::FCandidate InitialCandidate;
    TestTrue(TEXT("Player muzzle has valid horizontal target path"), CandyFightAimAssist::HasShotPath(Env.World, Player,
        Combat->EquippedWeapon, Target, Combat->EquippedWeapon->Muzzle->GetComponentLocation(), Env.Config->Range, InitialCandidate.Aim));
    TestTrue(TEXT("Player initial cursor has visible candidate"), CandyFightAimAssist::Evaluate(Env.World, Player,
        Combat->EquippedWeapon, Target, Combat->EquippedWeapon->Muzzle->GetComponentLocation(), Projection,
        Player->RawAimScreenPosition, *Env.Config, InitialCandidate));
    Player->bHasValidAim = true;
    Player->LastAimUpdateFrame = GFrameCounter;
    Player->UpdateCombatAim(Projection, GFrameCounter, .02f);
    TestTrue(TEXT("Player acquires target"), Player->bAimAssistActive);
    TestEqual(TEXT("Raw aim preserved"), Player->AimWorldLocation, Raw);
    FVector Aim; uint64 Frame;
    Player->GetCombatAim(Aim, Frame);
    const FVector2D Anchor = Pixel(Projection, FVector(Target->GetActorLocation().X, Target->GetActorLocation().Y, Aim.Z));
    TestTrue(TEXT("First update attracts without snapping instantly"), (Player->AimScreenPosition-Anchor).Size() > 1.);
    TestTrue(TEXT("Displayed cursor is closer to anchor than raw input"),
        (Player->AimScreenPosition-Anchor).Size() < (Player->RawAimScreenPosition-Anchor).Size());
    TestTrue(TEXT("Combat aim reaches center before the crosshair"), Aim.Equals(InitialCandidate.Aim, .001));
    const FVector2D Once = Player->AimScreenPosition;
    Player->UpdateCombatAim(Projection, GFrameCounter, 1.f);
    TestEqual(TEXT("Repeated camera evaluation does not advance snap"), Player->AimScreenPosition, Once);
    Combat->FireShot(*Player, 0.);
    TestEqual(TEXT("Shot during attraction commits once"), Combat->TotalShotsFired, 1);
    FVector ShotAim; uint64 ShotFrame;
    Player->GetCombatAim(ShotAim, ShotFrame);
    TestTrue(TEXT("Pre-shot checks do not advance displayed crosshair"), Player->AimScreenPosition.Equals(Once, .001));
    TestTrue(TEXT("Shot during attraction already aims at horizontal center"),
        FVector2D(ShotAim).Equals(FVector2D(Target->GetActorLocation()), .001));
    TestTrue(TEXT("Shot during attraction follows the corrected combat direction"),
        (Combat->LastShotEnds[0]-Combat->LastShotStart).GetSafeNormal2D().Equals((ShotAim-Combat->LastShotStart).GetSafeNormal2D(), .00001));
    for (TActorIterator<ACandyFightProjectileShot> It(Env.World); It; ++It) It->Destroy();
    Player->CrosshairWidget = NewObject<UCandyFightCrosshairWidget>(Player);
    Player->UpdateCombatAim(Projection, GFrameCounter + 1, .5f);
    Player->GetCombatAim(Aim, Frame);
    TestTrue(TEXT("Settled aim converges to horizontal center"), FVector2D(Aim).Equals(FVector2D(Target->GetActorLocation()), .001));
    TestEqual(TEXT("Widget follows displayed cursor"), Player->CrosshairWidget->ScreenPosition, Player->AimScreenPosition);
    TestEqual(TEXT("Raw input remains unchanged while snapping"), Player->AimWorldLocation, Raw);
    Player->SetActorRotation(FRotator(0., (Aim-Player->GetActorLocation()).Rotation().Yaw, 0.));
    Combat->FireShot(*Player, 1.);
    TestEqual(TEXT("Settled shot committed once"), Combat->TotalShotsFired, 2);
    TestEqual(TEXT("Two shots deduct two rounds"), Combat->GetAmmo(), Env.Config->MagazineCapacity - 2);
    TestTrue(TEXT("Gun matches launch direction"), FVector::DotProduct(Combat->EquippedWeapon->GetActorForwardVector(), (Combat->LastShotEnds[0]-Combat->LastShotStart).GetSafeNormal2D()) > .999);
    ACandyFightProjectileShot* Shot = nullptr;
    for (TActorIterator<ACandyFightProjectileShot> It(Env.World); It; ++It) Shot = *It;
    TestNotNull(TEXT("Flight instance created"), Shot);
    if (Shot)
    {
        const FVector Direction = Shot->GetPellets()[0].Direction;
        TestEqual(TEXT("Assisted projectile remains horizontal"), Direction.Z, 0.);
        Target->SetActorLocation(Target->GetActorLocation() + FVector(0,500,0));
        Shot->AdvanceTo(1.05);
        TestEqual(TEXT("Projectile does not track moving target"), Shot->GetPellets()[0].Direction, Direction);
        TestTrue(TEXT("Projectile still moves along launch direction"), Shot->GetPellets()[0].Position.Equals(Combat->LastShotStart + Direction * 250., .001));
        Player->RevalidateCombatAim();
        TestFalse(TEXT("Pre-shot revalidation releases body moved outside cursor region"), Player->bAimAssistActive);
    }
    Target->SetActorLocation(FVector(1000,250,88));
    Player->ResetAimAssist();
    Player->UpdateCombatAim(Projection, GFrameCounter + 2, .02f);
    Target->SetActorHiddenInGame(true);
    const FVector2D BeforeHidden = Player->AimScreenPosition;
    Player->UpdateCombatAim(Projection, GFrameCounter + 2, .02f);
    TestFalse(TEXT("Same-frame revalidation releases hidden target"), Player->bAimAssistActive);
    TestEqual(TEXT("Hidden target release does not jump crosshair"), Player->AimScreenPosition, BeforeHidden);
    Target->SetActorHiddenInGame(false);
    Player->UpdateCombatAim(Projection, GFrameCounter + 2, .02f);
    TestFalse(TEXT("Repeated camera evaluation cannot reacquire within frame"), Player->bAimAssistActive);
    Player->UpdateCombatAim(Projection, GFrameCounter + 3, .02f);
    TestTrue(TEXT("Next frame can reacquire"), Player->bAimAssistActive);
    Env.Config->bEnableAimAssist = false;
    const FVector2D BeforeOff = Player->AimScreenPosition;
    Player->RevalidateCombatAim();
    Player->GetCombatAim(Aim, Frame);
    TestFalse(TEXT("Runtime off clears active target"), Player->bAimAssistActive);
    TestTrue(TEXT("Runtime off shoots through displayed aim"), Pixel(Projection, Aim).Equals(BeforeOff,.001));
    TestEqual(TEXT("Runtime off preserves original mouse projection"), Player->AimWorldLocation, Raw);
    TestEqual(TEXT("Runtime off preserves displayed position"), Player->AimScreenPosition, BeforeOff);
    Env.Config->bEnableAimAssist = true;
    Player->UpdateCombatAim(Projection, GFrameCounter + 4, .02f);
    const FVector2D BeforeDrag = Player->AimScreenPosition;
    Player->CrosshairSnapState.UpdateInput(Player->RawAimScreenPosition, FVector2D(70,0), Projection.GetConstrainedViewRect(), GFrameCounter + 5);
    Player->RawAimScreenPosition = Player->CrosshairSnapState.RawPosition;
    const FVector2D DraggedRaw = Player->RawAimScreenPosition;
    Player->UpdateCombatAim(Projection, GFrameCounter + 5, .02f);
    TestFalse(TEXT("Mouse drag releases capture"), Player->bAimAssistActive);
    TestTrue(TEXT("Release keeps applied mouse movement"), Player->AimScreenPosition.Equals(BeforeDrag + FVector2D(70,0), .001));
    TestEqual(TEXT("Drag release preserves accumulated mouse input"), Player->RawAimScreenPosition, DraggedRaw);
    Player->RawAimScreenPosition = Pixel(Projection, Target->GetActorLocation());
    Player->UpdateCombatAim(Projection, GFrameCounter + 6, .02f);
    TestTrue(TEXT("Returning raw cursor to body immediately reacquires without a blocked-target latch"), Player->bAimAssistActive);
    Player->RawAimScreenPosition += FVector2D(200,0);
    Player->UpdateCombatAim(Projection, GFrameCounter + 7, .02f);
    Player->RawAimScreenPosition = Pixel(Projection, Target->GetActorLocation());
    Player->UpdateCombatAim(Projection, GFrameCounter + 8, .02f);
    TestTrue(TEXT("Leaving and reentering can acquire target again"), Player->bAimAssistActive);
    const FVector2D RawBeforeMotion = Player->RawAimScreenPosition;
    const FVector2D DisplayBeforeMotion = Player->AimScreenPosition;
    Target->SetActorLocation(Target->GetActorLocation() + FVector(0,20,0));
    Player->UpdateCombatAim(Projection, GFrameCounter + 9, .12f);
    TestTrue(TEXT("Small target motion remains captured without mouse input"), Player->bAimAssistActive);
    TestEqual(TEXT("Following target motion does not move raw input"), Player->RawAimScreenPosition, RawBeforeMotion);
    TestTrue(TEXT("Displayed crosshair follows moving target"), (Player->AimScreenPosition-DisplayBeforeMotion).Size() > 1.);
    Player->GetCombatAim(Aim, Frame);
    TestTrue(TEXT("Moving target immediately updates ballistic center"),
        FVector2D(Aim).Equals(FVector2D(Target->GetActorLocation()), .001));
    TArray<FVector2D> Hull;
    FVector2D BodyCenter;
    TestTrue(TEXT("Held target body projects"), CandyFightAimAssist::ProjectBody(Target->HitBox, Projection, Hull, BodyCenter));
    if (!Hull.IsEmpty())
    {
        FVector2D Edge = Hull[0];
        for (const auto& Point : Hull) if (Point.X > Edge.X) Edge = Point;
        Player->RawAimScreenPosition = Edge + FVector2D(Env.Config->AimAssistPaddingPixels + Env.Config->AimAssistReleasePaddingPixels * .5, 0.);
        CandyFightAimAssist::FCandidate Held;
        TestFalse(TEXT("Hold-only cursor cannot acquire a fresh target"), CandyFightAimAssist::Evaluate(Env.World, Player,
            Combat->EquippedWeapon, Target, Combat->EquippedWeapon->Muzzle->GetComponentLocation(), Projection,
            Player->RawAimScreenPosition, *Env.Config, Held));
        Player->UpdateCombatAim(Projection, GFrameCounter + 10, .02f);
        TestTrue(TEXT("Existing capture remains in extra hold region"), Player->bAimAssistActive);
        Player->RevalidateCombatAim();
        Player->GetCombatAim(Aim, Frame);
        TestTrue(TEXT("Ballistic correction remains active throughout hold region"),
            Player->bAimAssistActive && FVector2D(Aim).Equals(FVector2D(Target->GetActorLocation()), .001));
    }
    Target->SetActorLocation(Target->GetActorLocation() + FVector(0,600,0));
    Player->UpdateCombatAim(Projection, GFrameCounter + 11, .12f);
    TestFalse(TEXT("Passive motion outside hold range releases capture"), Player->bAimAssistActive);
    const FVector2D ReleasedDisplay = Player->AimScreenPosition;
    const FVector2D ReleasedRaw = Player->RawAimScreenPosition;
    Player->UpdateCombatAim(Projection, GFrameCounter + 12, .12f);
    TestFalse(TEXT("Released moving target cannot drag cursor indefinitely"), Player->bAimAssistActive);
    TestEqual(TEXT("Passive release never changes raw mouse input"), Player->RawAimScreenPosition, ReleasedRaw);
    TestTrue(TEXT("Passive release smoothly returns visible cursor toward raw input"),
        (Player->AimScreenPosition-ReleasedRaw).Size() < (ReleasedDisplay-ReleasedRaw).Size());
    auto* NewEnemy = Env.Enemy(FVector(1000,-250,88));
    Player->RawAimScreenPosition = Pixel(Projection, NewEnemy->GetActorLocation() + FVector(0,0,80));
    Player->UpdateCombatAim(Projection, GFrameCounter + 13, .001f);
    TestEqual(TEXT("Mixed-type acquisition replaces previous target"), Player->AimAssistTarget.Get(), static_cast<AActor*>(NewEnemy));
    Player->GetCombatAim(Aim, Frame);
    TestTrue(TEXT("Target replacement never uses previous ballistic center"),
        FVector2D(Aim).Equals(FVector2D(NewEnemy->GetActorLocation()), .001));
    Player->UnPossessed();
    TestFalse(TEXT("Unpossession clears correction"), Player->bAimAssistActive);
    TestFalse(TEXT("Unpossession clears weak target"), Player->AimAssistTarget.IsValid());
    Player->bHasValidAim = false;
    const int32 AmmoBeforeInvalidAim = Combat->GetAmmo();
    Combat->FireShot(*Player, 2.);
    TestEqual(TEXT("Invalid aim cannot commit ammunition"), Combat->GetAmmo(), AmmoBeforeInvalidAim);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimAssistReleaseCameraTest, "CandyFight.Aiming.Assist.ReleaseCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAimAssistReleaseCameraTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightAimFraming;
    for (int32 Reason = 0; Reason < 6; ++Reason)
    {
        FAimWorld Env;
        auto* Player = Env.World->SpawnActor<ACandyFightCharacter>(FVector(0,0,88),FRotator::ZeroRotator);
        auto* PC = Env.World->SpawnActor<APlayerController>();
        PC->Possess(Player);
        Player->CameraConfig = NewObject<UCandyFightCameraConfig>(Player);
        auto* Combat = Player->Combat.Get();
        Combat->WeaponConfigs = {Env.Config,Env.Config};
        Combat->WeaponStates.SetNum(2);
        for (auto& State : Combat->WeaponStates) State.Initialize(*Env.Config,0.);
        TestTrue(TEXT("Release fixture equips weapon"),Combat->EquipTestWeapon(1));
        auto* TargetActor = Env.Target(FVector(1000,250,88));
        const auto Projection = MakeAimProjection();
        const FIntRect Rect = Projection.GetConstrainedViewRect();
        const FVector2D Margins(.1,.1);
        uint64 Frame = GFrameCounter;
        Player->RawAimScreenPosition = Pixel(Projection,TargetActor->GetActorLocation()+FVector(0,0,80));
        Player->AimFramingState.Update(Target(Player->RawAimScreenPosition,Rect,Margins),true,0.f,0.f,Frame,Margins);
        Player->UpdateCombatAim(Projection,Frame,.5f);
        TestTrue(TEXT("Release fixture captures target"),Player->bAimAssistActive);
        TestTrue(TEXT("Release fixture has a real input/display difference"),
            (Player->RawAimScreenPosition-Player->AimScreenPosition).Size() > 1.);
        FVector2D ExpectedDisplay = Player->AimScreenPosition;
        if (Reason == 0)
        {
            ++Frame;
            const FVector2D Drag(70,0);
            Player->CrosshairSnapState.UpdateInput(Player->RawAimScreenPosition,Drag,Rect,Frame);
            Player->RawAimScreenPosition = Player->CrosshairSnapState.RawPosition;
            ExpectedDisplay += Drag;
            Player->AimFramingState.Update(Target(Player->RawAimScreenPosition,Rect,Margins),true,0.f,1.f/60,Frame,Margins);
        }
        const auto BeforeFraming = Player->AimFramingState.Position;
        const auto BeforeRaw = Player->RawAimScreenPosition;
        FVector BeforeOffset;
        TestTrue(TEXT("Pre-release camera is solvable"),SolveOffset(Projection,BeforeFraming,Player->GetActorLocation(),BeforeOffset));
        if (Reason == 0) Player->UpdateCombatAim(Projection,Frame,.02f);
        if (Reason == 1) { TargetActor->SetActorHiddenInGame(true); Player->RevalidateCombatAim(); }
        if (Reason == 2) { TargetActor->TakeDamage(1000.f,FDamageEvent(),nullptr,Player); Player->RevalidateCombatAim(); }
        if (Reason == 3)
        {
            Env.Blocker((Combat->EquippedWeapon->Muzzle->GetComponentLocation()+
                FVector(TargetActor->GetActorLocation().X,TargetActor->GetActorLocation().Y,108))* .5);
            Player->RevalidateCombatAim();
        }
        if (Reason == 4) { Env.Config->bEnableAimAssist = false; Player->RevalidateCombatAim(); }
        if (Reason == 5) TestTrue(TEXT("Switch invokes shared release"),Combat->EquipTestWeapon(2));
        TestFalse(TEXT("Every tested release clears the target"),Player->bAimAssistActive);
        TestEqual(TEXT("Release keeps the displayed cursor and applied input"),Player->AimScreenPosition,ExpectedDisplay);
        TestEqual(TEXT("Every release preserves the original mouse input"),Player->RawAimScreenPosition,BeforeRaw);
        TestEqual(TEXT("Release does not create a camera rebound compensation"),Player->AimFramingState.ReleaseSeconds,0.);
        const auto RawTarget = Target(Player->RawAimScreenPosition,Rect,Margins);
        TestEqual(TEXT("Same-frame camera evaluation cannot jump or advance"),
            Player->AimFramingState.Update(RawTarget,true,0.f,.2f,Frame,Margins),BeforeFraming);
        Player->AimFramingState.Update(RawTarget,true,0.f,.12f,++Frame,Margins);
        FVector AfterOffset;
        SolveOffset(Projection,Player->AimFramingState.Position,Player->GetActorLocation(),AfterOffset);
        TestTrue(TEXT("Stationary mouse keeps camera unchanged after release"),BeforeOffset.Equals(AfterOffset,.00001));
        Player->CrosshairSnapState.UpdateInput(Player->RawAimScreenPosition,FVector2D::ZeroVector,Rect,Frame);
        Player->CrosshairSnapState.ReturnToInput(.12f,Frame);
        Player->AimScreenPosition = Player->CrosshairSnapState.DisplayPosition;
        const FVector2D ReturnedDisplay = BeforeRaw + (ExpectedDisplay-BeforeRaw)*.05;
        TestTrue(TEXT("Released cursor catches up smoothly without rebasing input"),Player->AimScreenPosition.Equals(ReturnedDisplay,.001));
        auto CurrentProjection = Projection;
        CurrentProjection.ViewOrigin += AfterOffset;
        Player->AimAssistProjection = MakeShared<FSceneViewProjectionData>(CurrentProjection);
        Player->RevalidateCombatAim();
        TestTrue(TEXT("Shot revalidation does not advance release catch-up"),Player->AimScreenPosition.Equals(ReturnedDisplay,.001));
        FVector Aim; uint64 AimFrame;
        TestTrue(TEXT("Displayed aim remains valid during catch-up"),Player->GetCombatAim(Aim,AimFrame));
        TestTrue(TEXT("Combat aim matches displayed crosshair during catch-up"),Pixel(CurrentProjection,Aim).Equals(Player->AimScreenPosition,.001));
        Player->SetActorRotation(FRotator(0.,(Aim-Player->GetActorLocation()).Rotation().Yaw,0.));
        Combat->FireShot(*Player,0.);
        TestEqual(TEXT("Shooting during release still commits a shot"),Combat->TotalShotsFired,1);
        if (TestEqual(TEXT("Release fixture creates one base trajectory"),Combat->LastShotEnds.Num(),1))
        {
            TestTrue(TEXT("Gun and base trajectory remain aligned during camera blend"),
                FVector::DotProduct(Combat->EquippedWeapon->GetActorForwardVector(),
                    (Combat->LastShotEnds[0]-Combat->LastShotStart).GetSafeNormal2D()) > .999);
            TestTrue(TEXT("Shot follows the actual displayed aim during release"),
                (Combat->LastShotEnds[0]-Combat->LastShotStart).GetSafeNormal2D().Equals((Aim-Combat->LastShotStart).GetSafeNormal2D(),.00001));
        }
        const FVector2D MouseMove(30,-15);
        Player->CrosshairSnapState.UpdateInput(Player->RawAimScreenPosition,MouseMove,Rect,++Frame);
        Player->RawAimScreenPosition = Player->CrosshairSnapState.RawPosition;
        const FVector2D MovedTarget = Target(Player->RawAimScreenPosition,Rect,Margins);
        TestEqual(TEXT("New mouse motion drives normal camera framing without release lag"),
            Player->AimFramingState.Update(MovedTarget,true,0.f,.01f,Frame,Margins),MovedTarget);
        Player->AimFramingState.BeginReleaseBlend(BeforeFraming,.2f);
        Player->UnPossessed();
        TestEqual(TEXT("Losing control clears camera compensation"),Player->AimFramingState.ReleaseSeconds,0.);
    }
    return true;
}
#endif
