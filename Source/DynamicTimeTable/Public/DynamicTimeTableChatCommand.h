#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "DynamicTimeTableChatCommand.generated.h"

UCLASS(NotBlueprintable)
class DYNAMICTIMETABLE_API ADynamicTimeTableChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()

public:
    ADynamicTimeTableChatCommand();

    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};
