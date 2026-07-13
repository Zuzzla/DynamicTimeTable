#include "DynamicTimeTableWidget.h"

#include "DynamicTimeTableSubsystem.h"
#include "DynamicTimeTableRemoteCallObject.h"
#include "FGPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogDynamicTimeTableWidget, Log, All);

namespace
{
    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        Text->SetColorAndOpacity(FSlateColor(Color));
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Size;
        Text->SetFont(Font);
        return Text;
    }

    UButton* MakeButton(UWidgetTree* Tree, const FString& Label)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        Button->SetContent(MakeText(Tree, Label, 15, FLinearColor::White));
        return Button;
    }

    UHorizontalBoxSlot* AddRowItem(UHorizontalBox* Row, UWidget* Widget, float Padding = 5.0f, bool bFill = false)
    {
        UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget);
        Slot->SetPadding(FMargin(Padding));
        Slot->SetVerticalAlignment(VAlign_Center);
        if (bFill) Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        return Slot;
    }
}

UDynamicTimeTableWidget::UDynamicTimeTableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    mUseKeyboard = true;
    mUseMouse = true;
    mMouseLockMode = EMouseLockMode::LockOnCapture;
    mHideCursorDuringCapture = false;
    mDisablePlayerActions = true;
    mDisableBuildGunActions = true;
    mDisablePlayerEquipmentManagement = true;
    mDisableVisualizationModeActions = true;
    mFlushMouseKeysOnOpen = true;
    mSupportsStacking = false;
    mSupportsCaching = false;
    mRestoreFocusWhenLost = true;
}

void UDynamicTimeTableWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree && !WidgetTree->RootWidget) BuildWidgetTree();
}

void UDynamicTimeTableWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bInitialFocusApplied = false;
    LocalLiveRefreshAccumulator = 0.0f;

    CloseButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleCloseClicked);
    RecalculateButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleRecalculateClicked);
    CreateGroupButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleCreateGroupClicked);
    RenameGroupButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleRenameGroupClicked);
    DeleteGroupButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleDeleteGroupClicked);
    AddStationButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleAddStationClicked);
    ApplySlotsButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleApplySlotsClicked);
    RemoveStationButton->OnClicked.AddDynamic(this, &UDynamicTimeTableWidget::HandleRemoveStationClicked);
    GroupSelector->OnSelectionChanged.AddDynamic(this, &UDynamicTimeTableWidget::HandleGroupSelectionChanged);
    GroupStationSelector->OnSelectionChanged.AddDynamic(this, &UDynamicTimeTableWidget::HandleGroupStationSelectionChanged);
    SetDefaultFocusWidget(CloseButton);

    if (UDynamicTimeTableSubsystem* Subsystem = GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())
        GroupsChangedHandle = Subsystem->OnGroupsChangedNative.AddUObject(this, &UDynamicTimeTableWidget::HandleLocalGroupsChanged);
    if (AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer()))
    {
        RemoteCallObject = Cast<UDynamicTimeTableRemoteCallObject>(PC->GetRemoteCallObjectOfClass(UDynamicTimeTableRemoteCallObject::StaticClass()));
        if (!RemoteCallObject) RemoteCallObject = Cast<UDynamicTimeTableRemoteCallObject>(PC->RegisterRemoteCallObjectClass(UDynamicTimeTableRemoteCallObject::StaticClass()));
        if (RemoteCallObject)
        {
            SnapshotChangedHandle = RemoteCallObject->OnSnapshotChanged.AddUObject(this, &UDynamicTimeTableWidget::HandleRemoteSnapshot);
            OperationResultHandle = RemoteCallObject->OnOperationResult.AddUObject(this, &UDynamicTimeTableWidget::HandleRemoteResult);
            if (IsRemoteClient()) RemoteCallObject->RequestSnapshot();
        }
    }
    RefreshData();
    if (!IsRemoteClient())
    {
        if (UDynamicTimeTableSubsystem* Subsystem = GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())
            LastAppliedConfigurationRevision = Subsystem->GetConfigurationRevision();
    }
    UE_LOG(LogDynamicTimeTableWidget, Verbose, TEXT("Virtual /dtt window opened with group editor."));
}

void UDynamicTimeTableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Remote clients are refreshed by the RCO snapshot timer. Standalone and
    // listen-host widgets poll only their existing runtime text/color widgets.
    // This keeps live values current without rebuilding the widget tree or
    // closing combo-box popups.
    if (!IsRemoteClient())
    {
        LocalLiveRefreshAccumulator += InDeltaTime;
        if (LocalLiveRefreshAccumulator >= 1.5f)
        {
            LocalLiveRefreshAccumulator = 0.0f;
            RefreshRuntimeDisplay();
        }
    }

    if (AFGPlayerController* Player = Cast<AFGPlayerController>(GetOwningPlayer()))
    {
        Player->SetShowMouseCursor(true);
        Player->bEnableClickEvents = true;
        Player->bEnableMouseOverEvents = true;
        Player->CurrentMouseCursor = EMouseCursor::Default;
        if (!bInitialFocusApplied)
        {
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Player->SetInputMode(InputMode);
            SetUserFocus(Player);
            SetKeyboardFocus();
            Player->SetIgnoreMoveInput(true);
            Player->SetIgnoreLookInput(true);
            bInitialFocusApplied = true;
            UE_LOG(LogDynamicTimeTableWidget, Verbose, TEXT("Virtual /dtt window received initial UI focus."));
        }
    }
}

void UDynamicTimeTableWidget::NativeDestruct()
{
    if (UDynamicTimeTableSubsystem* Subsystem = GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())
        if (GroupsChangedHandle.IsValid()) Subsystem->OnGroupsChangedNative.Remove(GroupsChangedHandle);
    GroupsChangedHandle.Reset();
    if (RemoteCallObject)
    {
        if (SnapshotChangedHandle.IsValid()) RemoteCallObject->OnSnapshotChanged.Remove(SnapshotChangedHandle);
        if (OperationResultHandle.IsValid()) RemoteCallObject->OnOperationResult.Remove(OperationResultHandle);
    }
    SnapshotChangedHandle.Reset(); OperationResultHandle.Reset(); RemoteCallObject = nullptr;

    if (AFGPlayerController* Player = Cast<AFGPlayerController>(GetOwningPlayer()))
    {
        FInputModeGameOnly InputMode;
        Player->SetInputMode(InputMode);
        Player->SetShowMouseCursor(false);
        Player->SetIgnoreMoveInput(false);
        Player->SetIgnoreLookInput(false);
    }
    UE_LOG(LogDynamicTimeTableWidget, Verbose, TEXT("Virtual /dtt window closed."));
    Super::NativeDestruct();
}

void UDynamicTimeTableWidget::OnEscapePressed_Implementation() { CloseWindow(); }

