// Beatmap model: a 1:1 (in-memory) representation of the parts of an .osu file
// that matter for gameplay. File parsing lives in beatmap.cpp.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace osu {

enum class GameMode : int {
    Standard = 0,
    Taiko = 1,
    Catch = 2,
    Mania = 3,
};

const char* game_mode_name(GameMode mode);

// A single entry of [TimingPoints].
//
// `beat_length` is positive for uninherited (BPM) points -- milliseconds per beat.
// It is negative for inherited points, where the slider velocity multiplier is
// encoded as -100 / beat_length (so -50 means "2x slider speed").
struct TimingPoint {
    double time = 0.0;
    double beat_length = 0.0;
    int meter = 4;
    int sample_set = 0;
    int sample_index = 0;
    int volume = 100;
    bool uninherited = true;
    int effects = 0;

    double slider_velocity_multiplier() const {
        return uninherited ? 1.0 : -100.0 / beat_length;
    }
};

enum class CurveType : uint8_t {
    Bezier = 0,          // 'B'
    Catmull = 1,         // 'C' (legacy)
    Linear = 2,          // 'L'
    PerfectCircle = 3,   // 'P'
};

CurveType curve_type_from_char(char c);

// A slider's raw definition straight out of the file. The actual playable
// geometry (spans, ticks, sampled polyline) is produced by standard.cpp.
struct SliderData {
    CurveType curve = CurveType::Linear;
    // Control points as written in the file, head first. The head is also the
    // object position; extra points were authored in the editor.
    std::vector<Vec2> control_points;
    float pixel_length = 0.0f;   // declared length of ONE span, in osu pixels
    int repeats = 1;             // total spans; 1 = no repeat
};

enum class ObjectKind : uint8_t {
    Circle,
    Slider,
    Spinner,
    HoldNote,   // osu!mania only; parsed for completeness
};

const char* object_kind_name(ObjectKind kind);

struct HitObject {
    ObjectKind kind = ObjectKind::Circle;
    Vec2 pos;                  // playfield coordinates (0..512, 0..384)
    double time = 0.0;         // start time, ms from audio start
    double end_time = 0.0;     // == time for circles; set for sliders/spinners/holds
    int raw_type = 0;          // original bitmask, useful for debugging
    int sound = 0;             // hit sound bitmask (normal/whistle/finish/clap)
    bool new_combo = false;
    int combo_offset = 0;      // "combo colour skip" encoded in the type bits
    SliderData slider;         // only meaningful when kind == Slider

    bool is_circle() const { return kind == ObjectKind::Circle; }
    bool is_slider() const { return kind == ObjectKind::Slider; }
    bool is_spinner() const { return kind == ObjectKind::Spinner; }
};

struct Beatmap {
    // [General]
    std::string audio_filename;
    int audio_lead_in = 0;
    int preview_time = -1;
    int countdown = 0;
    std::string sample_set = "Normal";
    float stack_leniency = 0.7f;
    GameMode mode = GameMode::Standard;
    bool letterbox_in_breaks = false;
    bool widescreen_storyboard = false;

    // [Metadata]
    std::string title;
    std::string title_unicode;
    std::string artist;
    std::string artist_unicode;
    std::string creator;
    std::string version;       // difficulty name
    std::string source;
    std::string tags;
    std::string beatmap_id;
    std::string beatmapset_id;

    // [Difficulty] -- ApproachRate falls back to OverallDifficulty when absent,
    // which is what both stable and lazer do.
    float hp_drain = 5.0f;
    float circle_size = 5.0f;
    float overall_difficulty = 5.0f;
    float approach_rate = -1.0f;
    float slider_multiplier = 1.4f;
    float slider_tick_rate = 1.0f;

    // [Events]
    std::string background_file;

    // [TimingPoints] / [HitObjects]
    std::vector<TimingPoint> timing_points;
    std::vector<HitObject> objects;

    int format_version = 0;

    float effective_approach_rate() const {
        return approach_rate < 0.0f ? overall_difficulty : approach_rate;
    }

    double first_object_time() const {
        return objects.empty() ? 0.0 : objects.front().time;
    }

    double last_object_time() const {
        double t = 0.0;
        for (const HitObject& o : objects) t = std::max(t, o.end_time);
        return t;
    }
};

// Optional debug hook: when set, the parser reports what it sees. Used by the PSP
// build to debug parsing on the device without a debugger.
extern void (*osu_debug_sink)(const char* message);

// Parses an .osu document from memory.
//
// Returns false on structural errors (unknown object type, malformed timing point,
// truncated slider definition) and fills `error` with "line N: message".
// Unknown sections and keys are skipped on purpose so that newer osu! versions
// keep loading. `text` may contain a UTF-8 BOM and CRLF line endings.
bool parse_osu(std::string_view text, Beatmap& out, std::string* error);

// Reads and parses a file. Rejects files larger than `max_bytes` (default 8 MiB)
// because on PSP a hostile/broken file must not exhaust the 32 MiB of RAM.
bool load_osu_file(const char* path, Beatmap& out, std::string* error,
                   size_t max_bytes = 8u * 1024u * 1024u);

}  // namespace osu
