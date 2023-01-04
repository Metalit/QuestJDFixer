#include "main.hpp"
#include "utils.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData_NoteJumpValueType.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/LevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/GameplayModifiersPanelController.hpp"

#include "UnityEngine/Vector3.hpp"

#include "questui/shared/QuestUI.hpp"
#include "custom-types/shared/register.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;

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

        self->moveSpeed /= practiceSpeed;
        startNoteJumpMovementSpeed /= practiceSpeed;

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = values.MainValue;
        if(!values.UseDuration)
            noteJumpValue /= startNoteJumpMovementSpeed;

        getLogger().info("Changing jump duration to %.2f", noteJumpValue * 2);
    }

    BeatmapObjectSpawnMovementData_Init(self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpValueType, noteJumpValue, jumpOffsetYProvider, rightVec, forwardVec);
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);

    getLogger().info("Loading level in menu");

    UpdateLevel(self->selectedDifficultyBeatmap, modifierSpeed);
}

MAKE_HOOK_MATCH(LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync, &LevelScenesTransitionSetupDataSO::BeforeScenesWillBeActivatedAsync, System::Threading::Tasks::Task*, LevelScenesTransitionSetupDataSO* self) {

    auto task = LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync(self);

    getLogger().info("Loading level before gameplay");

    UpdateLevel(self->gameplayCoreSceneSetupData->difficultyBeatmap, modifierSpeed);

    auto practiceSettings = self->gameplayCoreSceneSetupData->practiceSettings;
    practiceSpeed = practiceSettings ? practiceSettings->songSpeedMul : 1;

    return task;
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
    INSTALL_HOOK(logger, LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync);
    INSTALL_HOOK(logger, PlayerDataModel_Load);
    INSTALL_HOOK(logger, GameplayModifiersPanelController_RefreshTotalMultiplierAndRankUI);
    getLogger().info("Installed all hooks!");
}