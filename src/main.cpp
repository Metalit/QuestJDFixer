#include "main.hpp"

#include "GlobalNamespace/BeatmapDataTransformHelper.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnControllerHelpers.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/EnvironmentEffectsFilterPreset.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/GameplayModifiersPanelController.hpp"
#include "GlobalNamespace/LevelParamsPanel.hpp"
#include "GlobalNamespace/LevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/VariableMovementDataProvider.hpp"
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "utils.hpp"

using namespace GlobalNamespace;

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

static bool inPractice = false;
static float practiceSpeed = 1;
static float modifierSpeed = 1;

MAKE_HOOK_MATCH(
    VariableMovementDataProvider_Init,
    &VariableMovementDataProvider::Init,
    void,
    VariableMovementDataProvider* self,
    float startHalfJumpDurationInBeats,
    float maxHalfJumpDistance,
    float noteJumpMovementSpeed,
    float minRelativeNoteJumpSpeed,
    float bpm,
    BeatmapObjectSpawnMovementData::NoteJumpValueType noteJumpValueType,
    float noteJumpValue,
    UnityEngine::Vector3 centerPosition,
    UnityEngine::Vector3 forwardVector
) {
    if (!getModConfig().Disable.GetValue()) {
        auto values = JDFixer::GetAppliedValues();
        if (values.OverrideNJS)
            noteJumpMovementSpeed = values.NJS;

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = values.MainValue;

        // convert distance to duration
        float actualNjs = noteJumpMovementSpeed;
        if (!inPractice)
            actualNjs *= modifierSpeed;
        if (!values.UseDuration)
            noteJumpValue /= actualNjs;

        if (!getModConfig().Half.GetValue())
            noteJumpValue *= 0.5;

        // if we do need to adjust for speed modifications, we have to actually increase the duration when the njs is faster, counterintuitively
        // this is because increasing the speed doesn't actually change the njs the game uses, but instead effectively speeds up time
        // so the effect is identical to faster njs, but we need to increase the duration instead of distance to make it result in the same amount of
        // time
        if (values.UseDuration) {
            if (inPractice)  // modifiers don't apply in practice mode
                noteJumpValue *= practiceSpeed;
            else
                noteJumpValue *= modifierSpeed;
        }
        if (getModConfig().Practice.GetValue()) {
            noteJumpMovementSpeed /= practiceSpeed;
            // compensate for distance since we actually changed the njs
            if (!values.UseDuration)
                noteJumpValue *= practiceSpeed;
        }

        logger.info("Changing jump duration to {:.2f}", noteJumpValue * 2);
    }

    VariableMovementDataProvider_Init(
        self,
        startHalfJumpDurationInBeats,
        maxHalfJumpDistance,
        noteJumpMovementSpeed,
        minRelativeNoteJumpSpeed,
        bpm,
        noteJumpValueType,
        noteJumpValue,
        centerPosition,
        forwardVector
    );
}

MAKE_HOOK_MATCH(
    VariableMovementDataProvider_HandleNoteJumpMovementSpeedEvent,
    &VariableMovementDataProvider::HandleNoteJumpMovementSpeedEvent,
    void,
    VariableMovementDataProvider* self,
    NoteJumpSpeedEventData* currentEventData
) {
    if (getModConfig().Disable.GetValue() || !JDFixer::GetAppliedValues().OverrideNJS)
        VariableMovementDataProvider_HandleNoteJumpMovementSpeedEvent(self, currentEventData);
}

