#pragma once

#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "config.hpp"

float GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset);

std::optional<std::pair<Indicator, LevelPreset>> GetLevelPreset(DifficultyBeatmap beatmap);
void SetLevelPreset(DifficultyBeatmap beatmap, LevelPreset value);
void RemoveLevelPreset(DifficultyBeatmap beatmap);

struct Values {
    float halfJumpDuration;
    float halfJumpDistance;
    float njs;
    float GetJumpDuration() const { return halfJumpDuration * (getModConfig().Half.GetValue() ? 1 : 2); }
    float GetJumpDistance() const { return halfJumpDistance * (getModConfig().Half.GetValue() ? 1 : 2); }
};

Values GetLevelDefaults(DifficultyBeatmap beatmap, float speed);

float GetNPS(GlobalNamespace::BeatmapLevel* beatmap, GlobalNamespace::IReadonlyBeatmapData* data);
float GetBPM(GlobalNamespace::BeatmapLevel* beatmap);

float GetValue(Values& values, int id);

bool ConditionMet(ConditionPreset const& check, float njs, float nps, float bpm);

#define PROP(type, name) void Set##name(type value); type Get##name();
class Preset {
    enum class Type { Main, Level, Condition };
    Type type = Type::Main;
    LevelPreset internalLevel;
    ConditionPreset internalCondition;
    std::string internalLevelID;
    std::string internalMapID;
    void UpdateLevelPreset();
    int internalIdx;
    void UpdateCondition();
    float levelNJS = 0;
    float Bound(float value);

   public:
    PROP(float, MainValue);
    PROP(float, Distance);
    PROP(float, Duration);
    PROP(float, NJS);
    PROP(bool, OverrideNJS);
    PROP(bool, UseDuration);
    PROP(bool, UseDefaults);
    PROP(bool, UseBounds);
    PROP(float, BoundMin);
    PROP(float, BoundMax);
    void SetCondition(Condition value, int idx);
    Condition GetCondition(int idx);
    void RemoveCondition(int idx);
    int GetConditionCount();
    int GetConditionPresetIndex();
    void SyncCondition(int idxChange = 0);
    bool ShiftForward();
    bool ShiftBackward();
    LevelPreset GetAsLevelPreset();
    bool GetIsLevelPreset();
    void UpdateLevel(Values const& levelValues);

    Preset(std::string levelID, std::string mapID, Values const& levelValues);
    Preset(int conditionIdx, Values const& levelValues, bool setToLevel = true);
    Preset(Values const& levelValues, bool setToLevel = true);
    Preset() = default;
};
#undef PROP

void UpdateScoreSubmission(bool overridingNJS);
