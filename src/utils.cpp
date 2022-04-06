#include "utils.hpp"
#include "main.hpp"
#include "ModConfig.hpp"

float GetDesiredJumpDuration(float noteJumpSpeed) {
    // configured jump speed should be applied before this function
    // using jump duration
    if(getModConfig().AutoReact.GetValue()) {
        float jumpDuration = getModConfig().ReactTime.GetValue();
        // still clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            float jumpDistance = jumpDuration * noteJumpSpeed;
            if(jumpDistance > getModConfig().MaxJD.GetValue())
                jumpDistance = getModConfig().MaxJD.GetValue();
            if(jumpDistance < getModConfig().MinJD.GetValue())
                jumpDistance = getModConfig().MinJD.GetValue();
            jumpDuration = jumpDistance / noteJumpSpeed;
        }
        return jumpDuration;
    // using jump distance
    } else {
        float jumpDistance = getModConfig().JumpDist.GetValue();
        // clamp to distance if enabled
        if(getModConfig().BoundJD.GetValue()) {
            if(jumpDistance > getModConfig().MaxJD.GetValue())
                jumpDistance = getModConfig().MaxJD.GetValue();
            if(jumpDistance < getModConfig().MinJD.GetValue())
                jumpDistance = getModConfig().MinJD.GetValue();
        }
        return jumpDistance / noteJumpSpeed;
    }
}

const float startHalfJumpDurationInBeats = 4;
const float maxHalfJumpDistance = 18;

float GetDefaultJumpDuration(float njs, float beatDuration, float startBeatOffset) {
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
    // turn back into duration in seconds, instead of in beats, also turn from half to full jump
	return halfJumpDuration * beatDuration * 2;
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