MAKE_HOOK_MATCH(
    BeatmapObjectSpawnControllerHelpers_GetNoteJumpValues,
    &BeatmapObjectSpawnControllerHelpers::GetNoteJumpValues,
    void,
    PlayerSpecificSettings* playerSpecificSettings,
    float defaultNoteJumpStartBeatOffset,
    ByRef<BeatmapObjectSpawnMovementData::NoteJumpValueType> noteJumpValueType,
    ByRef<float> noteJumpValue
) {
    BeatmapObjectSpawnControllerHelpers_GetNoteJumpValues(playerSpecificSettings, defaultNoteJumpStartBeatOffset, noteJumpValueType, noteJumpValue);

    if (!getModConfig().Disable.GetValue())
        *noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {

    StandardLevelDetailView_RefreshContent(self);

    logger.info("Loading level in menu");
    JDFixer::UpdateLevel(self->beatmapKey, modifierSpeed);
}

MAKE_HOOK_MATCH(LevelParamsPanel_set_notesPerSecond, &LevelParamsPanel::set_notesPerSecond, void, LevelParamsPanel* self, float value) {

    LevelParamsPanel_set_notesPerSecond(self, value);

    JDFixer::UpdateNotesPerSecond(value);
}

MAKE_HOOK_MATCH(
    LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync,
    &LevelScenesTransitionSetupDataSO::BeforeScenesWillBeActivatedAsync,
    System::Threading::Tasks::Task*,
    LevelScenesTransitionSetupDataSO* self
) {
    JDFixer::UpdateLevel(self->gameplayCoreSceneSetupData->beatmapKey, modifierSpeed);

    auto practiceSettings = self->gameplayCoreSceneSetupData->practiceSettings;
    inPractice = practiceSettings != nullptr;
    practiceSpeed = inPractice ? practiceSettings->songSpeedMul : 1;

    return LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync(self);
}

MAKE_HOOK_MATCH(
    BeatmapDataTransformHelper_CreateTransformedBeatmapData,
    &BeatmapDataTransformHelper::CreateTransformedBeatmapData,
    IReadonlyBeatmapData*,
    IReadonlyBeatmapData* beatmapData,
    BeatmapLevel* beatmapLevel,
    GameplayModifiers* gameplayModifiers,
    bool leftHanded,
    EnvironmentEffectsFilterPreset environmentEffectsFilterPreset,
    EnvironmentIntensityReductionOptions* environmentIntensityReductionOptions,
    ByRef<::BeatSaber::Settings::Settings> settings
) {
    JDFixer::UpdateNotesPerSecond(JDFixer::GetNPS(beatmapLevel, beatmapData));

    return BeatmapDataTransformHelper_CreateTransformedBeatmapData(
        beatmapData, beatmapLevel, gameplayModifiers, leftHanded, environmentEffectsFilterPreset, environmentIntensityReductionOptions, settings
    );
}

MAKE_HOOK_MATCH(
    PlayerDataModel_Inject,
    &PlayerDataModel::Inject,
    void,
    PlayerDataModel* self,
    StringW playerDataJsonString,
    PlayerDataFileModel* playerDataFileModel
) {
    PlayerDataModel_Inject(self, playerDataJsonString, playerDataFileModel);

    modifierSpeed = JDFixer::GetNJSModifier(self->playerData->gameplayModifiers->songSpeedMul);
}

MAKE_HOOK_MATCH(
    GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI,
    &GameplayModifiersPanelController::RefreshTotalMultiplierAndRankUI,
    void,
    GameplayModifiersPanelController* self
) {
    GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI(self);

    modifierSpeed = JDFixer::GetNJSModifier(self->gameplayModifiers->songSpeedMul);

    JDFixer::UpdateLevel({}, modifierSpeed);
}

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getModConfig().Init(modInfo);

    logger.info("Completed setup!");
}

extern "C" void load() {
    logger.info("Installing mod...");
    il2cpp_functions::Init();

    JDFixer::UpdateScoreSubmission(getModConfig().UseNJS.GetValue());

    BSML::Register::RegisterGameplaySetupTab(MOD_ID, JDFixer::GameplaySettings, BSML::MenuType::All);

    // Install hooks
    INSTALL_HOOK(logger, VariableMovementDataProvider_Init);
    INSTALL_HOOK(logger, VariableMovementDataProvider_HandleNoteJumpMovementSpeedEvent);
    INSTALL_HOOK(logger, BeatmapObjectSpawnControllerHelpers_GetNoteJumpValues);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, LevelParamsPanel_set_notesPerSecond);
    INSTALL_HOOK(logger, LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync);
    INSTALL_HOOK(logger, BeatmapDataTransformHelper_CreateTransformedBeatmapData);
    INSTALL_HOOK(logger, PlayerDataModel_Inject);
    INSTALL_HOOK(logger, GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI);
    logger.info("Installed all hooks!");
}
