#include "DynamicTimeTableRemoteCallObject.h"
#include "DynamicTimeTableSubsystem.h"
#include "DynamicTimeTableWidget.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"
void UDynamicTimeTableRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UDynamicTimeTableRemoteCallObject,bReplicationAnchor);}
bool UDynamicTimeTableRemoteCallObject::Check(int32 V){if(V==ProtocolVersion)return true;Client_OperationResult(false,TEXT("dTT client/server protocol mismatch."));return false;}
void UDynamicTimeTableRemoteCallObject::SendSnapshot(){FDTTClientSnapshot X;X.ProtocolVersion=ProtocolVersion;if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>()){X.Revision=S->GetConfigurationRevision();X.Groups=S->GetStationGroups();S->GetAllVanillaStationNames(X.AvailableStations);for(const auto&G:X.Groups){TArray<FDTTStationRuntimeLoad>L;S->GetGroupRuntimeLoad(G.GroupId,L);X.RuntimeLoads.Append(L);}}Client_ReceiveSnapshot(X);}
void UDynamicTimeTableRemoteCallObject::RequestSnapshot(){Server_RequestSnapshot(ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestCreateGroup(const FString&N){Server_CreateGroup(N,ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestRenameGroup(FGuid G,const FString&N){Server_RenameGroup(G,N,ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestDeleteGroup(FGuid G){Server_DeleteGroup(G,ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestAddStation(FGuid G,const FString&N){Server_AddStation(G,N,ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestRemoveStation(FGuid G,const FString&N){Server_RemoveStation(G,N,ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestSetTargetSlots(FGuid G,const FString&N,int32 V){Server_SetTargetSlots(G,N,FMath::Clamp(V,1,999),ProtocolVersion);}void UDynamicTimeTableRemoteCallObject::RequestRecalculate(){Server_Recalculate(ProtocolVersion);}
void UDynamicTimeTableRemoteCallObject::Server_RequestSnapshot_Implementation(int32 V){if(Check(V))SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_CreateGroup_Implementation(const FString&N,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->CreateGroup(N.TrimStartAndEnd().Left(64)).IsValid();Client_OperationResult(O,O?TEXT("Group created."):TEXT("Create failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_RenameGroup_Implementation(FGuid G,const FString&N,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->RenameGroup(G,N.TrimStartAndEnd().Left(64));Client_OperationResult(O,O?TEXT("Group renamed."):TEXT("Rename failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_DeleteGroup_Implementation(FGuid G,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->RemoveGroup(G);Client_OperationResult(O,O?TEXT("Group deleted."):TEXT("Delete failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_AddStation_Implementation(FGuid G,const FString&N,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->AddStationByName(G,N,1);Client_OperationResult(O,O?TEXT("Station added."):TEXT("Add failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_RemoveStation_Implementation(FGuid G,const FString&N,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->RemoveStationByName(G,N);Client_OperationResult(O,O?TEXT("Station removed."):TEXT("Remove failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_SetTargetSlots_Implementation(FGuid G,const FString&N,int32 Slots,int32 V){if(!Check(V))return;auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>();bool O=S&&S->SetTargetSlots(G,N,FMath::Clamp(Slots,1,999));Client_OperationResult(O,O?TEXT("Slots updated."):TEXT("Update failed."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Server_Recalculate_Implementation(int32 V){if(!Check(V))return;if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->RecalculateBookingsFromVanilla();Client_OperationResult(true,TEXT("Bookings recalculated."));SendSnapshot();}
void UDynamicTimeTableRemoteCallObject::Client_ReceiveSnapshot_Implementation(const FDTTClientSnapshot&X){CachedSnapshot=X;OnSnapshotChanged.Broadcast();}
void UDynamicTimeTableRemoteCallObject::Client_OperationResult_Implementation(bool O,const FString&M){OnOperationResult.Broadcast(O,M);}
void UDynamicTimeTableRemoteCallObject::Client_OpenWindow_Implementation()
{
    AFGPlayerController* PC = GetTypedOuter<AFGPlayerController>();
    if (!IsValid(PC) || !PC->IsLocalController()) return;

    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        PC, Widgets, UDynamicTimeTableWidget::StaticClass(), true);

    for (UUserWidget* Widget : Widgets)
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().ClearTimer(LiveUpdateTimerHandle);
            }
            return;
        }
    }

    UDynamicTimeTableWidget* Window = CreateWidget<UDynamicTimeTableWidget>(
        PC, UDynamicTimeTableWidget::StaticClass());
    if (!IsValid(Window)) return;

    Window->AddToViewport(1000);
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(Window->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(Mode);
    PC->SetShowMouseCursor(true);
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;
    PC->CurrentMouseCursor = EMouseCursor::Default;
    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);

    StartLiveUpdates();
}

void UDynamicTimeTableRemoteCallObject::StartLiveUpdates()
{
    UWorld* World = GetWorld();
    if (!IsValid(World)) return;

    RequestSnapshot();
    World->GetTimerManager().ClearTimer(LiveUpdateTimerHandle);
    World->GetTimerManager().SetTimer(
        LiveUpdateTimerHandle,
        this,
        &UDynamicTimeTableRemoteCallObject::PollLiveSnapshot,
        1.5f,
        true);
}

void UDynamicTimeTableRemoteCallObject::PollLiveSnapshot()
{
    AFGPlayerController* PC = GetTypedOuter<AFGPlayerController>();
    UWorld* World = GetWorld();
    if (!IsValid(PC) || !IsValid(World)) return;

    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        PC, Widgets, UDynamicTimeTableWidget::StaticClass(), true);

    const bool bWindowOpen = Widgets.ContainsByPredicate(
        [](const UUserWidget* Widget)
        {
            return IsValid(Widget) && Widget->IsInViewport();
        });

    if (!bWindowOpen)
    {
        World->GetTimerManager().ClearTimer(LiveUpdateTimerHandle);
        return;
    }

    RequestSnapshot();
}
