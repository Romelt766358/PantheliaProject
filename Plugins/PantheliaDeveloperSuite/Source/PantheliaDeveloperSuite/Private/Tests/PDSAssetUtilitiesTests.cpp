#include "Misc/AutomationTest.h"
#include "PDSAssetUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSNormalizePackageRootTest,
    "Panthelia.DeveloperSuite.AssetUtilities.NormalizePackageRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSNormalizePackageRootTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Relative roots are placed under /Game"),
        PDSAssetUtilities::NormalizePackageRoot(TEXT("Blueprints/")),
        FString(TEXT("/Game/Blueprints")));

    TestEqual(
        TEXT("Backslashes and trailing slashes are normalized"),
        PDSAssetUtilities::NormalizePackageRoot(TEXT("Game\\Characters\\")),
        FString(TEXT("/Game/Characters")));

    TestEqual(
        TEXT("The /Game root remains stable"),
        PDSAssetUtilities::NormalizePackageRoot(TEXT("/Game")),
        FString(TEXT("/Game")));

    TestEqual(
        TEXT("The filesystem-style root remains stable"),
        PDSAssetUtilities::NormalizePackageRoot(TEXT("/")),
        FString(TEXT("/")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPathUnderPackageRootTest,
    "Panthelia.DeveloperSuite.AssetUtilities.PathUnderPackageRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPathUnderPackageRootTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("A direct child matches"),
        PDSAssetUtilities::IsPathUnderPackageRoot(
            TEXT("/Game/Fab/MetaHuman"),
            TEXT("/Game/Fab")));

    TestTrue(
        TEXT("The root itself matches"),
        PDSAssetUtilities::IsPathUnderPackageRoot(
            TEXT("/Game/Fab"),
            TEXT("/Game/Fab")));

    TestFalse(
        TEXT("Folder-prefix collisions do not match"),
        PDSAssetUtilities::IsPathUnderPackageRoot(
            TEXT("/Game/Fabricated"),
            TEXT("/Game/Fab")));

    TestTrue(
        TEXT("Package root matching is case-insensitive"),
        PDSAssetUtilities::IsPathUnderPackageRoot(
            TEXT("/game/fab/metahuman"),
            TEXT("/Game/Fab")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAssetOriginPrecedenceTest,
    "Panthelia.DeveloperSuite.AssetUtilities.AssetOriginPrecedence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAssetOriginPrecedenceTest::RunTest(const FString& Parameters)
{
    const TArray<FString> CoreRoots = {
        TEXT("/Game/Characters")
    };
    const TArray<FString> ExternalRoots = {
        TEXT("/Game/Characters/ThirdParty")
    };

    TestTrue(
        TEXT("External roots override an overlapping Core root"),
        PDSAssetUtilities::ClassifyAssetOriginWithRoots(
            TEXT("/Game/Characters/ThirdParty/Mannequin"),
            CoreRoots,
            ExternalRoots) == EPDSAssetOrigin::ExternalContent);

    TestTrue(
        TEXT("Non-overlapping assets remain Core"),
        PDSAssetUtilities::ClassifyAssetOriginWithRoots(
            TEXT("/Game/Characters/Player"),
            CoreRoots,
            ExternalRoots) == EPDSAssetOrigin::PantheliaCore);

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSOriginResolverTest,
    "Panthelia.DeveloperSuite.AssetUtilities.OriginResolver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSOriginResolverTest::RunTest(const FString& Parameters)
{
    const FPDSOriginResolver Resolver(
        {TEXT("Blueprints/"), TEXT("/Game/Characters/")},
        {TEXT("/Game/Characters/ThirdParty"), TEXT("Game/Fab/")},
        {TEXT("Developers/")});

    TestTrue(
        TEXT("The resolver normalizes Core roots once and classifies Core assets"),
        Resolver.Classify(TEXT("/Game/Blueprints/AbilitySystem"))
            == EPDSAssetOrigin::PantheliaCore);

    TestTrue(
        TEXT("External keeps precedence over overlapping Core roots"),
        Resolver.Classify(TEXT("/Game/Characters/ThirdParty/Mannequin"))
            == EPDSAssetOrigin::ExternalContent);

    TestTrue(
        TEXT("Excluded roots are normalized and detected"),
        Resolver.IsExcluded(TEXT("/Game/Developers/Romelt/TestAsset")));

    TestTrue(
        TEXT("Other /Game assets remain Game Content"),
        Resolver.Classify(TEXT("/Game/Environment/Ruins"))
            == EPDSAssetOrigin::GameContent);

    return true;
}

#endif
