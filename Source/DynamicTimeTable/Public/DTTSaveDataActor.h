#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "GameFramework/Actor.h"
#include "DynamicTimeTableTypes.h"
#include "DTTSaveDataActor.generated.h"

UCLASS(NotBlueprintable, NotPlaceable)
class DYNAMICTIMETABLE_API ADTTSaveDataActor : public AActor, public IFGSaveInterface
{
    GENERATED_BODY()

public:
    ADTTSaveDataActor();

    virtual void BeginPlay() override;

    virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
    virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
    virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
    virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
    virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
    virtual bool NeedTransform_Implementation() override { return false; }
    virtual bool ShouldSave_Implementation() const override { return true; }

    void SetGroups(const TArray<FDTTStationGroup>& InGroups);
    const TArray<FDTTStationGroup>& GetGroups() const { return SavedGroups; }

private:
    UPROPERTY(SaveGame)
    int32 DataVersion = 1;

    UPROPERTY(SaveGame)
    TArray<FDTTStationGroup> SavedGroups;
};