void UDynamicTimeTableWidget::BuildWidgetTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;

    UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>();
    Dimmer->SetBrushColor(FLinearColor(0,0,0,0.72f));
    UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(Dimmer);
    DimmerSlot->SetAnchors(FAnchors(0,0,1,1)); DimmerSlot->SetOffsets(FMargin(0));

    USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
    WindowSize->SetWidthOverride(1120); WindowSize->SetHeightOverride(820);
    UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(WindowSize);
    WindowSlot->SetAnchors(FAnchors(0.5f)); WindowSlot->SetAlignment(FVector2D(0.5f));
    WindowSlot->SetSize(FVector2D(1120,820));

    UBorder* WindowBorder = WidgetTree->ConstructWidget<UBorder>();
    WindowBorder->SetBrushColor(FLinearColor(0.035f,0.055f,0.07f,0.98f));
    WindowBorder->SetPadding(FMargin(22)); WindowSize->SetContent(WindowBorder);
    UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(); WindowBorder->SetContent(Main);

    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Main->AddChildToVerticalBox(Header)->SetPadding(FMargin(0,0,0,8));
    AddRowItem(Header, MakeText(WidgetTree,TEXT("DynamicTimeTable"),30,FLinearColor(0.2f,0.8f,1)),5,true);
    CloseButton = MakeButton(WidgetTree,TEXT("Close")); AddRowItem(Header,CloseButton);

    SummaryText = MakeText(WidgetTree,TEXT("Loading..."),16,FLinearColor(0.78f,0.85f,0.9f));
    Main->AddChildToVerticalBox(SummaryText)->SetPadding(FMargin(5));

    BuildEditorPanel(Main);

    GroupScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
    UVerticalBoxSlot* ScrollSlot = Main->AddChildToVerticalBox(GroupScrollBox);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ScrollSlot->SetPadding(FMargin(5,8));

    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
    Main->AddChildToVerticalBox(Footer)->SetHorizontalAlignment(HAlign_Right);
    RecalculateButton = MakeButton(WidgetTree,TEXT("Recalculate bookings")); AddRowItem(Footer,RecalculateButton);
    UButton* FooterClose = MakeButton(WidgetTree,TEXT("Close"));
    FooterClose->OnClicked.AddDynamic(this,&UDynamicTimeTableWidget::HandleCloseClicked); AddRowItem(Footer,FooterClose);
}

void UDynamicTimeTableWidget::BuildEditorPanel(UVerticalBox* Main)
{
    UBorder* EditorBorder = WidgetTree->ConstructWidget<UBorder>();
    EditorBorder->SetBrushColor(FLinearColor(0.08f,0.12f,0.15f,1)); EditorBorder->SetPadding(FMargin(10));
    Main->AddChildToVerticalBox(EditorBorder)->SetPadding(FMargin(5,5,5,8));
    UVerticalBox* Editor = WidgetTree->ConstructWidget<UVerticalBox>(); EditorBorder->SetContent(Editor);
    Editor->AddChildToVerticalBox(MakeText(WidgetTree,TEXT("Group configuration"),19,FLinearColor(0.2f,0.8f,1)));

    UHorizontalBox* CreateRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Editor->AddChildToVerticalBox(CreateRow);
    NewGroupNameInput = WidgetTree->ConstructWidget<UEditableTextBox>(); NewGroupNameInput->SetHintText(FText::FromString(TEXT("New group name")));
    AddRowItem(CreateRow,NewGroupNameInput,5,true); CreateGroupButton=MakeButton(WidgetTree,TEXT("Create group")); AddRowItem(CreateRow,CreateGroupButton);

    UHorizontalBox* GroupRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Editor->AddChildToVerticalBox(GroupRow);
    GroupSelector=WidgetTree->ConstructWidget<UComboBoxString>(); AddRowItem(GroupRow,GroupSelector,5,true);
    GroupNameInput=WidgetTree->ConstructWidget<UEditableTextBox>(); GroupNameInput->SetHintText(FText::FromString(TEXT("Group name")));
    AddRowItem(GroupRow,GroupNameInput,5,true); RenameGroupButton=MakeButton(WidgetTree,TEXT("Rename")); AddRowItem(GroupRow,RenameGroupButton);
    DeleteGroupButton=MakeButton(WidgetTree,TEXT("Delete")); AddRowItem(GroupRow,DeleteGroupButton);

    UHorizontalBox* AddStationRow=WidgetTree->ConstructWidget<UHorizontalBox>(); Editor->AddChildToVerticalBox(AddStationRow);
    AvailableStationSelector=WidgetTree->ConstructWidget<UComboBoxString>(); AddRowItem(AddStationRow,AvailableStationSelector,5,true);
    AddStationButton=MakeButton(WidgetTree,TEXT("Add station")); AddRowItem(AddStationRow,AddStationButton);

    UHorizontalBox* StationRow=WidgetTree->ConstructWidget<UHorizontalBox>(); Editor->AddChildToVerticalBox(StationRow);
    GroupStationSelector=WidgetTree->ConstructWidget<UComboBoxString>(); AddRowItem(StationRow,GroupStationSelector,5,true);
    TargetSlotsInput=WidgetTree->ConstructWidget<USpinBox>(); TargetSlotsInput->SetMinValue(1); TargetSlotsInput->SetMaxValue(999); TargetSlotsInput->SetMinSliderValue(1); TargetSlotsInput->SetMaxSliderValue(999); TargetSlotsInput->SetDelta(1); TargetSlotsInput->SetAlwaysUsesDeltaSnap(true); TargetSlotsInput->SetValue(1);
    AddRowItem(StationRow,TargetSlotsInput); ApplySlotsButton=MakeButton(WidgetTree,TEXT("Apply target slots")); AddRowItem(StationRow,ApplySlotsButton);
    RemoveStationButton=MakeButton(WidgetTree,TEXT("Remove station")); AddRowItem(StationRow,RemoveStationButton);

    EditorStatusText=MakeText(WidgetTree,TEXT("Ready."),14,FLinearColor(0.7f,0.8f,0.85f)); Editor->AddChildToVerticalBox(EditorStatusText)->SetPadding(FMargin(5));
}

