#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ACandyFightWeapon::ACandyFightWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    MountRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MountRoot"));
    SetRootComponent(MountRoot);
    WeaponVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponVisualRoot"));
    WeaponVisualRoot->SetupAttachment(MountRoot);
    Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
    Muzzle->SetupAttachment(MountRoot);
    Muzzle->SetRelativeLocation(FVector(55., 0., 0.));
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
    Grip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grip"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (UStaticMeshComponent* Part : {Body.Get(), Barrel.Get(), Grip.Get()})
    {
        Part->SetupAttachment(WeaponVisualRoot);
        Part->SetStaticMesh(Cube.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
    }
    Body->SetRelativeLocation(FVector(12., 0., 0.));
    Body->SetRelativeScale3D(FVector(.30, .13, .16));
    Barrel->SetRelativeLocation(FVector(38., 0., 2.));
    Barrel->SetRelativeScale3D(FVector(.30, .065, .065));
    Grip->SetRelativeLocation(FVector(4., 0., -12.));
    Grip->SetRelativeScale3D(FVector(.10, .10, .18));
}

void ACandyFightWeapon::BeginPlay()
{
    Super::BeginPlay();
    // Also disable collision on any decorative mesh added later in the blueprint.
    TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
    for (UPrimitiveComponent* Part : Primitives)
    {
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
    }
}

bool ACandyFightWeapon::ValidateMuzzle() const
{
    return IsValid(Muzzle) && Muzzle->IsRegistered() && !Muzzle->GetComponentTransform().ContainsNaN();
}

void ACandyFightWeapon::AimAt(const FVector& Target, const FVector& FallbackForward)
{
    if (!ValidateMuzzle()) return;
    const FVector Pivot = GetActorLocation();
    FVector ToTarget = Target - Pivot;
    ToTarget.Z = 0.;
    // Solve yaw including lateral muzzle offset, so customized guns still converge on the cursor.
    const FVector LocalMuzzle = GetActorTransform().InverseTransformPosition(Muzzle->GetComponentLocation()) * GetActorScale3D();
    const double Distance = ToTarget.Size();
    if (Distance <= LocalMuzzle.Size2D() + 5. || FVector::DotProduct(ToTarget, FallbackForward) <= 0.)
    {
        SetActorRotation(FRotator(0., FallbackForward.Rotation().Yaw, 0.));
        return;
    }
    const double OffsetAngle = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(LocalMuzzle.Y / Distance, -1., 1.)));
    SetActorRotation(FRotator(0., ToTarget.Rotation().Yaw - OffsetAngle, 0.));
}
