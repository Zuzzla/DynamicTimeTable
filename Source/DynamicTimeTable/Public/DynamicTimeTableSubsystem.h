#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DynamicTimeTableTypes.h"
#include "DynamicTimeTableSubsystem.generated.h"

class AFGTrain;
class AFGBuildableRailroadStation;
class AFGTrainStationIdentifier;
class AFGRailroadTimeTable;
class ADTTSaveDataActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDTTGroupsChangedDelegate);
DECLARE_MULTICAST_DELEGATE(FDTTGroupsChangedNativeDelegate);

UCLASS(BlueprintType)
class DYNAMICTIMETABLE_API UDynamicTimeTableSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    void HandleTrainDocked(AFGTrain* Train, AFGBuildableRailroadStation* DockedStation);

    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") FGuid CreateGroup(const FString& GroupName);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") bool RemoveGroup(FGuid GroupId);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") bool RenameGroup(FGuid GroupId, const FString& NewName);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") bool AddStationByName(FGuid GroupId, const FString& StationName, int32 TargetSlots = 1);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") bool RemoveStationByName(FGuid GroupId, const FString& StationName);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") bool SetTargetSlots(FGuid GroupId, const FString& StationName, int32 TargetSlots);
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") void GetAllVanillaStationNames(TArray<FString>& OutStationNames) const;
    UFUNCTION(BlueprintPure, Category = "Dynamic Time Table|Groups") const TArray<FDTTStationGroup>& GetStationGroups() const;
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Groups") void GetGroupRuntimeLoad(FGuid GroupId, TArray<FDTTStationRuntimeLoad>& OutLoads) const;
    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table|Runtime") void RecalculateBookingsFromVanilla();
    int32 GetConfigurationRevision() const { return ConfigurationRevision; }

    UPROPERTY(BlueprintAssignable, Category = "Dynamic Time Table")
    FDTTGroupsChangedDelegate OnGroupsChanged;

    FDTTGroupsChangedNativeDelegate OnGroupsChangedNative;

private:
    TArray<FDTTStationGroup> StationGroups;
    TMap<AFGTrain*, AFGTrainStationIdentifier*> ActiveBookings;
    TWeakObjectPtr<ADTTSaveDataActor> SaveDataActor;
    bool bPersistenceReady = false;
    int32 ConfigurationRevision = 0;

    void InitializePersistence();
    void RegisterDTTChatCommand();
    void SyncConfigurationToSaveActor();
    bool IsRuntimeGameWorld() const;

    FDTTStationGroup* FindGroup(FGuid GroupId);
    const FDTTStationGroup* FindGroup(FGuid GroupId) const;
    FDTTStationGroup* FindGroupByStation(AFGTrainStationIdentifier* Identifier);
    const FDTTStationGroup* FindGroupByStation(AFGTrainStationIdentifier* Identifier) const;
    FDTTStationEntry* FindStationEntry(FDTTStationGroup& Group, const FString& StationName);
    const FDTTStationEntry* FindStationEntry(const FDTTStationGroup& Group, const FString& StationName) const;
    AFGTrainStationIdentifier* FindVanillaStationByName(const FString& StationName) const;
    void ResolveStationReferences();
    void RemoveInvalidBookings();
    void ReleaseBookingIfReached(AFGTrain* Train, AFGTrainStationIdentifier* DockedIdentifier);
    void RefreshPriorityIndices(FDTTStationGroup& Group);
    void ConfigurationChanged(bool bRecalculateBookings = true);
    FDTTStationEntry* SelectBestStation(FDTTStationGroup& Group);
    int32 GetAssignedCount(AFGTrainStationIdentifier* Identifier) const;
    bool ReplaceCurrentStop(AFGRailroadTimeTable* TimeTable, int32 CurrentStop, AFGTrainStationIdentifier* NewIdentifier) const;
};