bool UDynamicTimeTableWidget::IsRemoteClient() const{return GetWorld()&&GetWorld()->GetNetMode()==NM_Client;}
const TArray<FDTTStationGroup>& UDynamicTimeTableWidget::ViewGroups()const{if(IsRemoteClient()&&RemoteCallObject)return RemoteCallObject->GetSnapshot().Groups;static TArray<FDTTStationGroup> Empty;if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())return S->GetStationGroups();return Empty;}
void UDynamicTimeTableWidget::HandleLocalGroupsChanged()
{
    UDynamicTimeTableSubsystem* Subsystem = GetWorld()
        ? GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>()
        : nullptr;
    if (!Subsystem) return;

    const int32 Revision = Subsystem->GetConfigurationRevision();
    if (Revision != LastAppliedConfigurationRevision)
    {
        RefreshData();
        LastAppliedConfigurationRevision = Revision;
        return;
    }

    RefreshRuntimeDisplay();
}
void UDynamicTimeTableWidget::HandleRemoteSnapshot()
{
    if (!RemoteCallObject) return;

    const int32 SnapshotRevision = RemoteCallObject->GetSnapshot().Revision;
    const bool bConfigurationChanged =
        !bHasAppliedRemoteConfiguration ||
        bForceFullRefreshOnNextSnapshot ||
        SnapshotRevision != LastAppliedConfigurationRevision;

    if (bConfigurationChanged)
    {
        RefreshData();
        LastAppliedConfigurationRevision = SnapshotRevision;
        bHasAppliedRemoteConfiguration = true;
        bForceFullRefreshOnNextSnapshot = false;
        return;
    }

    RefreshRuntimeDisplay();
}
void UDynamicTimeTableWidget::HandleRemoteResult(bool O,const FString&M)
{
    if (O) bForceFullRefreshOnNextSnapshot = true;
    SetEditorStatus(M,!O);
}
void UDynamicTimeTableWidget::RefreshData()
{
    RebuildRuntimeDisplay();
    RefreshEditorControls();
}
void UDynamicTimeTableWidget::RefreshRuntimeDisplay()
{
    if (!GroupScrollBox) return;

    if (!RuntimeLayoutMatches())
    {
        RebuildRuntimeDisplay();
        return;
    }

    UpdateRuntimeDisplayInPlace();
}
bool UDynamicTimeTableWidget::RuntimeLayoutMatches() const
{
    const TArray<FDTTStationGroup>& Groups = ViewGroups();
    if (RuntimeGroupIds.Num() != Groups.Num() ||
        RuntimeGroupTexts.Num() != Groups.Num() ||
        RuntimeGroupBorders.Num() != Groups.Num() ||
        RuntimeStationCounts.Num() != Groups.Num())
        return false;

    int32 FlatStationIndex = 0;
    for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
    {
        const FDTTStationGroup& Group = Groups[GroupIndex];
        if (RuntimeGroupIds[GroupIndex] != Group.GroupId ||
            RuntimeStationCounts[GroupIndex] != Group.Stations.Num())
            return false;

        for (const FDTTStationEntry& Entry : Group.Stations)
        {
            if (!RuntimeStationNames.IsValidIndex(FlatStationIndex) ||
                RuntimeStationNames[FlatStationIndex] != Entry.DisplayName)
                return false;
            ++FlatStationIndex;
        }
    }

    return FlatStationIndex == RuntimeStationNames.Num() &&
        FlatStationIndex == RuntimeStationTexts.Num();
}
void UDynamicTimeTableWidget::RebuildRuntimeDisplay()
{
    if (!GroupScrollBox) return;

    GroupScrollBox->ClearChildren();
    RuntimeGroupBorders.Reset();
    RuntimeGroupTexts.Reset();
    RuntimeStationTexts.Reset();
    RuntimeGroupIds.Reset();
    RuntimeStationCounts.Reset();
    RuntimeStationNames.Reset();

    const TArray<FDTTStationGroup>& Groups = ViewGroups();
    SummaryText->SetText(FText::FromString(FString::Printf(
        TEXT("%d group(s) - %s"),
        Groups.Num(),
        IsRemoteClient() ? TEXT("server snapshot") : TEXT("live booking status"))));

    for (const FDTTStationGroup& Group : Groups)
    {
        RuntimeGroupIds.Add(Group.GroupId);
        RuntimeStationCounts.Add(Group.Stations.Num());

        UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
        Border->SetPadding(FMargin(14));
        GroupScrollBox->AddChild(Border);
        RuntimeGroupBorders.Add(Border);

        UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
        Border->SetContent(Panel);

        UTextBlock* GroupText = MakeText(WidgetTree, Group.GroupName, 21, FLinearColor(0.2f,0.8f,1));
        Panel->AddChildToVerticalBox(GroupText);
        RuntimeGroupTexts.Add(GroupText);

        for (const FDTTStationEntry& Entry : Group.Stations)
        {
            UTextBlock* StationText = MakeText(WidgetTree, Entry.DisplayName, 16, FLinearColor::White);
            Panel->AddChildToVerticalBox(StationText);
            RuntimeStationTexts.Add(StationText);
            RuntimeStationNames.Add(Entry.DisplayName);
        }
    }

    UpdateRuntimeDisplayInPlace();
}
void UDynamicTimeTableWidget::UpdateRuntimeDisplayInPlace()
{
    const TArray<FDTTStationGroup>& Groups = ViewGroups();
    SummaryText->SetText(FText::FromString(FString::Printf(
        TEXT("%d group(s) - %s"),
        Groups.Num(),
        IsRemoteClient() ? TEXT("server snapshot") : TEXT("live booking status"))));

    int32 FlatStationIndex = 0;
    for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
    {
        const FDTTStationGroup& Group = Groups[GroupIndex];
        TArray<FDTTStationRuntimeLoad> Loads;

        if (IsRemoteClient() && RemoteCallObject)
        {
            for (const FDTTStationRuntimeLoad& Load : RemoteCallObject->GetSnapshot().RuntimeLoads)
            {
                if (Group.Stations.ContainsByPredicate([&](const FDTTStationEntry& Entry)
                    { return Entry.DisplayName == Load.StationName; }))
                    Loads.Add(Load);
            }
        }
        else if (UDynamicTimeTableSubsystem* Subsystem = GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())
        {
            Subsystem->GetGroupRuntimeLoad(Group.GroupId, Loads);
        }

        int32 Assigned = 0;
        int32 Target = 0;
        for (const FDTTStationRuntimeLoad& Load : Loads)
        {
            Assigned += Load.AssignedTrains;
            Target += Load.TargetSlots;
        }

        const bool bOverloaded = Assigned > Target;
        RuntimeGroupBorders[GroupIndex]->SetBrushColor(
            bOverloaded
                ? FLinearColor(0.25f,0.035f,0.025f,0.95f)
                : FLinearColor(0.06f,0.10f,0.13f,0.95f));
        RuntimeGroupTexts[GroupIndex]->SetText(FText::FromString(FString::Printf(
            TEXT("%s    %d/%d%s"),
            *Group.GroupName,
            Assigned,
            Target,
            bOverloaded ? TEXT("  OVERLOAD") : TEXT(""))));
        RuntimeGroupTexts[GroupIndex]->SetColorAndOpacity(FSlateColor(
            bOverloaded ? FLinearColor::Red : FLinearColor(0.2f,0.8f,1)));

        for (const FDTTStationEntry& Entry : Group.Stations)
        {
            const FDTTStationRuntimeLoad* Load = Loads.FindByPredicate(
                [&](const FDTTStationRuntimeLoad& Candidate)
                { return Candidate.StationName == Entry.DisplayName; });

            const int32 StationAssigned = Load ? Load->AssignedTrains : 0;
            const int32 StationTarget = Load ? Load->TargetSlots : FMath::Max(1, Entry.TargetSlots);
            const bool bStationOverloaded = Load ? Load->bOverloaded : StationAssigned > StationTarget;

            UTextBlock* StationText = RuntimeStationTexts[FlatStationIndex++];
            StationText->SetText(FText::FromString(FString::Printf(
                TEXT("    %s     booked %d / target %d%s"),
                *Entry.DisplayName,
                StationAssigned,
                StationTarget,
                bStationOverloaded ? TEXT("  OVERLOAD") : TEXT(""))));
            StationText->SetColorAndOpacity(FSlateColor(
                bStationOverloaded ? FLinearColor::Red : FLinearColor::White));
        }
    }
}
void UDynamicTimeTableWidget::RefreshEditorControls(){if(bRefreshingControls)return;bRefreshingControls=true;const auto&Groups=ViewGroups();if(!SelectedGroupId.IsValid()||!Groups.ContainsByPredicate([&](const FDTTStationGroup&G){return G.GroupId==SelectedGroupId;}))SelectedGroupId=Groups.Num()?Groups[0].GroupId:FGuid();GroupSelector->ClearOptions();GroupOptionIds.Reset();const FDTTStationGroup*Selected=nullptr;for(const auto&G:Groups){GroupSelector->AddOption(G.GroupName);GroupOptionIds.Add(G.GroupId);if(G.GroupId==SelectedGroupId)Selected=&G;}if(Selected){GroupSelector->SetSelectedOption(Selected->GroupName);GroupNameInput->SetText(FText::FromString(Selected->GroupName));}else GroupNameInput->SetText(FText::GetEmpty());GroupStationSelector->ClearOptions();if(Selected)for(const auto&E:Selected->Stations)GroupStationSelector->AddOption(E.DisplayName);if(GroupStationSelector->GetOptionCount()>0)GroupStationSelector->SetSelectedIndex(0);TArray<FString>All;if(IsRemoteClient()&&RemoteCallObject)All=RemoteCallObject->GetSnapshot().AvailableStations;else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->GetAllVanillaStationNames(All);AvailableStationSelector->ClearOptions();for(const FString&N:All)if(!Groups.ContainsByPredicate([&](const FDTTStationGroup&G){return G.Stations.ContainsByPredicate([&](const FDTTStationEntry&E){return E.DisplayName==N;});}))AvailableStationSelector->AddOption(N);if(AvailableStationSelector->GetOptionCount()>0)AvailableStationSelector->SetSelectedIndex(0);bRefreshingControls=false;HandleGroupStationSelectionChanged(GroupStationSelector->GetSelectedOption(),ESelectInfo::Direct);}
void UDynamicTimeTableWidget::SetEditorStatus(const FString&M,bool E){EditorStatusText->SetText(FText::FromString(M));EditorStatusText->SetColorAndOpacity(FSlateColor(E?FLinearColor::Red:FLinearColor(0.3f,1,0.5f)));}FString UDynamicTimeTableWidget::GetSelectedGroupStationName()const{return GroupStationSelector?GroupStationSelector->GetSelectedOption():FString();}
void UDynamicTimeTableWidget::HandleGroupSelectionChanged(FString Item,ESelectInfo::Type){if(bRefreshingControls)return;int32 I=GroupSelector->FindOptionIndex(Item);if(GroupOptionIds.IsValidIndex(I)){SelectedGroupId=GroupOptionIds[I];RefreshEditorControls();}}
void UDynamicTimeTableWidget::HandleGroupStationSelectionChanged(FString Item,ESelectInfo::Type){if(Item.IsEmpty())return;if(const auto*G=ViewGroups().FindByPredicate([&](const FDTTStationGroup&X){return X.GroupId==SelectedGroupId;}))if(const auto*E=G->Stations.FindByPredicate([&](const FDTTStationEntry&X){return X.DisplayName==Item;}))TargetSlotsInput->SetValue(E->TargetSlots);}
void UDynamicTimeTableWidget::HandleCreateGroupClicked(){FString N=NewGroupNameInput->GetText().ToString().TrimStartAndEnd();if(N.IsEmpty()){SetEditorStatus(TEXT("Enter a group name."),true);return;}if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestCreateGroup(N);else SetEditorStatus(TEXT("Server connection unavailable."),true);}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>()){SelectedGroupId=S->CreateGroup(N);RefreshData();}NewGroupNameInput->SetText(FText::GetEmpty());}
void UDynamicTimeTableWidget::HandleRenameGroupClicked(){FString N=GroupNameInput->GetText().ToString().TrimStartAndEnd();if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestRenameGroup(SelectedGroupId,N);}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())SetEditorStatus(S->RenameGroup(SelectedGroupId,N)?TEXT("Group renamed."):TEXT("Rename failed."));}
void UDynamicTimeTableWidget::HandleDeleteGroupClicked(){if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestDeleteGroup(SelectedGroupId);}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>()){S->RemoveGroup(SelectedGroupId);SelectedGroupId=FGuid();RefreshData();}}
void UDynamicTimeTableWidget::HandleAddStationClicked(){FString N=AvailableStationSelector->GetSelectedOption();if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestAddStation(SelectedGroupId,N);}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->AddStationByName(SelectedGroupId,N,1);}
void UDynamicTimeTableWidget::HandleApplySlotsClicked(){int32 Slots=FMath::Clamp(FMath::RoundToInt(TargetSlotsInput->GetValue()),1,999);if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestSetTargetSlots(SelectedGroupId,GetSelectedGroupStationName(),Slots);}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->SetTargetSlots(SelectedGroupId,GetSelectedGroupStationName(),Slots);}
void UDynamicTimeTableWidget::HandleRemoveStationClicked(){if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestRemoveStation(SelectedGroupId,GetSelectedGroupStationName());}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->RemoveStationByName(SelectedGroupId,GetSelectedGroupStationName());}
void UDynamicTimeTableWidget::HandleRecalculateClicked(){if(IsRemoteClient()){if(RemoteCallObject)RemoteCallObject->RequestRecalculate();}else if(auto*S=GetWorld()->GetSubsystem<UDynamicTimeTableSubsystem>())S->RecalculateBookingsFromVanilla();}
void UDynamicTimeTableWidget::HandleCloseClicked(){CloseWindow();}void UDynamicTimeTableWidget::CloseWindow(){AFGPlayerController*P=Cast<AFGPlayerController>(GetOwningPlayer());RemoveFromParent();if(IsValid(P)){FInputModeGameOnly I;P->SetInputMode(I);P->SetShowMouseCursor(false);P->SetIgnoreMoveInput(false);P->SetIgnoreLookInput(false);}}
