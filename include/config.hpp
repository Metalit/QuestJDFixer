#pragma once

#include "config-utils/shared/config-utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"

DECLARE_JSON_CLASS(Condition,
    VALUE(int, AndOr)
    // 0: nps, 1: njs, 2: bpm
    VALUE(int, Type)
    // 0: under, 1: over
    VALUE(int, Comparison)
    VALUE(float, Value)
    DISCARD_EXTRA_FIELDS
)

DECLARE_JSON_CLASS(LevelPreset,
    VALUE(float, MainValue)
    VALUE(bool, UseDuration)
    VALUE(bool, OverrideNJS)
    VALUE(float, NJS)
    VALUE_DEFAULT(bool, Active, true)
    DISCARD_EXTRA_FIELDS
)

DECLARE_JSON_CLASS(ConditionPreset,
    VALUE_DEFAULT(float, Duration, 0.5)
    VALUE_DEFAULT(float, Distance, 10)
    VALUE_DEFAULT(bool, UseDuration, true)
    VECTOR_DEFAULT(Condition, Conditions, std::vector<Condition>{{}})
    VALUE(bool, SetToDefaults)
    VALUE(bool, DistanceBounds)
    VALUE(float, DistanceMin)
    VALUE(float, DistanceMax)
    VALUE(bool, OverrideNJS)
    VALUE_DEFAULT(float, NJS, 18)
    DISCARD_EXTRA_FIELDS
)

DECLARE_CONFIG(ModConfig,
    CONFIG_VALUE(Disable, bool, "Disable Mod", false, "Whether to disable the mod entirely, allowing the base game settings to take effect");
    CONFIG_VALUE(Practice, bool, "Fix Practice NJS", true, "Makes practice mode NJS be unchanged by the speed modifier");
    CONFIG_VALUE(Decimals, int, "Decimal Precision", 2, "The decimal precision to use for distance, duration, and njs values");
    CONFIG_VALUE(Half, bool, "Half Values", true, "Whether to use half values for distance and duration, which correspond to the real note movements but may be less familiar");

    CONFIG_VALUE(Duration, float, "Jump Duration", 0.5, "The jump duration to set the level to");
    CONFIG_VALUE(UseDuration, bool, "Slider Controls", true, "Whether the main slider controls distance or duration");
    CONFIG_VALUE(AutoDef, bool, "Set To Level Values", false, "Automatically resets the jump values to the level's when one is selected");
    CONFIG_VALUE(Distance, float, "Half Jump Distance", 18.0, "The half jump distance to set the level to");
    CONFIG_VALUE(BoundJD, bool, "Use Bounds", false, "Prevents calculated jump distance from being under or over configured bounds");
    CONFIG_VALUE(MinJD, float, "Min Jump Distance", 10.0, "The minimum half jump distance allowed");
    CONFIG_VALUE(MaxJD, float, "Max Jump Distance", 20.0, "The maximum half jump distance allowed");
    CONFIG_VALUE(UseNJS, bool, "Override NJS", false, "Overrides the note jump speed (disables score submission)");

    CONFIG_VALUE(NJS, float, "Note Jump Speed", 18.0, "The note jump speed to set the level to");
    CONFIG_VALUE(Presets, std::vector<ConditionPreset>, "Presets", std::vector<ConditionPreset>{});
    CONFIG_VALUE(Levels, StringKeyedMap<LevelPreset>, "Level Presets", StringKeyedMap<LevelPreset>{});
)

void UpdateLevel(GlobalNamespace::IDifficultyBeatmap* beatmap, float speed);
void UpdateNotesPerSecond(float nps);
LevelPreset GetAppliedValues();

// gameplay menu config
void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation);
