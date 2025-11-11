#pragma once
#include "CoreMinimal.h"

/** Storage format for each queued animation */
struct FQueuedAnim
{
    FSoftObjectPath Path;
    FText           DisplayName; // nice name for UI

    bool operator==(const FQueuedAnim& Other) const
    { return Path == Other.Path; }
};

namespace SeqCfg
{
    inline constexpr const TCHAR* Section = TEXT("ToucanSequencer");
    inline constexpr const TCHAR* Key     = TEXT("Queue"); // array of soft paths
    inline constexpr const TCHAR* CurrentIndexKey = TEXT("CurrentIndex");
}

/** Tiny, editor-only, in-memory queue with config persistence */
class FSeqQueue
{
public:
    static FSeqQueue& Get()
    {
        static FSeqQueue S;
        return S;
    }

    DECLARE_MULTICAST_DELEGATE(FOnQueueChanged);
    FOnQueueChanged& OnQueueChanged() { return QueueChanged; }

    void Load();
    void Save() const;

    void Clear();
    void Add(const FAssetData& A);
    void AddPath(const FSoftObjectPath& P, const FText& Nice);
    bool RemoveAt(int32 Index);
    const TArray<FQueuedAnim>& GetAll() const { return Items; }

    int32 GetCurrentIndex() const { return CurrentIndex; }
    void SetCurrentIndex(int32 NewIndex);
    
private:
    int32 CurrentIndex = INDEX_NONE;
    FSeqQueue() { Load(); }

    bool CheckBoundsIndex(int32 Index) const { return Index >= 0 && Index < Items.Num(); }

    static FText MakeDisplayName(const FAssetData& A);

private:
    TArray<FQueuedAnim> Items;
    FOnQueueChanged QueueChanged;

};
