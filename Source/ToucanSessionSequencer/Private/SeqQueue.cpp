#include "SeqQueue.h"
#include "Misc/ConfigCacheIni.h"
#include "AssetRegistry/AssetData.h"

void FSeqQueue::Load()
{
    Items.Reset();
    TArray<FString> Paths;

#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif
    GConfig->GetArray(SeqCfg::Section, SeqCfg::Key, Paths, Ini);

    for (const FString& S : Paths)
    {
        FSoftObjectPath P(S);
        FQueuedAnim Q;
        Q.Path = P;
        Q.DisplayName = FText::FromString(P.GetAssetName());
        Items.Add(MoveTemp(Q));
    }
}

void FSeqQueue::Save() const
{
    TArray<FString> Paths;
    Paths.Reserve(Items.Num());
    for (const auto& Q : Items)
    {
        Paths.Add(Q.Path.ToString());
    }

#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif
    GConfig->SetArray(SeqCfg::Section, SeqCfg::Key, Paths, Ini);
    GConfig->Flush(false, Ini);
}

FText FSeqQueue::MakeDisplayName(const FAssetData& A)
{
    FString Nice;
    if (A.GetTagValue(FName("DisplayName"), Nice) && !Nice.IsEmpty())
    {
        return FText::FromString(Nice);
    }
    return FText::FromName(A.AssetName);
}

void FSeqQueue::Add(const FAssetData& A)
{
    if (!A.IsValid()) return;
    FQueuedAnim Q;
    Q.Path = A.ToSoftObjectPath();
    Q.DisplayName = MakeDisplayName(A);
    if (!Items.Contains(Q))
    {
        Items.Add(MoveTemp(Q));
        Save();
        QueueChanged.Broadcast();
    }
}

void FSeqQueue::AddPath(const FSoftObjectPath& P, const FText& Nice)
{
    if (!P.IsValid()) return;
    FQueuedAnim Q; Q.Path = P; Q.DisplayName = Nice;
    if (!Items.Contains(Q))
    {
        Items.Add(MoveTemp(Q));
        Save();
        QueueChanged.Broadcast();
    }
}

bool FSeqQueue::RemoveAt(int32 Index)
{
    if (!Items.IsValidIndex(Index)) return false;
    Items.RemoveAt(Index);
    Save();
    QueueChanged.Broadcast();
    return true;
}
