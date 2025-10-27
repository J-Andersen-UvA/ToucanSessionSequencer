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

    void Load();
    void Save() const;

    void Clear()                  { Items.Reset(); }
    void Add(const FAssetData& A);
    void AddPath(const FSoftObjectPath& P, const FText& Nice);
    bool RemoveAt(int32 Index);
    const TArray<FQueuedAnim>& GetAll() const { return Items; }

private:
    FSeqQueue() { Load(); }

    static FText MakeDisplayName(const FAssetData& A);

private:
    TArray<FQueuedAnim> Items;
};
