#include "main.hpp"
#include "utils.hpp"

#include "bs-utils/shared/utils.hpp"

#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IBeatmapLevelData.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/CustomDifficultyBeatmap.hpp"
#include "GlobalNamespace/IBeatmapDataBasicInfo.hpp"
#include "GlobalNamespace/BeatmapLevelSO_DifficultyBeatmap.hpp"
#include "GlobalNamespace/BeatmapDataSO.hpp"
#include "BeatmapSaveDataVersion3/BeatmapSaveData.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"

using namespace GlobalNamespace;

inline float BoundDistance(float halfJumpDistance, float min, float max) {
    if(halfJumpDistance < min)
        return min;
    if(halfJumpDistance > max)
        return max;
    return halfJumpDistance;
}
inline float BoundDuration(float halfJumpDuration, float minDist, float maxDist, float njs) {
    float halfJumpDistance = halfJumpDuration * njs;
    return BoundDistance(halfJumpDistance, minDist, maxDist) / njs;
}

static constexpr float startHalfJumpDurationInBeats = 4;
static constexpr float maxHalfJumpDistance = 18;

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

std::vector<std::pair<std::string, std::vector<int>>> GetAllBeatmaps(IDifficultyBeatmap* beatmap) {
    std::vector<std::pair<std::string, std::vector<int>>> ret;

    ArrayW<IDifficultyBeatmapSet*> sets(beatmap->get_level()->get_beatmapLevelData()->get_difficultyBeatmapSets());
    for(auto& set : sets) {
        std::string name = set->get_beatmapCharacteristic()->serializedName;

        ArrayW<IDifficultyBeatmap*> maps(set->get_difficultyBeatmaps());
        std::vector<int> diffs;
        for(auto& map : maps)
            diffs.emplace_back((int) map->get_difficulty());

        ret.emplace_back(name, diffs);
    }
    return ret;
}

void MigrateLevelPresets(IDifficultyBeatmap* beatmap) {
    std::string id = beatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if(iter == map.end())
        return;
    // saved without difficulty/characteristic info, needs to be duplicated to all
    if(auto value = iter->second.GetValue<LevelPreset>()) {
        getLogger().debug("Duplicating unspecified level preset to all difficulties");
        StringKeyedMap<LevelPreset> newValue;
        auto allEntries = GetAllBeatmaps(beatmap);
        for(auto& [charName, diffs] : allEntries) {
            for(auto& diff : diffs)
                newValue[charName + std::to_string(diff)] = *value;
        }
        iter->second.SetValue(newValue);
        getModConfig().Levels.SetValue(map);
    }
}

std::optional<std::pair<Indicator, LevelPreset>> GetLevelPreset(IDifficultyBeatmap* beatmap) {
    MigrateLevelPresets(beatmap);
    std::string id = beatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if(iter == map.end())
        return std::nullopt;
    // guaranteed to be a map after migrate
    auto charDiffs = *iter->second.GetValue<StringKeyedMap<LevelPreset>>();
    // search: characteristic ser. name + difficulty "Standard2" to preset
    std::string charDiff = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    charDiff += std::to_string((int) beatmap->get_difficulty());
    auto iter2 = charDiffs.find(charDiff);
    if(iter2 == charDiffs.end())
        return std::nullopt;
    Indicator ret = {
        .id = id,
        .map = charDiff,
    };
    return std::make_pair(ret, iter2->second);
}

void SetLevelPreset(std::string levelID, std::string mapID, LevelPreset value) {
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(levelID);
    if(iter != map.end()) {
        auto charDiffs = *iter->second.GetValue<StringKeyedMap<LevelPreset>>();
        charDiffs[mapID] = value;
        iter->second.SetValue(charDiffs);
    } else
        map[levelID] = StringKeyedMap<LevelPreset>({{mapID, value}});
    getModConfig().Levels.SetValue(map);
}

