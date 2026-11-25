#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Flow/CandyFightFlow.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFlowResultsTest, "CandyFight.Flow.ResultSnapshotsAndDuplicateCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightFlowResultsTest::RunTest(const FString& Parameters)
{
    for (auto Outcome : {ECandyFightRunOutcome::Escaped, ECandyFightRunOutcome::Defeated,
        ECandyFightRunOutcome::TimedOut, ECandyFightRunOutcome::Abandoned})
    {
        FCandyFightRunLedger Run;
        TestFalse(TEXT("No result before a battle starts"), Run.Finish(Outcome, 50, 100.));
        Run.Start(10.);
        TestTrue(TEXT("First terminal callback commits"), Run.Finish(Outcome, 75, 42.5));
        TestEqual(TEXT("Outcome retained"), Run.Summary.Outcome, Outcome);
        TestEqual(TEXT("Gold snapshotted"), Run.Summary.Gold, 75);
        TestEqual(TEXT("Elapsed time snapshotted"), Run.Summary.ElapsedSeconds, 32.5f);
        TestFalse(TEXT("Late timeout/death cannot overwrite result"), Run.Finish(ECandyFightRunOutcome::TimedOut, 0, 999.));
        TestEqual(TEXT("Original gold preserved"), Run.Summary.Gold, 75);
        Run.Start(500.);
        TestEqual(TEXT("Next battle clears old result"), Run.Summary.Outcome, ECandyFightRunOutcome::None);
        TestTrue(TEXT("Second battle can finish"), Run.Finish(ECandyFightRunOutcome::Defeated, 2, 501.));
        TestEqual(TEXT("Second battle uses its own clock"), Run.Summary.ElapsedSeconds, 1.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFlowInvalidResultTest, "CandyFight.Flow.InvalidResultsDoNotEndBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightFlowInvalidResultTest::RunTest(const FString& Parameters)
{
    FCandyFightRunLedger Run;
    Run.Start(50.);
    TestFalse(TEXT("None cannot end a battle"), Run.Finish(ECandyFightRunOutcome::None, 10, 60.));
    TestTrue(TEXT("Invalid callback leaves battle active"), Run.bActive);
    TestTrue(TEXT("Real failure still commits"), Run.Finish(ECandyFightRunOutcome::Defeated, -10, 40.));
    TestEqual(TEXT("Gold is nonnegative"), Run.Summary.Gold, 0);
    TestEqual(TEXT("Elapsed time is nonnegative"), Run.Summary.ElapsedSeconds, 0.f);
    Run.Start(std::numeric_limits<double>::quiet_NaN());
    Run.Finish(ECandyFightRunOutcome::Abandoned, 0, std::numeric_limits<double>::infinity());
    TestEqual(TEXT("Invalid clock remains safe to display"), Run.Summary.ElapsedSeconds, 0.f);
    return true;
}
#endif