#pragma once

#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "DynamicTimeTableWidget.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UScrollBox;
class USpinBox;
class UTextBlock;
class UDynamicTimeTableRemoteCallObject;

UCLASS(NotBlueprintable)
class DYNAMICTIMETABLE_API UDynamicTimeTableWidget : public UFGInteractWidget
{
    GENERATED_BODY()

public:
    UDynamicTimeTableWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;
    virtual void OnEscapePressed_Implementation() override;

    UFUNCTION(BlueprintCallable, Category = "Dynamic Time Table")
    void RefreshData();

private:
    UPROPERTY() TObjectPtr<UScrollBox> GroupScrollBox;
    UPROPERTY() TObjectPtr<UTextBlock> SummaryText;
    UPROPERTY() TObjectPtr<UTextBlock> EditorStatusText;
    UPROPERTY() TObjectPtr<UButton> CloseButton;
    UPROPERTY() TObjectPtr<UButton> RecalculateButton;

    UPROPERTY() TObjectPtr<UComboBoxString> GroupSelector;
    UPROPERTY() TObjectPtr<UEditableTextBox> GroupNameInput;
    UPROPERTY() TObjectPtr<UEditableTextBox> NewGroupNameInput;
    UPROPERTY() TObjectPtr<UButton> CreateGroupButton;
    UPROPERTY() TObjectPtr<UButton> RenameGroupButton;
    UPROPERTY() TObjectPtr<UButton> DeleteGroupButton;

    UPROPERTY() TObjectPtr<UComboBoxString> AvailableStationSelector;
    UPROPERTY() TObjectPtr<UComboBoxString> GroupStationSelector;
    UPROPERTY() TObjectPtr<USpinBox> TargetSlotsInput;
    UPROPERTY() TObjectPtr<UButton> AddStationButton;
    UPROPERTY() TObjectPtr<UButton> ApplySlotsButton;
    UPROPERTY() TObjectPtr<UButton> RemoveStationButton;

    TArray<FGuid> GroupOptionIds;
    FGuid SelectedGroupId;
    FDelegateHandle GroupsChangedHandle;
    FDelegateHandle SnapshotChangedHandle;
    FDelegateHandle OperationResultHandle;
    UPROPERTY() TObjectPtr<UDynamicTimeTableRemoteCallObject> RemoteCallObject;
    UPROPERTY() TArray<TObjectPtr<UBorder>> RuntimeGroupBorders;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> RuntimeGroupTexts;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> RuntimeStationTexts;
    TArray<FGuid> RuntimeGroupIds;
    TArray<int32> RuntimeStationCounts;
    TArray<FString> RuntimeStationNames;
    bool bInitialFocusApplied = false;
    bool bRefreshingControls = false;
    bool bHasAppliedRemoteConfiguration = false;
    bool bForceFullRefreshOnNextSnapshot = false;
    int32 LastAppliedConfigurationRevision = INDEX_NONE;
    float LocalLiveRefreshAccumulator = 0.0f;

    void BuildWidgetTree();
    void BuildEditorPanel(class UVerticalBox* Main);
    void CloseWindow();
    void RefreshRuntimeDisplay();
    void RebuildRuntimeDisplay();
    void UpdateRuntimeDisplayInPlace();
    bool RuntimeLayoutMatches() const;
    void RefreshEditorControls();
    void SetEditorStatus(const FString& Message, bool bError = false);
    FString GetSelectedGroupStationName() const;
    bool IsRemoteClient() const;
    const TArray<FDTTStationGroup>& ViewGroups() const;
    void HandleLocalGroupsChanged();
    void HandleRemoteSnapshot();
    void HandleRemoteResult(bool bSuccess, const FString& Message);

    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleRecalculateClicked();
    UFUNCTION() void HandleCreateGroupClicked();
    UFUNCTION() void HandleRenameGroupClicked();
    UFUNCTION() void HandleDeleteGroupClicked();
    UFUNCTION() void HandleAddStationClicked();
    UFUNCTION() void HandleApplySlotsClicked();
    UFUNCTION() void HandleRemoveStationClicked();
    UFUNCTION() void HandleGroupSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleGroupStationSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