void SetLevelPreset(GlobalNamespace::IDifficultyBeatmap* beatmap, LevelPreset value) {
    MigrateLevelPresets(beatmap);

    std::string id = beatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    std::string charDiff = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    charDiff += std::to_string((int) beatmap->get_difficulty());

    SetLevelPreset(id, charDiff, value);
}

void RemoveLevelPreset(GlobalNamespace::IDifficultyBeatmap* beatmap) {
    MigrateLevelPresets(beatmap);

    std::string id = beatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    std::string charDiff = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    charDiff += std::to_string((int) beatmap->get_difficulty());

    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if(iter == map.end())
        return;
    auto charDiffs = *iter->second.GetValue<StringKeyedMap<LevelPreset>>();
    auto iter2 = charDiffs.find(charDiff);
    if(iter2 == charDiffs.end())
        return;
    charDiffs.erase(iter2);

    iter->second.SetValue(charDiffs);
    getModConfig().Levels.SetValue(map);
}

inline float GetDefaultDifficultyNJS(BeatmapDifficulty difficulty) {
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

Values GetLevelDefaults(IDifficultyBeatmap* beatmap, float speed) {
    if(!beatmap)
        return {};

    float bpm = ((IPreviewBeatmapLevel*) beatmap->get_level())->get_beatsPerMinute();

    float njs = beatmap->get_noteJumpMovementSpeed();
    if(njs <= 0)
        njs = GetDefaultDifficultyNJS(beatmap->get_difficulty());

    float offset = beatmap->get_noteJumpStartBeatOffset();

    float halfJumpDuration = GetDefaultHalfJumpDuration(njs, 60 / bpm, offset);
    njs *= speed;
    float halfJumpDistance = halfJumpDuration * njs;

    return Values{
        .halfJumpDuration = halfJumpDuration,
        .halfJumpDistance = halfJumpDistance,
        .njs = njs
    };
}

float GetNPS(IPreviewBeatmapLevel* level, IReadonlyBeatmapData* data) {
    float length = level->get_songDuration();
    int noteCount = data->i_IBeatmapDataBasicInfo()->get_cuttableNotesCount();
    return noteCount / length;
}
float GetBPM(IDifficultyBeatmap* beatmap) {
    return ((IPreviewBeatmapLevel*) beatmap->get_level())->get_beatsPerMinute();
}

float GetValue(Values& values, int id) {
    switch(id) {
    case 1:
        return values.GetJumpDuration();
    case 2:
        return values.GetJumpDistance();
    case 3:
        return values.njs;
    default:
        return 0;
    }
}

bool ConditionMet(ConditionPreset const& check, float njs, float nps, float bpm) {
    bool matches = true;
    for(auto const& condition : check.Conditions) {
        float value;
        if(condition.Type == 0)
            value = nps;
        else if(condition.Type == 1)
            value = njs;
        else
            value = bpm;
        bool thisMatch;
        if(condition.Comparison == 0)
            thisMatch = value <= condition.Value;
        else
            thisMatch = value >= condition.Value;
        // basically, or takes proirity over and
        if(condition.AndOr == 0)
            matches = matches && thisMatch;
        else if(matches)
            return true;
        else
            matches = thisMatch;
    }
    return matches;
}

void Preset::UpdateLevelPreset() {
    SetLevelPreset(internalLevelID, internalMapID, internalLevel);
}

void Preset::UpdateCondition() {
    auto presets = getModConfig().Presets.GetValue();
    presets[internalIdx] = internalCondition;
    getModConfig().Presets.SetValue(presets);
}

float Preset::Bound(float value) {
    if(!GetUseBounds())
        return value;
    if(GetUseDuration())
        return BoundDuration(value, GetBoundMin(), GetBoundMax(), GetNJS());
    else
        return BoundDistance(value, GetBoundMin(), GetBoundMax());
}

void Preset::SetMainValue(float value) {
    switch(type) {
    case Type::Main:
        if(getModConfig().UseDuration.GetValue())
            getModConfig().Duration.SetValue(value);
        else
            getModConfig().Distance.SetValue(value);
        break;
    case Type::Condition:
        if(internalCondition.UseDuration)
            internalCondition.Duration = value;
        else
            internalCondition.Distance = value;
        UpdateCondition();
        break;
    case Type::Level:
        internalLevel.MainValue = value;
        UpdateLevelPreset();
        break;
    }
}

float Preset::GetMainValue() {
    switch(type) {
    case Type::Main:
        if(getModConfig().UseDuration.GetValue())
            return getModConfig().Duration.GetValue();
        return getModConfig().Distance.GetValue();
    case Type::Condition:
        if(internalCondition.UseDuration)
            return internalCondition.Duration;
        return internalCondition.Distance;
    case Type::Level:
        return internalLevel.MainValue;
    }
}

void Preset::SetDuration(float value) {
    if(GetUseDuration())
        SetMainValue(value);
    else
        SetMainValue(value * GetNJS());
}

float Preset::GetDuration() {
    if(GetUseDuration())
        return Bound(GetMainValue());
    return Bound(GetMainValue()) / GetNJS();
}

void Preset::SetDistance(float value) {
    if(GetUseDuration())
        SetMainValue(value / GetNJS());
    else
        SetMainValue(value);
}

float Preset::GetDistance() {
    if(GetUseDuration())
        return Bound(GetMainValue()) * GetNJS();
    return Bound(GetMainValue());
}

void Preset::SetNJS(float value) {
    switch(type) {
    case Type::Main:
        getModConfig().NJS.SetValue(value);
        break;
    case Type::Condition:
        internalCondition.NJS = value;
        UpdateCondition();
        break;
    case Type::Level:
        internalLevel.NJS = value;
        UpdateLevelPreset();
        break;
    }
}

float Preset::GetNJS() {
    if(!GetOverrideNJS())
        return levelNJS;
    switch(type) {
    case Type::Main:
        return getModConfig().NJS.GetValue();
    case Type::Condition:
        return internalCondition.NJS;
    case Type::Level:
        return internalLevel.NJS;
    }
}

#define S_PROP(typ, name, cfgName, structName, levelRet, ...) \
void Preset::Set##name(typ value) { \
    switch(type) { \
    case Type::Main: \
        getModConfig().cfgName.SetValue(value); \
        break; \
    case Type::Condition: \
        internalCondition.structName = value; \
        UpdateCondition(); \
        break; \
    case Type::Level: \
        __VA_ARGS__; \
        break; \
    } \
} \
typ Preset::Get##name() { \
    switch(type) { \
    case Type::Main: \
        return getModConfig().cfgName.GetValue(); \
    case Type::Condition: \
        return internalCondition.structName; \
    case Type::Level: \
        return levelRet; \
    } \
}
#define PROP(typ, name, cfgName, structName) S_PROP(typ, name, cfgName, structName, internalLevel.structName, internalLevel.structName = value; UpdateLevelPreset())

