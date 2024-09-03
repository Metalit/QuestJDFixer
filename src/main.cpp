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
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "utils.hpp"

using namespace GlobalNamespace;

modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

bool inPractice = false;
float practiceSpeed = 1;
float modifierSpeed = 1;

MAKE_HOOK_MATCH(
    BeatmapObjectSpawnMovementData_Init,
    &BeatmapObjectSpawnMovementData::Init,
    void,
    BeatmapObjectSpawnMovementData* self,
    int noteLinesCount,
    float startNoteJumpMovementSpeed,
    float startBpm,
    BeatmapObjectSpawnMovementData::NoteJumpValueType noteJumpValueType,
    float noteJumpValue,
    IJumpOffsetYProvider* jumpOffsetYProvider,
    UnityEngine::Vector3 rightVec,
    UnityEngine::Vector3 forwardVec
) {
    if (!getModConfig().Disable.GetValue()) {
        auto values = GetAppliedValues();
        if (values.OverrideNJS)
            startNoteJumpMovementSpeed = values.NJS;

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = values.MainValue;

        // convert distance to duration
        float actualNjs = startNoteJumpMovementSpeed;
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
            self->_moveSpeed /= practiceSpeed;
            startNoteJumpMovementSpeed /= practiceSpeed;
            // compensate for distance since we actually changed the njs
            if (!values.UseDuration)
                noteJumpValue *= practiceSpeed;
        }

        logger.info("Changing jump duration to {:.2f}", noteJumpValue * 2);
    }

    BeatmapObjectSpawnMovementData_Init(
        self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpValueType, noteJumpValue, jumpOffsetYProvider, rightVec, forwardVec
    );
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
    UpdateLevel({self->beatmapKey, self->_beatmapLevel}, modifierSpeed);
}

MAKE_HOOK_MATCH(LevelParamsPanel_set_notesPerSecond, &LevelParamsPanel::set_notesPerSecond, void, LevelParamsPanel* self, float value) {

    LevelParamsPanel_set_notesPerSecond(self, value);

    UpdateNotesPerSecond(value);
}

MAKE_HOOK_MATCH(
    LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync,
    &LevelScenesTransitionSetupDataSO::BeforeScenesWillBeActivatedAsync,
    System::Threading::Tasks::Task*,
    LevelScenesTransitionSetupDataSO* self
) {
    UpdateLevel({self->gameplayCoreSceneSetupData->beatmapKey, self->gameplayCoreSceneSetupData->beatmapLevel}, modifierSpeed);

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
    BeatSaber::PerformancePresets::PerformancePreset* performancePreset
) {
    UpdateNotesPerSecond(GetNPS(beatmapLevel, beatmapData));

    return BeatmapDataTransformHelper_CreateTransformedBeatmapData(
        beatmapData,
        beatmapLevel,
        gameplayModifiers,
        leftHanded,
        environmentEffectsFilterPreset,
        environmentIntensityReductionOptions,
        performancePreset
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

    modifierSpeed = self->playerData->gameplayModifiers->get_songSpeedMul();
}

MAKE_HOOK_MATCH(
    GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI,
    &GameplayModifiersPanelController::RefreshTotalMultiplierAndRankUI,
    void,
    GameplayModifiersPanelController* self
) {
    GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI(self);

    modifierSpeed = self->gameplayModifiers->get_songSpeedMul();

    UpdateLevel({}, modifierSpeed);
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

    UpdateScoreSubmission(getModConfig().UseNJS.GetValue());

    BSML::Register::RegisterGameplaySetupTab(MOD_ID, GameplaySettings, BSML::MenuType::All);

    // Install hooks
    INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_Init);
    INSTALL_HOOK(logger, BeatmapObjectSpawnControllerHelpers_GetNoteJumpValues);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, LevelParamsPanel_set_notesPerSecond);
    INSTALL_HOOK(logger, LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync);
    INSTALL_HOOK(logger, BeatmapDataTransformHelper_CreateTransformedBeatmapData);
    INSTALL_HOOK(logger, PlayerDataModel_Inject);
    INSTALL_HOOK(logger, GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI);
    logger.info("Installed all hooks!");
}
