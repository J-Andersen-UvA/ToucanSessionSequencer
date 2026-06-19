#include "SeqQueue.h"
#include "Misc/ConfigCacheIni.h"
#include "AssetRegistry/AssetData.h"

void FSeqQueue::Load()
{
    Items.Reset();
    TArray<FString> Paths;
    TArray<FString> ProcessedPaths;
    TArray<FString> CheckpointEntries;

#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif
    GConfig->GetArray(SeqCfg::Section, SeqCfg::Key, Paths, Ini);
    GConfig->GetArray(SeqCfg::Section, TEXT("ProcessedQueue"), ProcessedPaths, Ini);
    GConfig->GetArray(SeqCfg::Section, TEXT("CheckpointQueue"), CheckpointEntries, Ini);
    GConfig->GetInt(SeqCfg::Section, SeqCfg::CurrentIndexKey, CurrentIndex, Ini);

    TSet<FString> ProcessedSet;
    for (const FString& ProcessedPath : ProcessedPaths)
    {
        ProcessedSet.Add(ProcessedPath);
    }

    TMap<FString, FString> CheckpointMap;
    for (const FString& Entry : CheckpointEntries)
    {
        FString QueuePath;
        FString CheckpointPath;
        if (Entry.Split(TEXT("="), &QueuePath, &CheckpointPath) && !QueuePath.IsEmpty() && !CheckpointPath.IsEmpty())
        {
            CheckpointMap.Add(QueuePath, CheckpointPath);
        }
    }
    
    for (const FString& S : Paths)
    {
        FSoftObjectPath P(S);
        FQueuedAnim Q;
        Q.Path = P;
        Q.DisplayName = FText::FromString(P.GetAssetName());
        Q.bProcessed = ProcessedSet.Contains(S);
        if (const FString* CheckpointPath = CheckpointMap.Find(S))
        {
            Q.bCheckpointed = true;
            Q.CheckpointPath = *CheckpointPath;
        }
        Items.Add(MoveTemp(Q));
    }

    CurrentIndex = CheckBoundsIndex(CurrentIndex) ? CurrentIndex : INDEX_NONE;
    
    CachedProcessedCount = -1; // Invalidate cache on load
}

void FSeqQueue::Save() const
{
    TArray<FString> Paths;
    TArray<FString> ProcessedPaths;
    TArray<FString> CheckpointEntries;
    Paths.Reserve(Items.Num());
    for (const auto& Q : Items)
    {
        const FString PathString = Q.Path.ToString();
        Paths.Add(PathString);
        if (Q.bProcessed)
        {
            ProcessedPaths.Add(PathString);
        }
        if (Q.bCheckpointed && !Q.CheckpointPath.IsEmpty())
        {
            CheckpointEntries.Add(PathString + TEXT("=") + Q.CheckpointPath);
        }
    }

#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif
    GConfig->SetArray(SeqCfg::Section, SeqCfg::Key, Paths, Ini);
    GConfig->SetArray(SeqCfg::Section, TEXT("ProcessedQueue"), ProcessedPaths, Ini);
    GConfig->SetArray(SeqCfg::Section, TEXT("CheckpointQueue"), CheckpointEntries, Ini);
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

    // Cache is invalid, recalculate from queue state without loading every animation.
    int32 Count = 0;
    for (const FQueuedAnim& Item : Items)
    {
        if (Item.bProcessed)
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

int32 FSeqQueue::FindIndexByPath(const FSoftObjectPath& Path) const
{
    return Items.IndexOfByPredicate([&Path](const FQueuedAnim& Item)
    {
        return Item.Path == Path;
    });
}

bool FSeqQueue::IsProcessed(const FSoftObjectPath& Path) const
{
    const int32 Index = FindIndexByPath(Path);
    return Items.IsValidIndex(Index) && Items[Index].bProcessed;
}

bool FSeqQueue::TryGetCheckpointPath(const FSoftObjectPath& Path, FString& OutCheckpointPath) const
{
    OutCheckpointPath.Reset();

    const int32 Index = FindIndexByPath(Path);
    if (!Items.IsValidIndex(Index) || !Items[Index].bCheckpointed || Items[Index].CheckpointPath.IsEmpty())
    {
        return false;
    }

    OutCheckpointPath = Items[Index].CheckpointPath;
    return true;
}

void FSeqQueue::SetProcessed(const FSoftObjectPath& Path, bool bProcessed)
{
    const int32 Index = FindIndexByPath(Path);
    if (!Items.IsValidIndex(Index))
    {
        return;
    }

    Items[Index].bProcessed = bProcessed;
    if (bProcessed)
    {
        Items[Index].bCheckpointed = false;
        Items[Index].CheckpointPath.Reset();
    }

    CachedProcessedCount = -1;
    Save();
    QueueChanged.Broadcast();
}

void FSeqQueue::SetCheckpoint(const FSoftObjectPath& Path, const FString& CheckpointPath)
{
    const int32 Index = FindIndexByPath(Path);
    if (!Items.IsValidIndex(Index))
    {
        return;
    }

    Items[Index].bCheckpointed = !CheckpointPath.IsEmpty();
    Items[Index].CheckpointPath = CheckpointPath;
    Save();
    QueueChanged.Broadcast();
}

void FSeqQueue::ClearCheckpoint(const FSoftObjectPath& Path)
{
    const int32 Index = FindIndexByPath(Path);
    if (!Items.IsValidIndex(Index))
    {
        return;
    }

    Items[Index].bCheckpointed = false;
    Items[Index].CheckpointPath.Reset();
    Save();
    QueueChanged.Broadcast();
}
