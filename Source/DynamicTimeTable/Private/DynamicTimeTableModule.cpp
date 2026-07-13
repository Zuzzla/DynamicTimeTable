#include "DynamicTimeTableModule.h"
#include "Modules/ModuleManager.h"
#if !WITH_EDITOR
#include "Patching/NativeHookManager.h"
#include "FGTrain.h"
#include "FGPlayerController.h"
#include "FGRemoteCallObject.h"
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
SUBSCRIBE_METHOD_AFTER(AFGTrain::OnDocked,[](AFGTrain* Train,AFGBuildableRailroadStation* Station){if(!IsValid(Train)||!IsValid(Train->GetWorld()))return;if(UDynamicTimeTableSubsystem* S=Train->GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->HandleTrainDocked(Train,Station);});
#endif
}
void FDynamicTimeTableModule::ShutdownModule(){
#if !WITH_EDITOR
if(PlayerControllerBegunPlayHandle.IsValid()){AFGPlayerController::PlayerControllerBegunPlay.Remove(PlayerControllerBegunPlayHandle);PlayerControllerBegunPlayHandle.Reset();}
#endif
UE_LOG(LogDynamicTimeTableModule,Log,TEXT("DynamicTimeTable module shut down."));}
IMPLEMENT_MODULE(FDynamicTimeTableModule,DynamicTimeTable)
