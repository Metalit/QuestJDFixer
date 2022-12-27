#include "main.hpp"
#include "utils.hpp"
#include "ModConfig.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData_NoteJumpValueType.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"
#include "GlobalNamespace/LevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"

#include "UnityEngine/Vector3.hpp"

#include "questui/shared/QuestUI.hpp"
#include "custom-types/shared/register.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;
DEFINE_CONFIG(ModConfig);

IDifficultyBeatmap* currentBeatmap = nullptr;
float songSpeed = 1;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

ModInfo& getModInfo() {
    return modInfo;
}

void UpdateLevel(IDifficultyBeatmap* beatmap, bool forceSet = false) {
    // don't run when loading the same level in a row
    if(currentBeatmap == beatmap && !forceSet)
        return;
    currentBeatmap = beatmap;

    // reset values if configured to
    if(getModConfig().AutoDef.GetValue()) {
        getLogger().info("Populating level values");

        float bpm = ((IPreviewBeatmapLevel*) beatmap->get_level())->get_beatsPerMinute();

        float njs = beatmap->get_noteJumpMovementSpeed();
        if(njs <= 0)
            njs = GetDefaultDifficultyNJS(beatmap->get_difficulty());

        float offset = beatmap->get_noteJumpStartBeatOffset();

        float halfJumpDuration = GetDefaultHalfJumpDuration(njs, 60 / bpm, offset);
        float halfJumpDistance = halfJumpDuration * njs;

        // clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            if(halfJumpDistance > getModConfig().MaxJD.GetValue())
                halfJumpDistance = getModConfig().MaxJD.GetValue();
            if(halfJumpDistance < getModConfig().MinJD.GetValue())
                halfJumpDistance = getModConfig().MinJD.GetValue();
            halfJumpDuration = halfJumpDistance / njs;
        }

        getModConfig().ReactTime.SetValue(halfJumpDuration);
        getModConfig().JumpDist.SetValue(halfJumpDistance);
        getModConfig().NJS.SetValue(njs);

        UpdateUI();
    }
}

void SetToLevelDefaults() {
    if(currentBeatmap)
        UpdateLevel(currentBeatmap, true);
}

// Hooks
MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Init, &BeatmapObjectSpawnMovementData::Init,
        void, BeatmapObjectSpawnMovementData* self, int noteLinesCount, float startNoteJumpMovementSpeed, float startBpm, BeatmapObjectSpawnMovementData::NoteJumpValueType noteJumpValueType, float noteJumpValue, IJumpOffsetYProvider* jumpOffsetYProvider, UnityEngine::Vector3 rightVec, UnityEngine::Vector3 forwardVec) {

    if(!getModConfig().Disable.GetValue()) {
        if(getModConfig().UseNJS.GetValue())
            startNoteJumpMovementSpeed = getModConfig().NJS.GetValue();

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = GetDesiredHalfJumpDuration(startNoteJumpMovementSpeed, songSpeed);

        self->moveSpeed /= songSpeed;
        startNoteJumpMovementSpeed /= songSpeed;

        getLogger().info("Changing jump duration to %.2f", noteJumpValue * 2);
    }

    BeatmapObjectSpawnMovementData_Init(self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpValueType, noteJumpValue, jumpOffsetYProvider, rightVec, forwardVec);
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);

    getLogger().info("Loading level in menu");

    UpdateLevel(self->selectedDifficultyBeatmap);
}

MAKE_HOOK_MATCH(LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync, &LevelScenesTransitionSetupDataSO::BeforeScenesWillBeActivatedAsync, System::Threading::Tasks::Task*, LevelScenesTransitionSetupDataSO* self) {
    auto task = LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync(self);

    getLogger().info("Loading level before gameplay");

    UpdateLevel(self->gameplayCoreSceneSetupData->difficultyBeatmap);

    auto practiceSettings = self->gameplayCoreSceneSetupData->practiceSettings;
    // auto modifiers = self->gameplayCoreSceneSetupData->gameplayModifiers;
    // practice settings override modifiers if enabled
    // songSpeed = practiceSettings ? practiceSettings->songSpeedMul : modifiers->get_songSpeedMul();
    songSpeed = practiceSettings ? practiceSettings->songSpeedMul : 1;

    return task;
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
    UpdateScoreSubmission();

    QuestUI::Init();
    QuestUI::Register::RegisterGameplaySetupMenu(modInfo, QuestUI::Register::MenuType::All, GameplaySettings);

    LoggerContextObject logger = getLogger().WithContext("load");

    // Install hooks
    INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_Init);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, LevelScenesTransitionSetupDataSO_BeforeScenesWillBeActivatedAsync);
    getLogger().info("Installed all hooks!");
}