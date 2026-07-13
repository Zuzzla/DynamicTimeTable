#pragma once

#include "CoreMinimal.h"
#include "DynamicTimeTableTypes.generated.h"

class AFGTrainStationIdentifier;

USTRUCT(BlueprintType)
struct FDTTStationEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Dynamic Time Table")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Dynamic Time Table", meta = (ClampMin = "1"))
    int32 TargetSlots = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Dynamic Time Table")
    int32 PriorityIndex = 0;

    // Runtime-only. Re-resolved from DisplayName after loading.
    TWeakObjectPtr<AFGTrainStationIdentifier> StationIdentifier;
};

USTRUCT(BlueprintType)
struct FDTTStationGroup
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Dynamic Time Table")
    FGuid GroupId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Dynamic Time Table")
    FString GroupName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Dynamic Time Table")
    TArray<FDTTStationEntry> Stations;
};

USTRUCT(BlueprintType)
struct FDTTStationRuntimeLoad
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dynamic Time Table")
    FString StationName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dynamic Time Table")
    int32 AssignedTrains = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dynamic Time Table")
    int32 TargetSlots = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dynamic Time Table")
    float Ratio = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dynamic Time Table")
    bool bOverloaded = false;
};
