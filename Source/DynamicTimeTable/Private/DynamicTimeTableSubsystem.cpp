#include "DynamicTimeTableSubsystem.h"

#include "DTTSaveDataActor.h"
#include "DynamicTimeTableChatCommand.h"
#include "Command/ChatCommandLibrary.h"
#include "FGTrain.h"
#include "FGRailroadTimeTable.h"
#include "FGTrainStationIdentifier.h"
#include "FGRailroadSubsystem.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogDynamicTimeTableSubsystem, Log, All);

namespace
{
    FString DTSStationName(const AFGTrainStationIdentifier* Identifier)
    {
        return IsValid(Identifier) ? Identifier->GetStationName().ToString() : TEXT("<none>");
    }
}

void UDynamicTimeTableSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogDynamicTimeTableSubsystem, Log, TEXT("DynamicTimeTable subsystem initialized."));
}

void UDynamicTimeTableSubsystem::Deinitialize()
{
    ActiveBookings.Reset();
    StationGroups.Reset();
    SaveDataActor.Reset();
    bPersistenceReady = false;
    Super::Deinitialize();
}

bool UDynamicTimeTableSubsystem::IsRuntimeGameWorld() const
{
    const UWorld* World = GetWorld();
    return IsValid(World) && !IsRunningCommandlet() &&
        (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDynamicTimeTableSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (IsRuntimeGameWorld())
    {
        InitializePersistence();
        RegisterDTTChatCommand();
    }
}

void UDynamicTimeTableSubsystem::RegisterDTTChatCommand()
{
    AChatCommandSubsystem* ChatSubsystem = AChatCommandSubsystem::Get(this);
    if (!IsValid(ChatSubsystem))
    {
        UE_LOG(LogDynamicTimeTableSubsystem, Warning, TEXT("dTT chat command registration skipped: chat subsystem unavailable."));
        return;
    }

    ChatSubsystem->RegisterCommand(
        TEXT("DynamicTimeTable"),
        ADynamicTimeTableChatCommand::StaticClass());

    UE_LOG(LogDynamicTimeTableSubsystem, Display, TEXT("Registered chat command: /dtt"));
}

void UDynamicTimeTableSubsystem::InitializePersistence()
{
    if (bPersistenceReady || !IsRuntimeGameWorld()) return;

    ADTTSaveDataActor* Found = nullptr;
    for (TActorIterator<ADTTSaveDataActor> It(GetWorld()); It; ++It)
    {
        Found = *It;
        break;
    }

    if (IsValid(Found))
    {
        SaveDataActor = Found;
        StationGroups = Found->GetGroups();
        UE_LOG(LogDynamicTimeTableSubsystem, Display,
            TEXT("Imported persistent dTT configuration. Groups=%d"), StationGroups.Num());
    }
    else if (GetWorld()->GetNetMode() != NM_Client)
    {
        FActorSpawnParameters Params;
        Params.Name = TEXT("DTT_SaveData");
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Found = GetWorld()->SpawnActor<ADTTSaveDataActor>(
            ADTTSaveDataActor::StaticClass(), FTransform::Identity, Params);
        SaveDataActor = Found;

        StationGroups.Reset();
        UE_LOG(LogDynamicTimeTableSubsystem, Display,
            TEXT("Created new DynamicTimeTable save-data actor with an empty configuration."));
    }

    bPersistenceReady = true;
    ResolveStationReferences();
    RecalculateBookingsFromVanilla();
    SyncConfigurationToSaveActor();
    OnGroupsChanged.Broadcast(); OnGroupsChangedNative.Broadcast();
}

void UDynamicTimeTableSubsystem::SyncConfigurationToSaveActor()
{
    if (SaveDataActor.IsValid() && GetWorld()->GetNetMode() != NM_Client)
    {
        SaveDataActor->SetGroups(StationGroups);
    }
}

FGuid UDynamicTimeTableSubsystem::CreateGroup(const FString& GroupName)
{
    FString Name = GroupName.TrimStartAndEnd();
    if (Name.IsEmpty()) Name = TEXT("New Station Group");
    FDTTStationGroup Group;
    Group.GroupId = FGuid::NewGuid();
    Group.GroupName = Name;
    StationGroups.Add(Group);
    ConfigurationChanged();
    UE_LOG(LogDynamicTimeTableSubsystem, Display, TEXT("Created group '%s' GUID=%s"), *Name, *Group.GroupId.ToString());
    return Group.GroupId;
}

bool UDynamicTimeTableSubsystem::RemoveGroup(FGuid GroupId)
{
    const int32 Removed = StationGroups.RemoveAll([GroupId](const FDTTStationGroup& G){ return G.GroupId == GroupId; });
    if (Removed == 0) return false;
    ConfigurationChanged();
    return true;
}

bool UDynamicTimeTableSubsystem::RenameGroup(FGuid GroupId, const FString& NewName)
{
    FDTTStationGroup* G = FindGroup(GroupId);
    const FString Name = NewName.TrimStartAndEnd();
    if (!G || Name.IsEmpty()) return false;
    G->GroupName = Name;
    ConfigurationChanged(false);
    return true;
}

bool UDynamicTimeTableSubsystem::AddStationByName(FGuid GroupId, const FString& StationName, int32 TargetSlots)
{
    FDTTStationGroup* G = FindGroup(GroupId);
    const FString Name = StationName.TrimStartAndEnd();
    if (!G || Name.IsEmpty() || FindStationEntry(*G, Name)) return false;
    for (const FDTTStationGroup& Other : StationGroups)
        if (Other.GroupId != GroupId && FindStationEntry(Other, Name)) return false;
    AFGTrainStationIdentifier* Id = FindVanillaStationByName(Name);
    if (!IsValid(Id)) return false;
    FDTTStationEntry E;
    E.DisplayName = Name;
    E.TargetSlots = FMath::Max(1, TargetSlots);
    E.PriorityIndex = G->Stations.Num();
    E.StationIdentifier = Id;
    G->Stations.Add(E);
    ConfigurationChanged();
    return true;
}

bool UDynamicTimeTableSubsystem::RemoveStationByName(FGuid GroupId, const FString& StationName)
{
    FDTTStationGroup* G = FindGroup(GroupId);
    if (!G) return false;
    const int32 Removed = G->Stations.RemoveAll([&](const FDTTStationEntry& E){ return E.DisplayName == StationName; });
    if (!Removed) return false;
    RefreshPriorityIndices(*G);
    ConfigurationChanged();
    return true;
}

bool UDynamicTimeTableSubsystem::SetTargetSlots(FGuid GroupId, const FString& StationName, int32 TargetSlots)
{
    FDTTStationGroup* G = FindGroup(GroupId);
    FDTTStationEntry* E = G ? FindStationEntry(*G, StationName) : nullptr;
    if (!E) return false;
    E->TargetSlots = FMath::Max(1, TargetSlots);
    ConfigurationChanged();
    return true;
}

void UDynamicTimeTableSubsystem::GetAllVanillaStationNames(TArray<FString>& Out) const
{
    Out.Reset();
    AFGRailroadSubsystem* R = AFGRailroadSubsystem::Get(GetWorld());
    if (!IsValid(R)) return;
    TArray<AFGTrainStationIdentifier*> Ids;
    R->GetAllTrainStations(Ids);
    for (AFGTrainStationIdentifier* Id : Ids) if (IsValid(Id)) Out.AddUnique(Id->GetStationName().ToString());
    Out.Sort();
}

const TArray<FDTTStationGroup>& UDynamicTimeTableSubsystem::GetStationGroups() const { return StationGroups; }

void UDynamicTimeTableSubsystem::GetGroupRuntimeLoad(FGuid GroupId, TArray<FDTTStationRuntimeLoad>& Out) const
{
    Out.Reset();
    const FDTTStationGroup* G = FindGroup(GroupId);
    if (!G) return;
    for (const FDTTStationEntry& E : G->Stations)
    {
        FDTTStationRuntimeLoad L;
        L.StationName=E.DisplayName; L.AssignedTrains=GetAssignedCount(E.StationIdentifier.Get());
        L.TargetSlots=FMath::Max(1,E.TargetSlots); L.Ratio=float(L.AssignedTrains)/float(L.TargetSlots);
        L.bOverloaded=L.AssignedTrains>L.TargetSlots; Out.Add(L);
    }
}
FDTTStationGroup* UDynamicTimeTableSubsystem::FindGroup(FGuid Id){ return StationGroups.FindByPredicate([&](const FDTTStationGroup& G){return G.GroupId==Id;}); }
const FDTTStationGroup* UDynamicTimeTableSubsystem::FindGroup(FGuid Id) const { return StationGroups.FindByPredicate([&](const FDTTStationGroup& G){return G.GroupId==Id;}); }
FDTTStationEntry* UDynamicTimeTableSubsystem::FindStationEntry(FDTTStationGroup& G,const FString& N){return G.Stations.FindByPredicate([&](const FDTTStationEntry&E){return E.DisplayName==N;});}
const FDTTStationEntry* UDynamicTimeTableSubsystem::FindStationEntry(const FDTTStationGroup& G,const FString& N) const{return G.Stations.FindByPredicate([&](const FDTTStationEntry&E){return E.DisplayName==N;});}

AFGTrainStationIdentifier* UDynamicTimeTableSubsystem::FindVanillaStationByName(const FString& Name) const
{
    AFGRailroadSubsystem* R=AFGRailroadSubsystem::Get(GetWorld()); if(!IsValid(R))return nullptr;
    TArray<AFGTrainStationIdentifier*> Ids; R->GetAllTrainStations(Ids);
    for(auto* Id:Ids) if(IsValid(Id)&&Id->GetStationName().ToString()==Name)return Id;
    return nullptr;
}

void UDynamicTimeTableSubsystem::ResolveStationReferences(){for(auto&G:StationGroups)for(auto&E:G.Stations)if(!E.StationIdentifier.IsValid())E.StationIdentifier=FindVanillaStationByName(E.DisplayName);}
void UDynamicTimeTableSubsystem::RefreshPriorityIndices(FDTTStationGroup& G){for(int32 i=0;i<G.Stations.Num();++i)G.Stations[i].PriorityIndex=i;}
void UDynamicTimeTableSubsystem::ConfigurationChanged(bool Recalc){++ConfigurationRevision;ResolveStationReferences();SyncConfigurationToSaveActor();if(Recalc)RecalculateBookingsFromVanilla();else OnGroupsChanged.Broadcast(); OnGroupsChangedNative.Broadcast();}
void UDynamicTimeTableSubsystem::RemoveInvalidBookings(){for(auto It=ActiveBookings.CreateIterator();It;++It)if(!IsValid(It.Key())||!IsValid(It.Value()))It.RemoveCurrent();}

void UDynamicTimeTableSubsystem::ReleaseBookingIfReached(AFGTrain* Train,AFGTrainStationIdentifier* Docked)
{
    if(auto** Existing=ActiveBookings.Find(Train)) if(*Existing==Docked){UE_LOG(LogDynamicTimeTableSubsystem,Verbose,TEXT("Released booking: Train='%s' BookedTarget='%s' ActuallyReached='%s'"),*Train->GetTrainName().ToString(),*DTSStationName(*Existing),*DTSStationName(Docked));ActiveBookings.Remove(Train);}
}

FDTTStationGroup* UDynamicTimeTableSubsystem::FindGroupByStation(AFGTrainStationIdentifier* Id){if(!IsValid(Id))return nullptr;return StationGroups.FindByPredicate([&](const FDTTStationGroup&G){return G.Stations.ContainsByPredicate([&](const FDTTStationEntry&E){return E.StationIdentifier.Get()==Id;});});}
const FDTTStationGroup* UDynamicTimeTableSubsystem::FindGroupByStation(AFGTrainStationIdentifier* Id) const{if(!IsValid(Id))return nullptr;return StationGroups.FindByPredicate([&](const FDTTStationGroup&G){return G.Stations.ContainsByPredicate([&](const FDTTStationEntry&E){return E.StationIdentifier.Get()==Id;});});}
int32 UDynamicTimeTableSubsystem::GetAssignedCount(AFGTrainStationIdentifier* Id) const{int32 C=0;for(const auto&P:ActiveBookings)if(IsValid(P.Key)&&P.Value==Id)++C;return C;}

FDTTStationEntry* UDynamicTimeTableSubsystem::SelectBestStation(FDTTStationGroup& G)
{
    FDTTStationEntry* Best=nullptr;double BestRatio=TNumericLimits<double>::Max();
    for(auto&E:G.Stations){auto*Id=E.StationIdentifier.Get();if(!IsValid(Id)||E.TargetSlots<=0)continue;const int32 C=GetAssignedCount(Id);const double R=double(C)/double(E.TargetSlots);UE_LOG(LogDynamicTimeTableSubsystem,VeryVerbose,TEXT("Candidate='%s' Assigned=%d TargetSlots=%d Ratio=%.3f"),*E.DisplayName,C,E.TargetSlots,R);if(!Best||R<BestRatio){Best=&E;BestRatio=R;}}
    return Best;
}

bool UDynamicTimeTableSubsystem::ReplaceCurrentStop(AFGRailroadTimeTable*T,int32 I,AFGTrainStationIdentifier*Id)const{if(!IsValid(T)||!IsValid(Id)||!T->IsValidStop(I))return false;TArray<FTimeTableStop>S;T->GetStops(S);if(!S.IsValidIndex(I))return false;S[I].Station=Id;return T->SetStops(S);}
void UDynamicTimeTableSubsystem::HandleTrainDocked(AFGTrain*Train,AFGBuildableRailroadStation*DockedStation)
{
    if(!IsValid(Train)||!IsValid(DockedStation))return;if(!bPersistenceReady)InitializePersistence();RemoveInvalidBookings();ResolveStationReferences();auto*Docked=DockedStation->GetStationIdentifier();ReleaseBookingIfReached(Train,Docked);auto*T=Train->GetTimeTable();const int32 I=IsValid(T)?T->GetCurrentStop():INDEX_NONE;if(!IsValid(T)||!T->IsValidStop(I))return;auto*Ref=T->GetStop(I).Station.Get();auto*G=FindGroupByStation(Ref);UE_LOG(LogDynamicTimeTableSubsystem,VeryVerbose,TEXT("dTT Docked: Train='%s' Station='%s' CurrentStop=%d Target='%s' Group=%s"),*Train->GetTrainName().ToString(),*DTSStationName(Docked),I,*DTSStationName(Ref),G?*G->GroupName:TEXT("<none>"));if(!G||ActiveBookings.Contains(Train))return;auto*Sel=SelectBestStation(*G);if(!Sel||!Sel->StationIdentifier.IsValid())return;auto*Id=Sel->StationIdentifier.Get();const bool Ok=ReplaceCurrentStop(T,I,Id);const FString Verified=T->IsValidStop(I)?DTSStationName(T->GetStop(I).Station.Get()):TEXT("<invalid>");if(Ok&&Verified==Sel->DisplayName){ActiveBookings.Add(Train,Id);const int32 C=GetAssignedCount(Id);UE_LOG(LogDynamicTimeTableSubsystem,Verbose,TEXT("ASSIGNED Train='%s' Selected='%s' Verified='%s' Load=%d/%d%s"),*Train->GetTrainName().ToString(),*Sel->DisplayName,*Verified,C,Sel->TargetSlots,C>Sel->TargetSlots?TEXT(" OVERLOAD"):TEXT(""));}OnGroupsChanged.Broadcast(); OnGroupsChangedNative.Broadcast();
}

void UDynamicTimeTableSubsystem::RecalculateBookingsFromVanilla()
{
    ActiveBookings.Reset();ResolveStationReferences();AFGRailroadSubsystem*R=AFGRailroadSubsystem::Get(GetWorld());if(!IsValid(R)){OnGroupsChanged.Broadcast(); OnGroupsChangedNative.Broadcast();return;}TArray<AFGTrain*>Trains;R->GetAllTrains(Trains);for(AFGTrain*Train:Trains){if(!IsValid(Train))continue;auto*T=Train->GetTimeTable();const int32 I=IsValid(T)?T->GetCurrentStop():INDEX_NONE;if(IsValid(T)&&T->IsValidStop(I)){auto*Target=T->GetStop(I).Station.Get();if(FindGroupByStation(Target))ActiveBookings.Add(Train,Target);}}UE_LOG(LogDynamicTimeTableSubsystem,Display,TEXT("Recalculated %d active booking(s) from Vanilla timetables."),ActiveBookings.Num());OnGroupsChanged.Broadcast(); OnGroupsChangedNative.Broadcast();
}

