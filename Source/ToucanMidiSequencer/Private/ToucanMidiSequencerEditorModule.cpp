#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Docking/TabManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserDelegates.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Animation/AnimSequence.h"
#include "Editor.h"
#include "SeqQueue.h"
#include "SQueueWindow.h"

static const FName ToucanQueueTabName(TEXT("ToucanQueueControls"));
static const FName ToucanEditingTabName(TEXT("ToucanEditingSession"));

class FToucanSequencerEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToucanQueueTabName,
            FOnSpawnTab::CreateRaw(this, &FToucanSequencerEditorModule::SpawnQueueControlTab))
            .SetDisplayName(FText::FromString(TEXT("Queue Controls")))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToucanEditingTabName,
            FOnSpawnTab::CreateRaw(this, &FToucanSequencerEditorModule::SpawnEditingSessionTab))
            .SetDisplayName(FText::FromString(TEXT("Editing Session")))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        // Add to main menu (e.g. Window → Developer Tools)
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FToucanSequencerEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToucanQueueTabName);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToucanEditingTabName);
    }

private:
    void RegisterMenus()
    {
        if (!UToolMenus::IsToolMenuUIEnabled())
            return;

        // --- Create or find the main Level Editor menu bar ---
        UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");

        // --- Add a new "Toucan" dropdown to the top menu bar ---
        FToolMenuSection& MenuBarSection = MainMenu->AddSection("ToucanMenuBarSection");
        MenuBarSection.AddSubMenu(
            "ToucanRoot",
            FText::FromString("Toucan sequencer"),
            FText::FromString("Toucan tools and utilities"),
            FNewToolMenuDelegate::CreateLambda([](UToolMenu* ToucanMenu)
                {
                    FToolMenuSection& Sec = ToucanMenu->AddSection("ToucanSeq", FText::FromString("Sequence Tools"));

                    Sec.AddMenuEntry(
                        "QueueControls",
                        FText::FromString("Queue Controls"),
                        FText::FromString("Open the Sequence Controller tab."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateLambda([] {
                            FGlobalTabmanager::Get()->TryInvokeTab(ToucanQueueTabName);
                            }))
                    );

                    Sec.AddMenuEntry(
                        "EditingSession",
                        FText::FromString("Editing Session"),
                        FText::FromString("Open the Editing Session tab."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateLambda([] {
                            FGlobalTabmanager::Get()->TryInvokeTab(ToucanEditingTabName);
                            }))
                    );
                }),
            /*bInOpenSubMenuOnClick=*/false,
            FSlateIcon()
        );
    }

    TSharedRef<SDockTab> SpawnQueueControlTab(const FSpawnTabArgs&)
    {
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)
        [
            SNew(SBorder).Padding(8)
            [
                SNew(SVerticalBox)

                // Queue animations in folder
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Queue all animations in folder")))
                    .OnClicked_Lambda([this]()
                    {
                        OpenFolderAndEnqueue();
                        return FReply::Handled();
                    })
                ]

                // Queue animation (single)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Queue animations by hand")))
                    .OnClicked_Lambda([this]()
                    {
                        OpenAssetDialogAndEnqueue();
                        return FReply::Handled();
                    })
                ]

                // Current queue
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Current queue")))
                    .OnClicked_Lambda([this]()
                    {
                        auto Win = SNew(SWindow)
                            .Title(FText::FromString(TEXT("Toucan – Current Queue")))
                            .ClientSize(FVector2D(520, 420))
                            .SupportsMaximize(false).SupportsMinimize(false);
                        Win->SetContent(SNew(SQueueWindow));
                        FSlateApplication::Get().AddWindow(Win);
                        return FReply::Handled();
                    })
                ]
            ]
        ];
    }
    TSharedRef<SDockTab> SpawnEditingSessionTab(const FSpawnTabArgs&)
    {
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)
        [
            SNew(SBorder).Padding(8)
            [
                SNew(SVerticalBox)

                // Editing Session
                +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Editing Session")))
                        .OnClicked_Lambda([this]()
                        {
                            auto Win = SNew(SWindow)
                                .Title(FText::FromString(TEXT("Toucan – Editing Session")))
                                .ClientSize(FVector2D(680, 520))
                                .SupportsMaximize(false)
                                .SupportsMinimize(false);

                            // Temporary placeholder (will be replaced with SEditingSessionWindow)
                            Win->SetContent(
                                SNew(SBorder).Padding(8)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Editing Session — placeholder. Will display session data here.")))
                                ]
                            );

                            FSlateApplication::Get().AddWindow(Win);
                            return FReply::Handled();
                        })
                ]
            ]
        ];
    }

    // ----- helpers -----

    void OpenFolderAndEnqueue()
    {
        // Simple path picker window
        FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        TSharedPtr<FString> PickedPath = MakeShared<FString>();

        FPathPickerConfig Config;
        Config.DefaultPath = TEXT("/Game");
        Config.OnPathSelected = FOnPathSelected::CreateLambda([PickedPath](const FString& NewPath)
            {
                *PickedPath = NewPath;
            });

        TSharedPtr<SWidget> Picker = CB.Get().CreatePathPicker(Config);

        // Wrap in a dialog with OK/Cancel
        TSharedRef<SWindow> Win = SNew(SWindow)
            .Title(FText::FromString(TEXT("Pick a folder")))
            .ClientSize(FVector2D(450, 400))
            .SupportsMaximize(false)
            .SupportsMinimize(false);

        Win->SetContent(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.f)[Picker.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton).Text(FText::FromString(TEXT("OK")))
                            .OnClicked_Lambda([this, WinPtr = &Win.Get(), PickedPath]()
                                {
                                    if (!PickedPath->IsEmpty())
                                    {
                                        EnqueueFolder(*PickedPath);
                                    }
                                    WinPtr->RequestDestroyWindow();
                                    return FReply::Handled();
                                })
                    ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
                    [
                        SNew(SButton).Text(FText::FromString(TEXT("Cancel")))
                            .OnClicked_Lambda([WinPtr = &Win.Get()]()
                                {
                                    WinPtr->RequestDestroyWindow();
                                    return FReply::Handled();
                                })
                    ]
            ]);

        FSlateApplication::Get().AddWindow(Win);
    }

    void EnqueueFolder(const FString& ContentPath) // Looks in /Game/ folder
    {
        FARFilter Filter;
        Filter.PackagePaths.Add(*ContentPath);
        Filter.bRecursivePaths = true;

        Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
        // Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());

        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

        TArray<FAssetData> Out;
        ARM.Get().GetAssets(Filter, Out);

        int32 Added = 0;
        for (const FAssetData& A : Out)
        {
            FSeqQueue::Get().Add(A);
            ++Added;
        }

        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Enqueued %d assets from %s"), Added, *ContentPath);
    }

    void OpenAssetDialogAndEnqueue()
    {
        FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        FOpenAssetDialogConfig C;
        C.DialogTitleOverride = FText::FromString(TEXT("Select animation"));
        C.bAllowMultipleSelection = true;
        C.AssetClassNames.Add(UAnimSequence::StaticClass()->GetClassPathName());

        CB.Get().CreateOpenAssetDialog(
            C,
            FOnAssetsChosenForOpen::CreateLambda([](const TArray<FAssetData>& SelectedAssets)
                {
                    for (const FAssetData& A : SelectedAssets)
                    {
                        FSeqQueue::Get().Add(A);
                    }
                    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Enqueued %d assets"), SelectedAssets.Num());
                }),
            FOnAssetDialogCancelled::CreateLambda([]()
                {
                    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Asset selection cancelled."));
                })
        );
    }
};

IMPLEMENT_MODULE(FToucanSequencerEditorModule, ToucanMidiSequencer)