PROP(bool, OverrideNJS, UseNJS, OverrideNJS);
PROP(bool, UseDuration, UseDuration, UseDuration);
S_PROP(bool, UseDefaults, AutoDef, SetToDefaults, false);
S_PROP(bool, UseBounds, BoundJD, DistanceBounds, false);
S_PROP(float, BoundMin, MinJD, DistanceMin, 0);
S_PROP(float, BoundMax, MaxJD, DistanceMax, 0);

void Preset::SetCondition(Condition value, int idx) {
    if(type == Type::Condition) {
        if(idx >= internalCondition.Conditions.size())
            internalCondition.Conditions.push_back(value);
        else
            internalCondition.Conditions[idx] = value;
        UpdateCondition();
    }
}

Condition Preset::GetCondition(int idx) {
    if(type == Type::Condition && idx < internalCondition.Conditions.size())
        return internalCondition.Conditions[idx];
    return {};
}

void Preset::RemoveCondition(int idx) {
    if(type == Type::Condition && idx < internalCondition.Conditions.size()) {
        internalCondition.Conditions.erase(internalCondition.Conditions.begin() + idx);
        UpdateCondition();
    }
}

int Preset::GetConditionCount() {
    if(type == Type::Condition)
        return internalCondition.Conditions.size();
    return 0;
}

