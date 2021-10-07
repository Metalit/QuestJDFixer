#include "main.hpp"
#include "ModConfig.hpp"

#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-type-check.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"

#include "UnityEngine/Vector3.hpp"

#include "questui/shared/QuestUI.hpp"
#include "config-utils/shared/config-utils.hpp"
#include "custom-types/shared/register.hpp"

#include <math.h>
#include <iomanip>
#include <sstream>

using namespace GlobalNamespace;

static ModInfo modInfo;
DEFINE_CONFIG(ModConfig);

GlobalNamespace::IDifficultyBeatmap* lastDiff;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

static float CalculateJumpDistance(float bpm, float njs, float offset) {
    // ost1, why?
    if(njs <= 0)
        njs = 10;

    float jumpdistance;
    float halfjump = 4;
    float num = 60 / bpm;

    while (njs * num * halfjump > 18)
        halfjump /= 2;

    halfjump += offset;
    if (halfjump < 1) halfjump = 1;

    jumpdistance = njs * num * halfjump * 2;

    return jumpdistance;
}

static float CalculateOffset(float bpm, float njs, float desiredJD) {
    // ost1, why?
    if(njs <= 0)
        njs = 10;

    float simOffset = 0;
    float spbCurr = 60 / bpm;
    float num2Curr = 4;

    while (njs * spbCurr * num2Curr > 18)
        num2Curr /= 2;

    float jumpDurCurr = num2Curr * spbCurr * 2;
    float jumpDisCurr = njs * jumpDurCurr;

    float desiredJumpDur = desiredJD / njs;
    float jumpDurMul = desiredJumpDur / jumpDurCurr;

    simOffset = (num2Curr * jumpDurMul) - num2Curr;

    return simOffset;
}

static void updateLevel(GlobalNamespace::StandardLevelDetailView* self) {
    // don't run when loading the same level in a row
    if(lastDiff == self->selectedDifficultyBeatmap)
        return;
    lastDiff = self->selectedDifficultyBeatmap;

    // GlobalNamespace::IPreviewBeatmapLevel* previewLevel = (GlobalNamespace::IPreviewBeatmapLevel*)(self->level);
    float bpm = reinterpret_cast<GlobalNamespace::IPreviewBeatmapLevel*>(self->level)->get_beatsPerMinute();

    float njs = self->selectedDifficultyBeatmap->get_noteJumpMovementSpeed();
    float offset = self->selectedDifficultyBeatmap->get_noteJumpStartBeatOffset();

    float jumpDistance = CalculateJumpDistance(bpm, njs, offset);

    changeDefJD(jumpDistance);

    changeReactJD(njs * getModConfig().ReactTime.GetValue());
}

// Hooks
MAKE_HOOK_MATCH(NotesSpawn, &GlobalNamespace::BeatmapObjectSpawnMovementData::Init, void, GlobalNamespace::BeatmapObjectSpawnMovementData* self, int noteLinesCount, float startNoteJumpMovementSpeed, float startBpm, float noteJumpStartBeatOffset, float jumpOffsetY, UnityEngine::Vector3 rightVec, UnityEngine::Vector3 forwardVec) {
    NotesSpawn(self, noteLinesCount, startNoteJumpMovementSpeed, startBpm, noteJumpStartBeatOffset, jumpOffsetY, rightVec, forwardVec);

    float desiredJD = getModConfig().JumpDist.GetValue();

    getLogger().info("Changing jump distance to %f", desiredJD);

    float njs = startNoteJumpMovementSpeed;

    self->noteJumpStartBeatOffset = CalculateOffset(startBpm, njs, desiredJD);
}

MAKE_HOOK_MATCH(NotesUpdate, &GlobalNamespace::BeatmapObjectSpawnMovementData::Update, void, GlobalNamespace::BeatmapObjectSpawnMovementData* self, float bpm, float jumpOffsetY) {
    NotesUpdate(self, bpm, jumpOffsetY);

    float desiredJD = getModConfig().JumpDist.GetValue();

    float njs = self->startNoteJumpMovementSpeed * bpm / self->startBpm;
    
    // recalculate desired jd in case of bpm change
    float offset = CalculateOffset(bpm, njs, desiredJD);
    self->noteJumpStartBeatOffset = offset;

    // does the same calculation as normal, except removes the min jump distance
    float spb = 60 / bpm;
    float num2 = 4;
    while (njs * spb * num2 > 18)
        num2 /= 2;
    
    num2 += offset;

    self->jumpDuration = spb * num2 * 2;
    self->jumpDistance = njs * self->jumpDuration;

    self->moveEndPos = self->centerPos + self->forwardVec * (self->jumpDistance * 0.5);
    self->jumpEndPos = self->centerPos - self->forwardVec * (self->jumpDistance * 0.5);
    self->moveStartPos = self->centerPos + self->forwardVec * (self->moveDistance + self->jumpDistance * 0.5);
    self->spawnAheadTime = self->moveDuration + self->jumpDuration * 0.5;
}

MAKE_HOOK_MATCH(LoadLevel, &GlobalNamespace::StandardLevelDetailView::RefreshContent, void, GlobalNamespace::StandardLevelDetailView* self) {
    LoadLevel(self);

    getLogger().info("Loading Level");
    
    updateLevel(self);
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

    // config stuff idk
    getModConfig().Init(modInfo);
    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController(modInfo, DidActivate);
    QuestUI::Register::RegisterGameplaySetupMenu(modInfo, 15, GameplaySettings);

    LoggerContextObject logger = getLogger().WithContext("load");

    // Install hooks
    INSTALL_HOOK(logger, NotesSpawn);
    INSTALL_HOOK(logger, NotesUpdate);
    INSTALL_HOOK(logger, LoadLevel);
    getLogger().info("Installed all hooks!");
}