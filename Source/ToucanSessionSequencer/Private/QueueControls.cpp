#include "QueueControls.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Animation/AnimSequence.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "SeqQueue.h"

void FQueueControls::AddAnimationsFromFolder()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TSharedPtr<FString> PickedPath = MakeShared<FString>();

    FPathPickerConfig Config;
    Config.DefaultPath = TEXT("/Game");
    Config.OnPathSelected = FOnPathSelected::CreateLambda([PickedPath](const FString& NewPath)
    {
        *PickedPath = NewPath;
    });

    TSharedRef<SWindow> Win = SNew(SWindow)
        .Title(FText::FromString(TEXT("Pick animation folder")))
        .ClientSize(FVector2D(450, 400))
        .SupportsMaximize(false).SupportsMinimize(false);

    Win->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)[CB.Get().CreatePathPicker(Config)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [

            SNew(SButton).Text(FText::FromString(TEXT("OK")))
            .OnClicked_Lambda([WinPtr=&Win.Get(),PickedPath]()
            {
                if (!PickedPath->IsEmpty())
                {
                    FARFilter Filter;
                    Filter.PackagePaths.Add(**PickedPath);
                    Filter.bRecursivePaths = true;
                    Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());

                    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
                    TArray<FAssetData> Out;
                    ARM.Get().GetAssets(Filter, Out);

                    for (const FAssetData& A : Out)
                        FSeqQueue::Get().Add(A);
                }
                WinPtr->RequestDestroyWindow();
                return FReply::Handled();
            })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
            [
                SNew(SButton)
                        .Text(FText::FromString(TEXT("Cancel")))
                        .OnClicked_Lambda([WinPtr = &Win.Get()]()
                            {
                                WinPtr->RequestDestroyWindow();
                                return FReply::Handled();
                            })
                ]
        ]);
    FSlateApplication::Get().AddWindow(Win);
}

void FQueueControls::AddAnimationsByHand()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

    FOpenAssetDialogConfig C;
    C.DialogTitleOverride = FText::FromString(TEXT("Select animations"));
    C.bAllowMultipleSelection = true;
    C.AssetClassNames.Add(UAnimSequence::StaticClass()->GetClassPathName());

    CB.Get().CreateOpenAssetDialog(
        C,
        FOnAssetsChosenForOpen::CreateLambda([](const TArray<FAssetData>& Selected)
        {
            for (const FAssetData& A : Selected)
                FSeqQueue::Get().Add(A);
        }),
        FOnAssetDialogCancelled::CreateLambda([] {})
    );
}
