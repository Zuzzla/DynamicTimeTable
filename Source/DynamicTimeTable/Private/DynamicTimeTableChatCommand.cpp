#include "DynamicTimeTableChatCommand.h"

#include "Command/CommandSender.h"
#include "DynamicTimeTableSubsystem.h"
#include "DynamicTimeTableWidget.h"
#include "DynamicTimeTableRemoteCallObject.h"
#include "UI/FGGameUI.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "FGPlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDynamicTimeTableChatCommand, Log, All);

ADynamicTimeTableChatCommand::ADynamicTimeTableChatCommand()
{
    CommandName = TEXT("dtt");
    Usage = NSLOCTEXT(
        "DynamicTimeTable",
        "ChatCommand.DTT.Usage",
        "/dtt - Open DynamicTimeTable; /dtt recalc - rebuild active bookings");
    Aliases.Add(TEXT("dynamictimetable"));
}

EExecutionStatus ADynamicTimeTableChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender,
    const TArray<FString>& Arguments,
    const FString& Label)
{
    if (!IsValid(Sender) || !Sender->IsPlayerSender())
    {
        if (IsValid(Sender))
        {
            Sender->SendChatMessage(
                TEXT("/dtt can only be used by a player."),
                FLinearColor::Red);
        }
        return EExecutionStatus::UNCOMPLETED;
    }

    AFGPlayerController* Player = Sender->GetPlayer();
    UWorld* World = IsValid(Player) ? Player->GetWorld() : nullptr;
    UDynamicTimeTableSubsystem* Subsystem = IsValid(World)
        ? World->GetSubsystem<UDynamicTimeTableSubsystem>()
        : nullptr;

    if (!IsValid(Subsystem))
    {
        Sender->SendChatMessage(
            TEXT("DynamicTimeTable is not available in this world."),
            FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (Arguments.Num() >= 2 &&
        Arguments[1].Equals(TEXT("recalc"), ESearchCase::IgnoreCase))
    {
        Subsystem->RecalculateBookingsFromVanilla();
        Sender->SendChatMessage(
            TEXT("dTT: Active bookings were rebuilt from Vanilla timetables."),
            FLinearColor(0.20f, 0.80f, 1.00f));
        return EExecutionStatus::COMPLETED;
    }

    if (!Player->IsLocalController())
    {
        UDynamicTimeTableRemoteCallObject* RemoteCallObject = Cast<UDynamicTimeTableRemoteCallObject>(
            Player->GetRemoteCallObjectOfClass(UDynamicTimeTableRemoteCallObject::StaticClass()));
        if (!IsValid(RemoteCallObject))
        {
            RemoteCallObject = Cast<UDynamicTimeTableRemoteCallObject>(
                Player->RegisterRemoteCallObjectClass(UDynamicTimeTableRemoteCallObject::StaticClass()));
        }
        if (!IsValid(RemoteCallObject))
        {
            Sender->SendChatMessage(TEXT("dTT: Multiplayer UI channel is not ready."), FLinearColor::Red);
            return EExecutionStatus::UNCOMPLETED;
        }
        RemoteCallObject->Client_OpenWindow();
        return EExecutionStatus::COMPLETED;
    }

    UFGGameUI* GameUI = Player->GetGameUI();
    if (!IsValid(GameUI))
    {
        Sender->SendChatMessage(TEXT("dTT: Game UI is not ready."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    TArray<UUserWidget*> ExistingWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        Player,
        ExistingWidgets,
        UDynamicTimeTableWidget::StaticClass(),
        true);

    for (UUserWidget* ExistingWidget : ExistingWidgets)
    {
        if (IsValid(ExistingWidget))
        {
            ExistingWidget->RemoveFromParent();
            UE_LOG(LogDynamicTimeTableChatCommand, Verbose, TEXT("dTT: existing virtual window closed by /dtt toggle."));
            return EExecutionStatus::COMPLETED;
        }
    }

    UDynamicTimeTableWidget* Window = CreateWidget<UDynamicTimeTableWidget>(
        Player,
        UDynamicTimeTableWidget::StaticClass());
    if (!IsValid(Window))
    {
        Sender->SendChatMessage(TEXT("dTT: Could not create the virtual window."), FLinearColor::Red);
        UE_LOG(LogDynamicTimeTableChatCommand, Error, TEXT("dTT: CreateWidget returned null."));
        return EExecutionStatus::UNCOMPLETED;
    }

    // AddToViewport is used deliberately here. The Satisfactory AddInteractWidget
    // Blueprint event only updates the stack for native widgets in this build and
    // did not attach the pure C++ widget to the viewport.
    Window->AddToViewport(1000);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(Window->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    Player->SetInputMode(InputMode);
    Player->SetShowMouseCursor(true);
    Player->bEnableClickEvents = true;
    Player->bEnableMouseOverEvents = true;
    Player->CurrentMouseCursor = EMouseCursor::Default;
    Player->SetIgnoreMoveInput(true);
    Player->SetIgnoreLookInput(true);

    UE_LOG(LogDynamicTimeTableChatCommand, Verbose, TEXT("dTT: virtual window attached directly to viewport."));
    return EExecutionStatus::COMPLETED;
}