int Preset::GetConditionPresetIndex() {
    if(type == Type::Condition)
        return internalIdx;
    return -1;
}

void Preset::SyncCondition(int idxChange) {
    if(type != Type::Condition)
        return;
    internalIdx += idxChange;
    internalCondition = getModConfig().Presets.GetValue()[internalIdx];
}

bool Preset::ShiftForward() {
    if(type != Type::Condition)
        return false;
    auto presets = getModConfig().Presets.GetValue();
    if(internalIdx == presets.size() - 1)
        return false;
    presets[internalIdx] = presets[internalIdx + 1];
    presets[internalIdx + 1] = internalCondition;
    internalIdx++;
    getModConfig().Presets.SetValue(presets);
    return true;
}

bool Preset::ShiftBackward() {
    if(type != Type::Condition || internalIdx == 0)
        return false;
    auto presets = getModConfig().Presets.GetValue();
    presets[internalIdx] = presets[internalIdx - 1];
    presets[internalIdx - 1] = internalCondition;
    internalIdx--;
    getModConfig().Presets.SetValue(presets);
    return true;
}

LevelPreset Preset::GetAsLevelPreset() {
    if(GetIsLevelPreset())
        return internalLevel;
    LevelPreset ret;
    ret.UseDuration = GetUseDuration();
    ret.MainValue = Bound(GetMainValue());
    ret.OverrideNJS = GetOverrideNJS();
    ret.NJS = GetNJS();
    return ret;
}

bool Preset::GetIsLevelPreset() {
    return type == Type::Level;
}

void Preset::UpdateLevel(Values const& levelValues) {
    levelNJS = levelValues.njs;
    if(!GetUseDefaults())
        return;
    if(GetUseDuration())
        SetMainValue(levelValues.GetJumpDuration());
    else
        SetMainValue(levelValues.GetJumpDistance());
    SetNJS(levelNJS);
}

Preset::Preset(std::string levelID, std::string mapID, Values const& levelValues) {
    type = Type::Level;
    internalLevelID = levelID;
    internalMapID = mapID;
    auto presets = getModConfig().Levels.GetValue()[levelID];
    auto maps = *presets.GetValue<StringKeyedMap<LevelPreset>>();
    internalLevel = maps[mapID];
    levelNJS = levelValues.njs;
}

Preset::Preset(int conditionIdx, Values const& levelValues, bool setToLevel) {
    type = Type::Condition;
    internalCondition = getModConfig().Presets.GetValue()[conditionIdx];
    internalIdx = conditionIdx;
    levelNJS = levelValues.njs;
    if(internalCondition.SetToDefaults && setToLevel) {
        if(internalCondition.UseDuration)
            internalCondition.Duration = levelValues.GetJumpDuration();
        else
            internalCondition.Distance = levelValues.GetJumpDistance();
        internalCondition.NJS = levelValues.njs;
        UpdateCondition();
    }
}

Preset::Preset(Values const& levelValues, bool setToLevel) {
    type = Type::Main;
    levelNJS = levelValues.njs;
    if(getModConfig().AutoDef.GetValue() && setToLevel) {
        if(getModConfig().UseDuration.GetValue())
            getModConfig().Duration.SetValue(levelValues.GetJumpDuration());
        else
            getModConfig().Distance.SetValue(levelValues.GetJumpDistance());
        getModConfig().NJS.SetValue(levelValues.njs);
    }
}

void UpdateScoreSubmission(bool overridingNJS) {
    if(overridingNJS && !getModConfig().Disable.GetValue())
        bs_utils::Submission::disable(getModInfo());
    else
        bs_utils::Submission::enable(getModInfo());
}
