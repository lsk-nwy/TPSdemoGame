#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightWeaponHoldPointTest, "CandyFight.Weapons.CharacterHoldPoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightWeaponHoldPointTest::RunTest(const FString& Parameters)
{
    auto* Class = LoadClass<ACandyFightCharacter>(nullptr, TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
    if (!TestNotNull(TEXT("Character Blueprint loads"), Class)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Character = World->SpawnActor<ACandyFightCharacter>(Class, FVector(0.,0.,90.), FRotator::ZeroRotator);
    auto* Combat = Character->Combat.Get();
    auto* Hold = Character->WeaponHoldPoint.Get();
    TestNotNull(TEXT("Character has an editable hold point"), Hold);
    TestTrue(TEXT("Hold point is attached to character root"), Hold->GetAttachParent() == Character->GetRootComponent());
    TArray<USceneComponent*> Components;
    Character->GetComponents(Components);
    for (const auto* Component : Components)
        TestFalse(TEXT("Old direction marker no longer exists"), Component->GetName() == TEXT("AimDirectionMarker"));
    TestTrue(TEXT("No weapon uses hold height fallback"), FMath::IsNearlyEqual(Combat->GetAimPlaneZ(Hold->GetComponentLocation().Z), Hold->GetComponentLocation().Z));
    const FTransform Edited(FRotator::ZeroRotator, FVector(-20.,80.,45.), FVector::OneVector);
    Hold->SetRelativeTransform(Edited);
    TestTrue(TEXT("Character configuration remains valid"), Character->ApplyConfiguration());
    TestTrue(TEXT("Applying configuration preserves edited hold transform"), Hold->GetRelativeTransform().Equals(Edited));
    Combat->WeaponStates.SetNum(Combat->WeaponConfigs.Num());
    for (int32 I=0; I<Combat->WeaponConfigs.Num(); ++I)
        Combat->WeaponStates[I].Initialize(*Combat->WeaponConfigs[I],0.);
    for (int32 I=1; I<=Combat->WeaponConfigs.Num(); ++I)
    {
        TestTrue(TEXT("Equip at character hold point"), Combat->EquipTestWeapon(I));
        auto* Weapon=Combat->EquippedWeapon.Get();
        TestTrue(TEXT("Gun parent is the hold point"), Weapon->GetRootComponent()->GetAttachParent()==Hold);
        const FVector Expected=Hold->GetComponentTransform().TransformPosition(Weapon->MountTransform.GetLocation());
        TestTrue(TEXT("Gun position includes per-weapon local adjustment"), Weapon->GetActorLocation().Equals(Expected,.001));
        TestTrue(TEXT("Equipping preserves hold transform"), Hold->GetRelativeTransform().Equals(Edited));
        Character->SetActorLocationAndRotation(FVector(100.,200.,90.),FRotator(0.,90.,0.));
        const FVector Rotated=Hold->GetComponentTransform().TransformPosition(Weapon->MountTransform.GetLocation());
        TestTrue(TEXT("Gun follows character movement and rotation"), Weapon->GetActorLocation().Equals(Rotated,.001));
        const FVector Target=Weapon->GetActorLocation()+Character->GetActorForwardVector()*2000.;
        Weapon->AimAt(Target,Character->GetActorForwardVector());
        const FVector AimDirection=(Target-Weapon->Muzzle->GetComponentLocation()).GetSafeNormal2D();
        TestTrue(TEXT("Gun still aims at target from edited attachment"), FVector::DotProduct(AimDirection,Weapon->GetActorForwardVector())>.9999);
        TestTrue(TEXT("Aiming does not move hold point"), Hold->GetRelativeTransform().Equals(Edited));
        TestTrue(TEXT("Equipped aim plane uses real muzzle height"), FMath::IsNearlyEqual(Combat->GetAimPlaneZ(-999.),Weapon->Muzzle->GetComponentLocation().Z));
    }
    World->DestroyWorld(false);
    return true;
}
#endif
