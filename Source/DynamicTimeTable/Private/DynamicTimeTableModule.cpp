#include "DynamicTimeTableModule.h"
#include "Modules/ModuleManager.h"
#if !WITH_EDITOR
#include "Patching/NativeHookManager.h"
#include "FGTrain.h"
#include "FGPlayerController.h"
#include "FGRemoteCallObject.h"
#include "FGRailroadTimeTable.h"
#include "FGTrainStationIdentifier.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "DynamicTimeTableSubsystem.h"
#include "DynamicTimeTableRemoteCallObject.h"
#include "Engine/World.h"
#endif
DEFINE_LOG_CATEGORY_STATIC(LogDynamicTimeTableModule,Log,All);
void FDynamicTimeTableModule::StartupModule(){UE_LOG(LogDynamicTimeTableModule,Log,TEXT("DynamicTimeTable module started."));
#if WITH_EDITOR
UE_LOG(LogDynamicTimeTableModule,Log,TEXT("Editor build: runtime registration disabled."));
#else
PlayerControllerBegunPlayHandle=AFGPlayerController::PlayerControllerBegunPlay.AddLambda([](AFGPlayerController* PC){if(!IsValid(PC))return;UFGRemoteCallObject* R=PC->RegisterRemoteCallObjectClass(UDynamicTimeTableRemoteCallObject::StaticClass());UE_LOG(LogDynamicTimeTableModule,Log,TEXT("dTT RCO registration. Success=%s"),IsValid(R)?TEXT("true"):TEXT("false"));});
SUBSCRIBE_METHOD_AFTER(AFGTrain::OnDocked,[](AFGTrain* Train,AFGBuildableRailroadStation* Station){if(IsValid(Train)&&IsValid(Train->GetWorld()))if(auto* S=Train->GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->HandleTrainDocked(Train,Station);});
SUBSCRIBE_METHOD_AFTER(AFGRailroadTimeTable::SetStops,[](const bool& Ok,AFGRailroadTimeTable* T,const TArray<FTimeTableStop>& Stops){(void)Stops;if(Ok&&IsValid(T)&&IsValid(T->GetWorld()))if(auto* S=T->GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->HandleTimeTableChanged(T);});
SUBSCRIBE_METHOD_AFTER(AFGTrainStationIdentifier::SetStationName,[](AFGTrainStationIdentifier* Id,const FText& Name){(void)Name;if(IsValid(Id)&&IsValid(Id->GetWorld()))if(auto* S=Id->GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->HandleStationNameChanged(Id);});
SUBSCRIBE_METHOD_AFTER(AFGTrain::SetSelfDrivingEnabled,[](AFGTrain* Train,bool Enabled){if(IsValid(Train)&&IsValid(Train->GetWorld()))if(auto* S=Train->GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->HandleSelfDrivingChanged(Train,Enabled);});
#endif
}
void FDynamicTimeTableModule::ShutdownModule(){
#if !WITH_EDITOR
if(PlayerControllerBegunPlayHandle.IsValid()){AFGPlayerController::PlayerControllerBegunPlay.Remove(PlayerControllerBegunPlayHandle);PlayerControllerBegunPlayHandle.Reset();}
#endif
UE_LOG(LogDynamicTimeTableModule,Log,TEXT("DynamicTimeTable module shut down."));}
IMPLEMENT_MODULE(FDynamicTimeTableModule,DynamicTimeTable)
