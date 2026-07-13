#include "DTTSaveDataActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDynamicTimeTableSaveData, Log, All);

ADTTSaveDataActor::ADTTSaveDataActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
    SetReplicates(false);
    bNetLoadOnClient = false;
}

void ADTTSaveDataActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogDynamicTimeTableSaveData, Display,
        TEXT("dTT save-data actor active. Version=%d Groups=%d"),
        DataVersion, SavedGroups.Num());
}

void ADTTSaveDataActor::SetGroups(const TArray<FDTTStationGroup>& InGroups)
{
    SavedGroups = InGroups;
}

void ADTTSaveDataActor::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
    UE_LOG(LogDynamicTimeTableSaveData, Display,
        TEXT("Saving dTT configuration. Groups=%d DataVersion=%d"),
        SavedGroups.Num(), DataVersion);
}

void ADTTSaveDataActor::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ADTTSaveDataActor::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ADTTSaveDataActor::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
    UE_LOG(LogDynamicTimeTableSaveData, Display,
        TEXT("Loaded dTT configuration. Groups=%d DataVersion=%d"),
        SavedGroups.Num(), DataVersion);
}

void ADTTSaveDataActor::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
    // Configuration contains only value types and station names.
}

