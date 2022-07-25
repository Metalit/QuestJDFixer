#include "utils.hpp"
#include "main.hpp"
#include "ModConfig.hpp"

#include "bs-utils/shared/utils.hpp"

float GetDesiredHalfJumpDuration(float noteJumpSpeed, float songSpeed) {
    noteJumpSpeed /= songSpeed;
    // configured jump speed should be applied before this function
    // using jump duration
    if(getModConfig().AutoReact.GetValue()) {
        // since the song speed is applied after jump distance calculation, we need to factor it in here
        float halfJumpDuration = getModConfig().ReactTime.GetValue() * songSpeed;
        // still clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            float halfJumpDistance = halfJumpDuration * noteJumpSpeed;
            if(halfJumpDistance > getModConfig().MaxJD.GetValue())
                halfJumpDistance = getModConfig().MaxJD.GetValue();
            if(halfJumpDistance < getModConfig().MinJD.GetValue())
                halfJumpDistance = getModConfig().MinJD.GetValue();
            halfJumpDuration = halfJumpDistance / noteJumpSpeed;
        }
        return halfJumpDuration;
    // using jump distance
    } else {
        float halfJumpDistance = getModConfig().JumpDist.GetValue();
        // clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            if(halfJumpDistance > getModConfig().MaxJD.GetValue())
                halfJumpDistance = getModConfig().MaxJD.GetValue();
            if(halfJumpDistance < getModConfig().MinJD.GetValue())
                halfJumpDistance = getModConfig().MinJD.GetValue();
        }
        return halfJumpDistance / noteJumpSpeed;
    }
}

const float startHalfJumpDurationInBeats = 4;
const float maxHalfJumpDistance = 18;

float GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset) {
    // transforms into duration in beats, note jump speed in beats
	float halfJumpDuration = startHalfJumpDurationInBeats;
	float distancePerBeat = njs * beatDuration;
    // could be do-while. smh my head, beat games
	float halfJumpDistance = distancePerBeat * halfJumpDuration;
	while(halfJumpDistance > maxHalfJumpDistance - 0.001) {
		halfJumpDuration /= (float) 2;
		halfJumpDistance = distancePerBeat * halfJumpDuration;
	}
    // this is where the "dynamic" jump distance setting is added or subtracted
	halfJumpDuration += startBeatOffset;
    // keeps it from being too low, but is flawed in my opinion:
    // it uses duration in beats, making the minimum too large if a map has a low enough bpm
	if(halfJumpDuration < 0.25) {
		halfJumpDuration = 0.25;
	}
    // turn back into duration in seconds, instead of in beats
	return halfJumpDuration * beatDuration;
}

float GetDefaultDifficultyNJS(GlobalNamespace::BeatmapDifficulty difficulty) {
    using namespace GlobalNamespace;
    switch(difficulty) {
        case BeatmapDifficulty::Easy:
            return 10;
        case BeatmapDifficulty::Normal:
            return 10;
        case BeatmapDifficulty::Hard:
            return 10;
        case BeatmapDifficulty::Expert:
            return 12;
        case BeatmapDifficulty::ExpertPlus:
            return 16;
        default:
            return 5;
    }
}

void UpdateScoreSubmission() {
    if(getModConfig().UseNJS.GetValue() && !getModConfig().Disable.GetValue())
        bs_utils::Submission::disable(getModInfo());
    else
        bs_utils::Submission::enable(getModInfo());
}
