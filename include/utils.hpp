#pragma once

#include "config.hpp"

#include "GlobalNamespace/BeatmapDifficulty.hpp"

float GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset);

struct Values {
    float halfJumpDuration;
    float halfJumpDistance;
    float njs;
};

Values GetLevelDefaults(GlobalNamespace::IDifficultyBeatmap* beatmap);

float GetNPS(GlobalNamespace::IDifficultyBeatmap* beatmap);
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
    LevelPreset GetAsLevelPreset();
    void UpdateLevel(Values const& levelValues);

    Preset(LevelPreset const& preset, Values const& levelValues);
    Preset(int conditionIdx, Values const& levelValues);
    Preset(Values const& levelValues);
    Preset() = default;
};
#undef PROP

void UpdateScoreSubmission(bool overridingNJS);
