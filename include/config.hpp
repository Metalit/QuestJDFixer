#pragma once

#include "config-utils/shared/config-utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"

DECLARE_JSON_CLASS(Condition,
    VALUE(int, AndOr)
    // 0: nps, 1: njs, 2: bpm
    VALUE(int, Type)
    // 0: under, 1: over
    VALUE(int, Comparison)
    VALUE(float, Value);
    DISCARD_EXTRA_FIELDS
)

DECLARE_JSON_CLASS(LevelPreset,
    VALUE(float, Duration)
    VALUE(bool, UseDuration)
    VALUE(bool, OverrideNJS)
    VALUE(float, NJS)
    DISCARD_EXTRA_FIELDS
)

DECLARE_JSON_CLASS(ConditionPreset,
    VALUE(float, Duration)
    VALUE(float, Distance)
    VALUE(bool, UseDuration)
    VECTOR(Condition, Conditions)
    VALUE(bool, SetToDefaults)
    VALUE(bool, DistanceBounds)
    VALUE(float, DistanceMin)
    VALUE(float, DistanceMax)
    VALUE(bool, OverrideNJS)
    VALUE(float, NJS)
    DISCARD_EXTRA_FIELDS
)

DECLARE_CONFIG(ModConfig,
    CONFIG_VALUE(Disable, bool, "Disable Mod", false, "Whether to disable the mod entirely, allowing the base game settings to take effect");

    CONFIG_VALUE(Duration, float, "Jump Duration", 0.5, "The jump duration to set the level to");
    CONFIG_VALUE(UseDuration, bool, "Slider Controls", true, "Whether the main slider controls distance or duration");
    CONFIG_VALUE(AutoDef, bool, "Set To Level Values", false, "Automatically resets the jump values to the level's when one is selected");
    CONFIG_VALUE(Distance, float, "Half Jump Distance", 18.0, "The half jump distance to set the level to");
    CONFIG_VALUE(BoundJD, bool, "Use Bounds", false, "Prevents calculated jump distance from being under or over configured bounds");
    CONFIG_VALUE(MinJD, float, "Min Jump Distance", 10.0, "The minimum half jump distance allowed");
    CONFIG_VALUE(MaxJD, float, "Max Jump Distance", 20.0, "The maximum half jump distance allowed");
    CONFIG_VALUE(UseNJS, bool, "Override NJS", false, "Overrides the note jump speed (disables score submission)");

    CONFIG_VALUE(NJS, float, "Note Jump Speed", 18.0, "The note jump speed to set the level to");
    CONFIG_VALUE(Presets, std::vector<ConditionPreset>, "Presets", {});
    CONFIG_VALUE(Levels, StringKeyedMap<LevelPreset>, "Level Presets", {});
)

void UpdateLevel(GlobalNamespace::IDifficultyBeatmap* beatmap);
LevelPreset GetAppliedValues();

// gameplay menu config
void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation);
