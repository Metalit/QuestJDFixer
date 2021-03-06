#pragma once

#include "GlobalNamespace/BeatmapDifficulty.hpp"

float GetDesiredHalfJumpDuration(float noteJumpSpeed, float songSpeed);

float GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset);

float GetDefaultDifficultyNJS(GlobalNamespace::BeatmapDifficulty difficulty);

void UpdateScoreSubmission();
