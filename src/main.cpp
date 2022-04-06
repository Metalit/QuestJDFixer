#include "main.hpp"
#include "utils.hpp"
#include "ModConfig.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "bs-utils/shared/utils.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData_NoteJumpValueType.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"

#include "UnityEngine/Vector3.hpp"

#include "questui/shared/QuestUI.hpp"
#include "custom-types/shared/register.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;
DEFINE_CONFIG(ModConfig);

IDifficultyBeatmap* currentBeatmap;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

ModInfo& getModInfo() {
    return modInfo;
}

void UpdateLevel(StandardLevelDetailView* self) {
    // don't run when loading the same level in a row
    if(currentBeatmap == self->selectedDifficultyBeatmap)
        return;
    currentBeatmap = self->selectedDifficultyBeatmap;

    // reset values if configured to
    if(getModConfig().AutoDef.GetValue()) {
        getLogger().info("Populating level values");

        float bpm = ((IPreviewBeatmapLevel*) self->level)->get_beatsPerMinute();

        float njs = self->selectedDifficultyBeatmap->get_noteJumpMovementSpeed();
        if(njs <= 0)
            njs = GetDefaultDifficultyNJS(self->selectedDifficultyBeatmap->get_difficulty());

        float offset = self->selectedDifficultyBeatmap->get_noteJumpStartBeatOffset();
        
        float jumpDuration = GetDefaultJumpDuration(njs, 60 / bpm, offset);
        float jumpDistance = jumpDuration * njs;
        
        // clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            if(jumpDistance > getModConfig().MaxJD.GetValue())
                jumpDistance = getModConfig().MaxJD.GetValue();
            if(jumpDistance < getModConfig().MinJD.GetValue())
                jumpDistance = getModConfig().MinJD.GetValue();
            jumpDuration = jumpDistance / njs;
        }

        getModConfig().ReactTime.SetValue(jumpDuration);
        getModConfig().JumpDist.SetValue(jumpDistance);
        getModConfig().NJS.SetValue(njs);

        UpdateUI();
    }
}

// Hooks
MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Init, &BeatmapObjectSpawnMovementData::Init,
        void, BeatmapObjectSpawnMovementData* self, int noteLinesCount, float startNoteJumpMovementSpeed, float startBpm, BeatmapObjectSpawnMovementData::NoteJumpValueType noteJumpValueType, float noteJumpValue, IJumpOffsetYProvider* jumpOffsetYProvider, UnityEngine::Vector3 rightVec, UnityEngine::Vector3 forwardVec) {
    
    if(!getModConfig().Disable.GetValue()) {
        if(getModConfig().UseNJS.GetValue())
            startNoteJumpMovementSpeed = getModConfig().NJS.GetValue();

        noteJumpValueType = BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
        noteJumpValue = GetDesiredJumpDuration(startNoteJumpMovementSpeed) / 2;
        
        getLogger().info("Changing jump duration to %.2f", noteJumpValue * 2);
    }

    BeatmapObjectSpawnMovementData_Init(self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpValueType, noteJumpValue, jumpOffsetYProvider, rightVec, forwardVec);
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);

    getLogger().info("Loading Level");
    
    UpdateLevel(self);
}

extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
	
    getLogger().info("Completed setup!");
}

extern "C" void load() {
    getLogger().info("Installing hooks...");
    il2cpp_functions::Init();

    getModConfig().Init(modInfo);
    if(getModConfig().UseNJS.GetValue())
        bs_utils::Submission::disable(modInfo);

    QuestUI::Init();
    QuestUI::Register::RegisterGameplaySetupMenu(modInfo, QuestUI::Register::MenuType::All, GameplaySettings);

    LoggerContextObject logger = getLogger().WithContext("load");

    // Install hooks
    INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_Init);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    getLogger().info("Installed all hooks!");
}