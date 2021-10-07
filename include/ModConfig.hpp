#pragma once
#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(ModConfig,

    CONFIG_VALUE(ReactTime, float, "Preferred Reaction Time", 1.0, "Makes the notes always take the same amount of time to reach you, so if the njs is higher, they spawn farther away.");
    CONFIG_VALUE(AutoReact, bool, "Use Reaction Time", false, "Automatically sets the jump distance to the preferred reaction time");
    CONFIG_VALUE(AutoDef, float, "Use Level Jump Distance", false, "Automatically sets the jump distance to that of the level");
    CONFIG_VALUE(JumpDist, float, "Jump Distance", 18.0, "The jump distance to set the level to");

    CONFIG_INIT_FUNCTION(
        CONFIG_INIT_VALUE(ReactTime);
        CONFIG_INIT_VALUE(AutoReact);
        CONFIG_INIT_VALUE(AutoDef);
        CONFIG_INIT_VALUE(JumpDist);
    )
)