#pragma once
#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "DynamicTimeTableTypes.h"
#include "DynamicTimeTableRemoteCallObject.generated.h"
USTRUCT(BlueprintType)
struct FDTTClientSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 ProtocolVersion = 2;
    UPROPERTY(BlueprintReadOnly) int32 Revision = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FDTTStationGroup> Groups;
    UPROPERTY(BlueprintReadOnly) TArray<FString> AvailableStations;
    UPROPERTY(BlueprintReadOnly) TArray<FDTTStationRuntimeLoad> RuntimeLoads;
};
DECLARE_MULTICAST_DELEGATE(FDTTClientSnapshotChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDTTClientOperationResult, bool, const FString&);
UCLASS()
class DYNAMICTIMETABLE_API UDynamicTimeTableRemoteCallObject : public UFGRemoteCallObject
{
    GENERATED_BODY()
public:
    static constexpr int32 ProtocolVersion = 2;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    const FDTTClientSnapshot& GetSnapshot() const { return CachedSnapshot; }
    FDTTClientSnapshotChanged OnSnapshotChanged;
    FDTTClientOperationResult OnOperationResult;
    void RequestSnapshot();
    void RequestCreateGroup(const FString& GroupName);
    void RequestRenameGroup(FGuid GroupId, const FString& NewName);
    void RequestDeleteGroup(FGuid GroupId);
    void RequestAddStation(FGuid GroupId, const FString& StationName);
    void RequestRemoveStation(FGuid GroupId, const FString& StationName);
    void RequestSetTargetSlots(FGuid GroupId, const FString& StationName, int32 TargetSlots);
    void RequestRecalculate();
    UFUNCTION(Client, Reliable)
    void Client_OpenWindow();
private:
    UPROPERTY(Replicated) bool bReplicationAnchor = true;
    UPROPERTY() FDTTClientSnapshot CachedSnapshot;
    UFUNCTION(Server, Reliable) void Server_RequestSnapshot(int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_CreateGroup(const FString& GroupName, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_RenameGroup(FGuid GroupId, const FString& NewName, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_DeleteGroup(FGuid GroupId, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_AddStation(FGuid GroupId, const FString& StationName, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_RemoveStation(FGuid GroupId, const FString& StationName, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_SetTargetSlots(FGuid GroupId, const FString& StationName, int32 TargetSlots, int32 ClientProtocol);
    UFUNCTION(Server, Reliable) void Server_Recalculate(int32 ClientProtocol);
    UFUNCTION(Client, Reliable) void Client_ReceiveSnapshot(const FDTTClientSnapshot& Snapshot);
    UFUNCTION(Client, Reliable) void Client_OperationResult(bool bSuccess, const FString& Message);
    bool Check(int32 ClientProtocol);
    void SendSnapshot();
    void StartLiveUpdates();
    void PollLiveSnapshot();

    FTimerHandle LiveUpdateTimerHandle;
};
