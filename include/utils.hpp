#pragma once

#include "GlobalNamespace/BeatmapDifficulty.hpp"

float GetDesiredJumpDuration(float noteJumpSpeed);

float GetDefaultJumpDuration(float njs, float beatDuration, float startBeatOffset);

float GetDefaultDifficultyNJS(GlobalNamespace::BeatmapDifficulty difficulty);
