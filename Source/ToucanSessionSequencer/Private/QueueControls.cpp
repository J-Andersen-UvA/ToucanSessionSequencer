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
#include "EditorAssetLibrary.h"
#include "Editor.h"

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

void FQueueControls::RemoveAllAnimations()
{
    auto& Queue = FSeqQueue::Get();
    const auto& All = Queue.GetAll();

    if (All.Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Queue already empty."));
        return;
    }

    // Remove all entries
    Queue.Clear();
    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Removed all %d animations from queue."), All.Num());
}

void FQueueControls::RemoveMarkedProcessedAnimations()
{
    auto& Queue = FSeqQueue::Get();
    auto All = Queue.GetAll(); // copy to iterate safely
    int32 RemovedCount = 0;

    for (int32 i = All.Num() - 1; i >= 0; --i)
    {
        UObject* AnimObject = All[i].Path.TryLoad();
        if (!AnimObject)
            continue;

        if (UEditorAssetLibrary::GetMetadataTag(AnimObject, TEXT("Processed")) == TEXT("True"))
        {
            Queue.RemoveAt(i);
            ++RemovedCount;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Removed %d processed animations from queue."), RemovedCount);
}
