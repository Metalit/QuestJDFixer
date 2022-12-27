#pragma once

#define HAS_CODEGEN
#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(ModConfig,
    CONFIG_VALUE(ReactTime, float, "Jump Duration", 0.5, "The jump duration to set the level to");
    CONFIG_VALUE(AutoReact, bool, "Use Jump Duration", true, "Whether to use the jump duration instead of jump distance");
    CONFIG_VALUE(AutoDef, bool, "Set To Level Values", false, "Automatically resets the jump values to the level's when one is selected");
    CONFIG_VALUE(JumpDist, float, "Half Jump Distance", 18.0, "The half jump distance to set the level to");
    CONFIG_VALUE(BoundJD, bool, "Use Bounds", false, "Whether to set minimum and maximum jump distance values");
    CONFIG_VALUE(MinJD, float, "Min Jump Distance", 10.0, "The minimum jump distance allowed");
    CONFIG_VALUE(MaxJD, float, "Max Jump Distance", 20.0, "The maximum jump distance allowed");
    CONFIG_VALUE(UseNJS, bool, "Override NJS", false, "Whether to override the note jump speed (disables score submission)");
    CONFIG_VALUE(NJS, float, "Note Jump Speed", 18.0, "The note jump speed to set the level to");
    CONFIG_VALUE(Disable, bool, "Disable Mod", false, "Whether to disable the mod entirely, allowing the base game settings to take effect");
)

void UpdateUI();
