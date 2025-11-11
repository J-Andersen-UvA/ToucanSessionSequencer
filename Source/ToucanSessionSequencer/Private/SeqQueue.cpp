#include "SeqQueue.h"
#include "Misc/ConfigCacheIni.h"
#include "AssetRegistry/AssetData.h"
#include "EditorAssetLibrary.h"

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
    GConfig->GetInt(SeqCfg::Section, SeqCfg::CurrentIndexKey, CurrentIndex, Ini);
    CurrentIndex = CheckBoundsIndex(CurrentIndex) ? CurrentIndex : INDEX_NONE;
    
    for (const FString& S : Paths)
    {
        FSoftObjectPath P(S);
        FQueuedAnim Q;
        Q.Path = P;
        Q.DisplayName = FText::FromString(P.GetAssetName());
        Items.Add(MoveTemp(Q));
    }
    
    CachedProcessedCount = -1; // Invalidate cache on load
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
    GConfig->SetInt(SeqCfg::Section, SeqCfg::CurrentIndexKey, CurrentIndex, Ini);
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
        CachedProcessedCount = -1; // Invalidate cache
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
        CachedProcessedCount = -1; // Invalidate cache
        Save();
        QueueChanged.Broadcast();
    }
}

bool FSeqQueue::RemoveAt(int32 Index)
{
    if (!Items.IsValidIndex(Index)) return false;
    Items.RemoveAt(Index);
    if (CurrentIndex == Index)
    {
        CurrentIndex = INDEX_NONE;
    }
    else if (CurrentIndex > Index)
    {
        --CurrentIndex;
    }

    CurrentIndex = CheckBoundsIndex(CurrentIndex) ? CurrentIndex : INDEX_NONE;
    CachedProcessedCount = -1; // Invalidate cache
    Save();
    QueueChanged.Broadcast();
    return true;
}

void FSeqQueue::Clear()
{
    Items.Reset();
    CurrentIndex = INDEX_NONE;
    CachedProcessedCount = 0; // Empty queue = 0 processed
    Save();
    QueueChanged.Broadcast();
}

void FSeqQueue::SetCurrentIndex(int32 NewIndex)
{
    // Check if NewIndex is in range. We set INDEX_NONE if not valid.
    CurrentIndex = CheckBoundsIndex(NewIndex) ? NewIndex : INDEX_NONE;
    Save();
    QueueChanged.Broadcast();
}

int32 FSeqQueue::GetProcessedCount()
{
    // Return cached value if valid
    if (CachedProcessedCount >= 0)
    {
        return CachedProcessedCount;
    }

    // Cache is invalid, recalculate
    int32 Count = 0;
    for (const FQueuedAnim& Item : Items)
    {
        UObject* Asset = Item.Path.TryLoad();
        if (Asset && UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed")) == TEXT("True"))
        {
            ++Count;
        }
    }
    
    CachedProcessedCount = Count;
    return Count;
}

void FSeqQueue::RefreshProcessedCount()
{
    // Invalidate cache to force recalculation on next GetProcessedCount()
    CachedProcessedCount = -1;
    QueueChanged.Broadcast(); // Trigger UI refresh
}
