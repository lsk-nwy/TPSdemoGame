#if WITH_DEV_AUTOMATION_TESTS

#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Misc/AutomationTest.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "Misc/DataValidation.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCrosshairSpreadTest, "CandyFight.Crosshair.DesignerSpread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightCrosshairSpreadTest::RunTest(const FString& Parameters)
{
	UCandyFightCrosshairWidget* Widget = NewObject<UCandyFightCrosshairWidget>();
	Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
	UCanvasPanel* Root = Widget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Widget->WidgetTree->RootWidget = Root;
	UCanvasPanel* Visual = Widget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CrosshairVisual"));
	Root->AddChildToCanvas(Visual);
	Widget->CrosshairVisual = Visual;
	TArray<UImage*> Arms;
	for (const FCandyFightCrosshairSpreadBinding& Binding : Widget->SpreadBindings)
	{
		UImage* Arm = Widget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Binding.WidgetName);
		Visual->AddChildToCanvas(Arm)->SetPosition(Binding.Direction * 11.0);
		Arm->SetRenderTranslation(FVector2D(2.0, 4.0));
		Arms.Add(Arm);
	}

	Widget->SetSpread(1.0f);
	for (int32 Index = 0; Index < Arms.Num(); ++Index)
	{
		const FVector2D Expected = FVector2D(2.0, 4.0) + Widget->SpreadBindings[Index].Direction * 18.0;
		TestEqual(TEXT("Each line and outline moves outward from its original translation"), Arms[Index]->GetRenderTransform().Translation, Expected);
		TestEqual(TEXT("Designer slot positions stay unchanged"), CastChecked<UCanvasPanelSlot>(Arms[Index]->Slot)->GetPosition(), Widget->SpreadBindings[Index].Direction * 11.0);
		TestEqual(TEXT("Spread does not change line thickness or scale"), Arms[Index]->GetRenderTransform().Scale, FVector2D(1.0));
	}
	Widget->SetSpread(0.5f);
	Widget->SetSpread(0.5f);
	TestEqual(TEXT("Repeated calls do not accumulate movement"), Arms[0]->GetRenderTransform().Translation, FVector2D(-7.0, 4.0));
	Widget->SetSpread(2.0f);
	TestEqual(TEXT("Spread is clamped to one"), Widget->Spread, 1.0f);
	Widget->SetSpread(-1.0f);
	TestEqual(TEXT("Negative spread restores the authored position"), Arms[0]->GetRenderTransform().Translation, FVector2D(2.0, 4.0));
	Widget->SetSpread(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid spread becomes zero"), Widget->Spread, 0.0f);
	Widget->SetSpread(1.0f);
	Widget->bApplyDesignerSpread = false;
	Widget->SetSpread(1.0f);
	TestEqual(TEXT("Disabling automatic spread restores the authored translation"), Arms[0]->GetRenderTransform().Translation, FVector2D(2.0, 4.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCrosshairAssetSpreadTest, "CandyFight.Crosshair.BlueprintAssetSpread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightCrosshairAssetSpreadTest::RunTest(const FString& Parameters)
{
	UClass* WidgetClass = LoadClass<UCandyFightCrosshairWidget>(nullptr,
		TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightCrosshairWidget.BP_CandyFightCrosshairWidget_C"));
	if (!TestNotNull(TEXT("The configured Widget Blueprint exists"), WidgetClass))
	{
		return false;
	}
	UCandyFightCrosshairWidget* Widget = NewObject<UCandyFightCrosshairWidget>(GetTransientPackage(), WidgetClass);
	TestTrue(TEXT("Blueprint widget initialization succeeds"), Widget->Initialize());
	TestNotNull(TEXT("The Designer group binds to the native widget"), Widget->CrosshairVisual.Get());
	TestFalse(TEXT("The Blueprint uses Designer visuals"), Widget->bDrawNativeCrosshair);
	TestTrue(TEXT("Automatic Designer spread is enabled"), Widget->bApplyDesignerSpread);
	for (const FCandyFightCrosshairSpreadBinding& Binding : Widget->SpreadBindings)
	{
		UWidget* Arm = Widget->GetWidgetFromName(Binding.WidgetName);
		if (!TestNotNull(*FString::Printf(TEXT("Designer arm %s exists"), *Binding.WidgetName.ToString()), Arm))
		{
			continue;
		}
		Widget->SetSpread(0.0f);
		const FVector2D Base = Arm->GetRenderTransform().Translation;
		Widget->SetSpread(1.0f);
		TestEqual(TEXT("SetSpread moves the actual Blueprint arm"), Arm->GetRenderTransform().Translation,
			Base + Binding.Direction * Widget->VisualSpreadDistance);
		Widget->SetSpread(0.0f);
		TestEqual(TEXT("Zero spread restores the actual Blueprint arm"), Arm->GetRenderTransform().Translation, Base);
	}
	TestNull(TEXT("No center dot was added"), Widget->GetWidgetFromName(TEXT("CenterDot")));
	return true;
}

namespace
{
    FSceneViewProjectionData DistanceProjection(const FIntRect& Rect, double Yaw = 0.)
    {
        FMinimalViewInfo View;
        View.Rotation=FRotator(-55,Yaw,0);
        View.Location=-View.Rotation.Vector()*2000.;
        View.FOV=90;
        View.bConstrainAspectRatio=false;
        FSceneViewProjectionData Data;
        Data.SetViewRectangle(Rect);
        FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View,AspectRatio_MaintainXFOV,Rect,Data);
        Data.ViewOrigin=View.Location;
        Data.ViewRotationMatrix=FInverseRotationMatrix(View.Rotation)*FMatrix(
            FPlane(0,0,1,0),FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
        return Data;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightDistanceSpreadTest,"CandyFight.Crosshair.DistanceProjectionAndSmoothing",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightDistanceSpreadTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightDistanceSpread;
    for (const FIntRect Rect : {FIntRect(0,0,1920,1080),FIntRect(75,125,1355,845),FIntRect(0,0,1024,768)})
    for (double CameraYaw : {0.,73.,180.})
    for (double ShotYaw : {0.,45.,90.,180.,270.})
    {
        const auto Projection=DistanceProjection(Rect,CameraYaw);
        double Previous=0;
        for (double Distance : {0.,100.,500.,1000.,2000.})
        {
            const FVector Muzzle=-FRotator(0,ShotYaw,0).Vector()*Distance;
            double Radius=0;
            TestTrue(TEXT("Fan projects in all horizontal directions"),ProjectRadius(Projection.ComputeViewProjectionMatrix(),Rect,Muzzle,FVector::ZeroVector,8,Radius));
            TestTrue(TEXT("More aiming distance produces a larger radius"),Distance==0. ? Radius==0. : Radius>Previous);
            Previous=Radius;
            double Narrow=0;
            ProjectRadius(Projection.ComputeViewProjectionMatrix(),Rect,Muzzle,FVector::ZeroVector,3,Narrow);
            TestTrue(TEXT("Larger actual weapon spread makes larger crosshair"),Narrow<=Radius);
        }
    }
    const auto P=DistanceProjection(FIntRect(0,0,1920,1080));
    double Radius=0;
    ProjectRadius(P.ComputeViewProjectionMatrix(),P.GetConstrainedViewRect(),FVector(-1000,0,0),FVector::ZeroVector,8,Radius);
    TestTrue(TEXT("Projection agrees with distance times tan half angle"),FMath::IsNearlyEqual(Radius,1000.*FMath::Tan(FMath::DegreesToRadians(8.))*960./2000.,.001));
    TestEqual(TEXT("Near minimum"),TargetGap(0,6,96,1),6.);
    TestEqual(TEXT("Far maximum"),TargetGap(1000,6,96,1),96.);
    TestEqual(TEXT("Display multiplier"),TargetGap(20,6,96,2),40.);
    TestEqual(TEXT("Zero multiplier stays compact"),TargetGap(20,6,96,0),6.);
    TestFalse(TEXT("Invalid angle rejected"),ProjectRadius(P.ComputeViewProjectionMatrix(),P.GetConstrainedViewRect(),FVector::ZeroVector,FVector(1,0,0),std::numeric_limits<float>::quiet_NaN(),Radius));
    for (double Dpi : {.75,1.,1.5,2.})
    {
        TestTrue(TEXT("Designer DPI conversion preserves physical gap"),FMath::IsNearlyEqual((6.+DesignerOffset(40,6,Dpi,false))*Dpi,40.,1.e-8));
        TestEqual(TEXT("Pixel perfect ignores DPI for arm spacing"),DesignerOffset(40,6,Dpi,true),34.);
    }
    for (int32 FPS : {30,60,120})
    {
        FCandyFightDistanceSpreadState State;
        State.Update(6,.06,0,0);
        for (int32 Frame=1;Frame<=FPS/10;++Frame) State.Update(96,.06,double(Frame)/FPS,Frame);
        TestTrue(TEXT("Same convergence at 30/60/120 fps"),FMath::IsNearlyEqual(State.Gap,96.-90.*FMath::Exp(-5.),1.e-7));
        const double Previous=State.Gap;
        State.Update(6,.06,.2,FPS/10);
        TestEqual(TEXT("Repeated frame cannot advance smoothing twice"),State.Gap,Previous);
        State.Update(6,.06,.1,1000);
        TestEqual(TEXT("Paused game time freezes smoothing"),State.Gap,Previous);
        State.Update(6,0,.2,1001);
        TestEqual(TEXT("Zero smoothing applies immediately"),State.Gap,6.);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightDistanceSpreadWidgetTest,"CandyFight.Crosshair.DistanceWidget",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightDistanceSpreadWidgetTest::RunTest(const FString& Parameters)
{
    UClass* Class=LoadClass<UCandyFightCrosshairWidget>(nullptr,TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightCrosshairWidget.BP_CandyFightCrosshairWidget_C"));
    if (!TestNotNull(TEXT("Existing crosshair asset"),Class)) return false;
    auto* Widget=NewObject<UCandyFightCrosshairWidget>(GetTransientPackage(),Class);
    Widget->Initialize();
    TestTrue(TEXT("Existing asset inherits enabled distance behavior"),Widget->bEnableDistanceSpread);
    Widget->DistanceSpreadSmoothSeconds=0;
    Widget->SetSpread(0);
    auto* Arm=Widget->GetWidgetFromName(TEXT("RightLine"));
    if (!TestNotNull(TEXT("Right arm exists"),Arm)) return false;
    const FVector2D Authored=Arm->GetRenderTransform().Translation;
    const FVector2D SlotPosition=CastChecked<UCanvasPanelSlot>(Arm->Slot)->GetPosition();
    const auto Projection=DistanceProjection(FIntRect(0,0,1920,1080));
    const auto Matrix=Projection.ComputeViewProjectionMatrix();
    Widget->SetAimViewRect(Projection.GetConstrainedViewRect());
    Widget->ScreenPosition=FVector2D(500,300);
    Widget->RecordAimProjection(Matrix,1);
    Widget->UpdateWeaponSpread(FVector::ZeroVector,FVector::ZeroVector,8,true,0,1);
    TestEqual(TEXT("Near target uses minimum gap"),Widget->CurrentDistanceGap,6.f);
    Widget->RecordAimProjection(Matrix,2);
    Widget->UpdateWeaponSpread(FVector(-1000,0,0),FVector::ZeroVector,8,true,.1,2);
    const float FarGap=Widget->CurrentDistanceGap;
    TestTrue(TEXT("Actual blueprint arm expands with distance"),FarGap>6 && Arm->GetRenderTransform().Translation.X>Authored.X);
    TestEqual(TEXT("Expansion preserves authored slots"),CastChecked<UCanvasPanelSlot>(Arm->Slot)->GetPosition(),SlotPosition);
    TestEqual(TEXT("Expansion never moves crosshair center"),Widget->ScreenPosition,FVector2D(500,300));
    Widget->UpdateWeaponSpread(FVector(-1000,0,0),FVector::ZeroVector,8,true,.1,2);
    TestEqual(TEXT("Repeated updates do not accumulate expansion"),Widget->CurrentDistanceGap,FarGap);
    Widget->RecordAimProjection(Matrix,3);
    Widget->UpdateWeaponSpread(FVector(-1000,0,0),FVector::ZeroVector,8,false,.2,3);
    TestEqual(TEXT("Invalid aim returns to minimum"),Widget->CurrentDistanceGap,6.f);
    Widget->UpdateWeaponSpread(FVector(-1000,0,0),FVector::ZeroVector,8,true,.3,4);
    TestEqual(TEXT("Stale camera projection cannot enlarge crosshair"),Widget->CurrentDistanceGap,6.f);
    Widget->bEnableDistanceSpread=false;
    Widget->UpdateWeaponSpread(FVector(-1000,0,0),FVector::ZeroVector,8,true,.4,5);
    TestEqual(TEXT("Disabling distance restores old angle mapping"),Widget->Spread,.8f);
#if WITH_EDITOR
    Widget->DistanceMaxGap=-1;
    FDataValidationContext Context;
    TestEqual(TEXT("Invalid distance settings are rejected"),Widget->IsDataValid(Context),EDataValidationResult::Invalid);
#endif
    return true;
}

#endif
