#pragma once

#include "config.hpp"

#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"

float GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset);

struct Values {
    float halfJumpDuration;
    float halfJumpDistance;
    float njs;
    float GetJumpDuration() const {
        return halfJumpDuration * (getModConfig().Half.GetValue() ? 1 : 2);
    }
    float GetJumpDistance() const {
        return halfJumpDistance * (getModConfig().Half.GetValue() ? 1 : 2);
    }
};

Values GetLevelDefaults(GlobalNamespace::IDifficultyBeatmap* beatmap, float speed);

float GetNPS(GlobalNamespace::IPreviewBeatmapLevel* beatmap, GlobalNamespace::IReadonlyBeatmapData* data);
float GetBPM(GlobalNamespace::IDifficultyBeatmap* beatmap);

float GetValue(Values& values, int id);

bool ConditionMet(ConditionPreset const& check, float njs, float nps, float bpm);

#define PROP(type, name) void Set##name(type value); type Get##name();
class Preset {
    enum class Type {
        Main,
        Level,
        Condition
    };
    Type type = Type::Main;
    LevelPreset internalLevel;
    ConditionPreset internalCondition;
    std::string internalLevelID;
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

    Preset(std::string levelID, Values const& levelValues);
    Preset(int conditionIdx, Values const& levelValues, bool setToLevel = true);
    Preset(Values const& levelValues, bool setToLevel = true);
    Preset() = default;
};
#undef PROP

void UpdateScoreSubmission(bool overridingNJS);
