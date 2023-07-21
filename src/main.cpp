#include "main.hpp"
#include "utils.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData_NoteJumpValueType.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/LevelParamsPanel.hpp"
#include "GlobalNamespace/LevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/BeatmapDataTransformHelper.hpp"
#include "GlobalNamespace/EnvironmentEffectsFilterPreset.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/GameplayModifiersPanelController.hpp"

#include "UnityEngine/Vector3.hpp"

#include "questui/shared/QuestUI.hpp"
#include "custom-types/shared/register.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;

bool inPractice = false;
float practiceSpeed = 1;
float modifierSpeed = 1;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo, {false, true});
    return *logger;
}

ModInfo& getModInfo() {
    return modInfo;
}

// Hooks
MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Init, &BeatmapObjectSpawnMovementData::Init,
        void, BeatmapObjectSpawnMovementData* self, int noteLinesCount, float startNoteJumpMovementSpeed, float startBpm, BeatmapObjectSpawnMovementData::NoteJumpValueType noteJumpValueType, float noteJumpValue, IJumpOffsetYProvider* jumpOffsetYProvider, UnityEngine::Vector3 rightVec, UnityEngine::Vector3 forwardVec) {

    if(!getModConfig().Disable.GetValue()) {
        auto values = GetAppliedValues();
        if(values.OverrideNJS)
            startNoteJumpMovementSpeed = values.NJS;

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = values.MainValue;

        // convert distance to duration
        float actualNjs = startNoteJumpMovementSpeed;
        if(!inPractice)
            actualNjs = startNoteJumpMovementSpeed * modifierSpeed;
        if(!values.UseDuration)
            noteJumpValue /= actualNjs;

        if(!getModConfig().Half.GetValue())
            noteJumpValue *= 0.5;

        // if we do need to adjust for speed modifications, we have to actually increase the duration when the njs is faster, counterintuitively
        // this is because increasing the speed doesn't actually change the njs the game uses, but instead effectively speeds up time
        // so the effect is identical to faster njs, but we need to increase the duration instead of distance to make it result in the same amount of time
        if(values.UseDuration) {
            if(inPractice) // modifiers don't apply in practice mode
                noteJumpValue *= practiceSpeed;
            else
                noteJumpValue *= modifierSpeed;
        }
        if(getModConfig().Practice.GetValue()) {
            self->moveSpeed /= practiceSpeed;
            startNoteJumpMovementSpeed /= practiceSpeed;
            // compensate for distance since we actually changed the njs
            if(!values.UseDuration)
                noteJumpValue *= practiceSpeed;
        }

        getLogger().info("Changing jump duration to %.2f", noteJumpValue * 2);
    }

    BeatmapObjectSpawnMovementData_Init(self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpValueType, noteJumpValue, jumpOffsetYProvider, rightVec, forwardVec);
}

#include "GlobalNamespace/BeatmapLevelDataExtensions.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSegmentedControlController.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {

    getLogger().info("Loading level in menu");

    auto map = BeatmapLevelDataExtensions::GetDifficultyBeatmap(
        self->level->get_beatmapLevelData(),
        self->beatmapCharacteristicSegmentedControlController->selectedBeatmapCharacteristic,
        self->beatmapDifficultySegmentedControlController->selectedDifficulty
    );
    UpdateLevel(map, modifierSpeed);

    StandardLevelDetailView_RefreshContent(self);
}

MAKE_HOOK_MATCH(LevelParamsPanel_set_notesPerSecond, &LevelParamsPanel::set_notesPerSecond, void, LevelParamsPanel* self, float value) {

    LevelParamsPanel_set_notesPerSecond(self, value);

    UpdateNotesPerSecond(value);
}

MAKE_HOOK_MATCH(LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync, &LevelScenesTransitionSetupDataSO::BeforeScenesWillBeActivatedAsync, System::Threading::Tasks::Task*, LevelScenesTransitionSetupDataSO* self) {

    UpdateLevel(self->gameplayCoreSceneSetupData->difficultyBeatmap, modifierSpeed);

    auto practiceSettings = self->gameplayCoreSceneSetupData->practiceSettings;
    inPractice = practiceSettings != nullptr;
    practiceSpeed = inPractice ? practiceSettings->songSpeedMul : 1;

    return LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync(self);
}

MAKE_HOOK_MATCH(BeatmapDataTransformHelper_CreateTransformedBeatmapData, &BeatmapDataTransformHelper::CreateTransformedBeatmapData,
        IReadonlyBeatmapData*, IReadonlyBeatmapData* beatmapData, IPreviewBeatmapLevel* beatmapLevel, GameplayModifiers* gameplayModifiers, bool leftHanded, EnvironmentEffectsFilterPreset environmentEffectsFilterPreset, EnvironmentIntensityReductionOptions* environmentIntensityReductionOptions, MainSettingsModelSO* mainSettingsModel) {

    UpdateNotesPerSecond(GetNPS(beatmapLevel, beatmapData));

    return BeatmapDataTransformHelper_CreateTransformedBeatmapData(beatmapData, beatmapLevel, gameplayModifiers, leftHanded, environmentEffectsFilterPreset, environmentIntensityReductionOptions, mainSettingsModel);
}

MAKE_HOOK_MATCH(PlayerDataModel_Load, &PlayerDataModel::Load, void, PlayerDataModel* self) {

    PlayerDataModel_Load(self);

    modifierSpeed = self->playerData->gameplayModifiers->get_songSpeedMul();
}

MAKE_HOOK_MATCH(GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI, &GameplayModifiersPanelController::RefreshTotalMultiplierAndRankUI, void, GameplayModifiersPanelController* self) {

    GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI(self);

    modifierSpeed = self->gameplayModifiers->get_songSpeedMul();

    UpdateLevel(nullptr, modifierSpeed);
}

extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;

    getLogger().info("Completed setup!");
}

extern "C" void load() {
    getLogger().info("Installing mod...");
    il2cpp_functions::Init();

    getModConfig().Init(modInfo);
    UpdateScoreSubmission(getModConfig().UseNJS.GetValue());

    QuestUI::Init();
    QuestUI::Register::RegisterGameplaySetupMenu(modInfo, QuestUI::Register::MenuType::All, GameplaySettings);

    LoggerContextObject logger = getLogger().WithContext("load");

    // Install hooks
    INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_Init);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, LevelParamsPanel_set_notesPerSecond);
    INSTALL_HOOK(logger, LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync);
    INSTALL_HOOK(logger, BeatmapDataTransformHelper_CreateTransformedBeatmapData);
    INSTALL_HOOK(logger, PlayerDataModel_Load);
    INSTALL_HOOK(logger, GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI);
    getLogger().info("Installed all hooks!");
}
