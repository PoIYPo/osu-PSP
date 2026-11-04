#include "beatmap.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace osu {

const char* game_mode_name(GameMode mode) {
    switch (mode) {
        case GameMode::Standard: return "osu!standard";
        case GameMode::Taiko: return "osu!taiko";
        case GameMode::Catch: return "osu!catch";
        case GameMode::Mania: return "osu!mania";
    }
    return "unknown";
}

const char* object_kind_name(ObjectKind kind) {
    switch (kind) {
        case ObjectKind::Circle: return "circle";
        case ObjectKind::Slider: return "slider";
        case ObjectKind::Spinner: return "spinner";
        case ObjectKind::HoldNote: return "hold";
    }
    return "unknown";
}

CurveType curve_type_from_char(char c) {
    switch (c) {
        case 'B': return CurveType::Bezier;
        case 'C': return CurveType::Catmull;
        case 'P': return CurveType::PerfectCircle;
        default: return CurveType::Linear;
    }
}

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

bool parse_long(std::string_view s, long& out) {
    s = trim(s);
    if (s.empty()) return false;
    std::string buf(s);
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(buf.c_str(), &end, 10);
    if (errno != 0 || end == buf.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

bool parse_double(std::string_view s, double& out) {
    s = trim(s);
    if (s.empty()) return false;
    std::string buf(s);
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(buf.c_str(), &end);
    if (errno != 0 || end == buf.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

bool parse_float(std::string_view s, float& out) {
    double v = 0.0;
    if (!parse_double(s, v)) return false;
    out = static_cast<float>(v);
    return true;
}

bool parse_int(std::string_view s, int& out) {
    long v = 0;
    if (!parse_long(s, v)) return false;
    if (v < -2147483647L || v > 2147483647L) return false;
    out = static_cast<int>(v);
    return true;
}

// Splits a comma separated line, honouring double quoted fields (used by [Events]).
void split_fields(std::string_view line, std::vector<std::string_view>& out) {
    out.clear();
    size_t start = 0;
    while (start <= line.size()) {
        const size_t comma = line.find(',', start);
        const size_t end = comma == std::string_view::npos ? line.size() : comma;
        out.push_back(line.substr(start, end - start));
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
}

std::string_view unquote(std::string_view s) {
    s = trim(s);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s.remove_prefix(1);
        s.remove_suffix(1);
    }
    return s;
}

enum class Section {
    None,
    General,
    Editor,
    Metadata,
    Difficulty,
    Events,
    TimingPoints,
    Colours,
    HitObjects,
    Variables,
    Storyboard,
    Ignored,
};

Section section_from_name(std::string_view name) {
    if (name == "General") return Section::General;
    if (name == "Editor") return Section::Editor;
    if (name == "Metadata") return Section::Metadata;
    if (name == "Difficulty") return Section::Difficulty;
    if (name == "Events") return Section::Events;
    if (name == "TimingPoints") return Section::TimingPoints;
    if (name == "Colours") return Section::Colours;
    if (name == "HitObjects") return Section::HitObjects;
    if (name == "Variables") return Section::Variables;
    if (name == "Storyboard") return Section::Storyboard;
    return Section::Ignored;
}

bool parse_timing_point(std::string_view line, TimingPoint& tp) {
    std::vector<std::string_view> f;
    split_fields(line, f);
    if (f.size() < 2) return false;

    double time = 0.0, beat_length = 0.0;
    if (!parse_double(f[0], time)) return false;
    if (!parse_double(f[1], beat_length)) return false;
    if (beat_length == 0.0) return false;  // a zero beat length is undefined

    tp.time = time;
    tp.beat_length = beat_length;

    int v = 0;
    if (f.size() > 2 && parse_int(f[2], v)) tp.meter = v;
    if (f.size() > 3 && parse_int(f[3], v)) tp.sample_set = v;
    if (f.size() > 4 && parse_int(f[4], v)) tp.sample_index = v;
    if (f.size() > 5 && parse_int(f[5], v)) tp.volume = v;
    // Field 6 omitted means "uninherited" in the legacy formats; a negative beat
    // length always denotes an inherited point regardless of the flag.
    bool uninherited = true;
    if (f.size() > 6 && parse_int(f[6], v)) uninherited = v != 0;
    tp.uninherited = uninherited && beat_length > 0.0;
    if (f.size() > 7 && parse_int(f[7], v)) tp.effects = v;
    return true;
}

// Parses "B|120:45|200:100|300:100" into the curve type plus extra points.
bool parse_slider_curve(std::string_view spec, SliderData& slider) {
    spec = trim(spec);
    if (spec.empty()) return false;
    slider.curve = curve_type_from_char(spec.front());
    size_t pos = spec.find('|');
    if (pos == std::string_view::npos) return true;  // head only: degenerate path

    ++pos;
    while (pos <= spec.size()) {
        size_t next = spec.find('|', pos);
        if (next == std::string_view::npos) next = spec.size();
        std::string_view pair = spec.substr(pos, next - pos);
        const size_t colon = pair.find(':');
        if (colon != std::string_view::npos) {
            double x = 0.0, y = 0.0;
            if (parse_double(pair.substr(0, colon), x) && parse_double(pair.substr(colon + 1), y)) {
                slider.control_points.push_back(
                    {static_cast<float>(x), static_cast<float>(y)});
            }
        }
        if (next == spec.size()) break;
        pos = next + 1;
    }
    return true;
}

bool parse_hit_object(std::string_view line, HitObject& obj) {
    std::vector<std::string_view> f;
    split_fields(line, f);
    if (f.size() < 5) return false;

    int x = 0, y = 0, time = 0, type = 0, sound = 0;
    if (!parse_int(f[0], x)) return false;
    if (!parse_int(f[1], y)) return false;
    if (!parse_int(f[2], time)) return false;
    if (!parse_int(f[3], type)) return false;
    if (!parse_int(f[4], sound)) return false;

    obj.pos = {static_cast<float>(x), static_cast<float>(y)};
    obj.time = static_cast<double>(time);
    obj.end_time = obj.time;
    obj.raw_type = type;
    obj.sound = sound;
    obj.new_combo = (type & 4) != 0;
    obj.combo_offset = (type >> 4) & 7;

    const bool is_circle = (type & 1) != 0;
    const bool is_slider = (type & 2) != 0;
    const bool is_spinner = (type & 8) != 0;
    const bool is_hold = (type & 128) != 0;

    if (is_spinner) {
        obj.kind = ObjectKind::Spinner;
        if (f.size() > 5) {
            double end = 0.0;
            if (parse_double(f[5], end)) obj.end_time = end;
        }
        return true;
    }

    if (is_hold) {
        obj.kind = ObjectKind::HoldNote;
        if (f.size() > 5) {
            std::string_view v = f[5];
            const size_t colon = v.find(':');
            if (colon != std::string_view::npos) v = v.substr(0, colon);
            double end = 0.0;
            if (parse_double(v, end)) obj.end_time = end;
        }
        return true;
    }

    if (is_slider && f.size() >= 8) {
        obj.kind = ObjectKind::Slider;
        if (!parse_slider_curve(f[5], obj.slider)) return false;

        // Prepend the head: every path starts at the object's position.
        std::vector<Vec2> points;
        points.reserve(obj.slider.control_points.size() + 1);
        points.push_back(obj.pos);
        for (const Vec2& p : obj.slider.control_points) points.push_back(p);
        obj.slider.control_points.swap(points);

        int repeats = 1;
        if (parse_int(f[6], repeats) && repeats > 0) obj.slider.repeats = repeats;
        float len = 0.0f;
        if (parse_float(f[7], len) && len > 0.0f) obj.slider.pixel_length = len;
        return true;
    }

    // Circles, and anything we cannot interpret (e.g. pre-v5 sliders which have
    // no curve data), degrade to a circle rather than failing the whole map.
    (void)is_circle;
    obj.kind = ObjectKind::Circle;
    obj.slider = SliderData{};
    return true;
}

bool parse_event_line(std::string_view line, Beatmap& bm) {
    std::vector<std::string_view> f;
    split_fields(line, f);
    if (f.size() < 3) return false;
    int type = 0;
    if (!parse_int(f[0], type)) return false;
    if (type != 0) return false;  // 0 = background; video/breaks/colours are ignored
    std::string_view name = unquote(f[2]);
    if (name.empty()) return false;
    if (bm.background_file.empty()) bm.background_file.assign(name.data(), name.size());
    return true;
}

bool parse_key_value(Section section, std::string_view key, std::string_view value, Beatmap& bm) {
    const bool is_general = section == Section::General;
    const bool is_meta = section == Section::Metadata;
    const bool is_diff = section == Section::Difficulty;

    if (is_general) {
        if (key == "AudioFilename") { bm.audio_filename.assign(value.data(), value.size()); return true; }
        if (key == "AudioLeadIn") { int v = 0; if (parse_int(value, v)) bm.audio_lead_in = v; return true; }
        if (key == "PreviewTime") { int v = 0; if (parse_int(value, v)) bm.preview_time = v; return true; }
        if (key == "Countdown") { int v = 0; if (parse_int(value, v)) bm.countdown = v; return true; }
        if (key == "SampleSet") { bm.sample_set.assign(value.data(), value.size()); return true; }
        if (key == "StackLeniency") { float v = 0.0f; if (parse_float(value, v)) bm.stack_leniency = v; return true; }
        if (key == "LetterboxInBreaks") { int v = 0; if (parse_int(value, v)) bm.letterbox_in_breaks = v != 0; return true; }
        if (key == "WidescreenStoryboard") { int v = 0; if (parse_int(value, v)) bm.widescreen_storyboard = v != 0; return true; }
        if (key == "Mode") {
            int v = 0;
            if (parse_int(value, v) && v >= 0 && v <= 3) bm.mode = static_cast<GameMode>(v);
            return true;
        }
        return false;
    }

    if (is_meta) {
        std::string* dst = nullptr;
        if (key == "Title") dst = &bm.title;
        else if (key == "TitleUnicode") dst = &bm.title_unicode;
        else if (key == "Artist") dst = &bm.artist;
        else if (key == "ArtistUnicode") dst = &bm.artist_unicode;
        else if (key == "Creator") dst = &bm.creator;
        else if (key == "Version") dst = &bm.version;
        else if (key == "Source") dst = &bm.source;
        else if (key == "Tags") dst = &bm.tags;
        else if (key == "BeatmapID") dst = &bm.beatmap_id;
        else if (key == "BeatmapSetID") dst = &bm.beatmapset_id;
        if (dst) { dst->assign(value.data(), value.size()); return true; }
        return false;
    }

    if (is_diff) {
        float v = 0.0f;
        if (key == "HPDrainRate") { if (parse_float(value, v)) bm.hp_drain = v; return true; }
        if (key == "CircleSize") { if (parse_float(value, v)) bm.circle_size = v; return true; }
        if (key == "OverallDifficulty") { if (parse_float(value, v)) bm.overall_difficulty = v; return true; }
        if (key == "ApproachRate") { if (parse_float(value, v)) bm.approach_rate = v; return true; }
        if (key == "SliderMultiplier") { if (parse_float(value, v)) bm.slider_multiplier = v; return true; }
        if (key == "SliderTickRate") { if (parse_float(value, v)) bm.slider_tick_rate = v; return true; }
        return false;
    }

    return false;
}

}  // namespace

bool parse_osu(std::string_view text, Beatmap& out, std::string* error) {
    out = Beatmap{};

    // Strip a UTF-8 BOM: many beatmaps were re-saved on Windows with one.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    auto fail = [&](int line_no, const char* msg) {
        if (error) {
            *error = "line " + std::to_string(line_no) + ": " + msg;
        }
        return false;
    };

    Section section = Section::None;
    int line_no = 0;
    size_t pos = 0;

    while (pos <= text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) eol = text.size();
        std::string_view line = text.substr(pos, eol - pos);
        pos = eol + 1;
        ++line_no;

        // Trailing '\r' (CRLF files) is handled by trim()/trailing check below.
        line = trim(line);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;

        if (line_no == 1 && line.size() > 17 && line.substr(0, 17) == "osu file format v") {
            int v = 0;
            if (parse_int(line.substr(17), v)) out.format_version = v;
            continue;
        }
        if (line.size() > 2 && line.front() == '/' && line[1] == '/') continue;

        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close == std::string_view::npos) continue;
            std::string_view name = trim(line.substr(1, close - 1));
            section = section_from_name(name);
            continue;
        }

        switch (section) {
            case Section::TimingPoints: {
                TimingPoint tp;
                if (!parse_timing_point(line, tp)) return fail(line_no, "bad timing point");
                out.timing_points.push_back(tp);
                break;
            }
            case Section::HitObjects: {
                HitObject obj;
                if (!parse_hit_object(line, obj)) return fail(line_no, "bad hit object");
                out.objects.push_back(std::move(obj));
                break;
            }
            case Section::Events:
                parse_event_line(line, out);
                break;
            case Section::General:
            case Section::Metadata:
            case Section::Difficulty: {
                const size_t colon = line.find(':');
                if (colon == std::string_view::npos) break;
                std::string_view key = trim(line.substr(0, colon));
                std::string_view value = trim(line.substr(colon + 1));
                parse_key_value(section, key, value, out);
                break;
            }
            default:
                break;  // Editor, Colours, Variables, Storyboard, unknown
        }
    }

    // A map with no timing point at all cannot be played; treat it as an error so
    // that broken downloads surface immediately instead of at play time.
    if (out.timing_points.empty() && !out.objects.empty()) {
        if (error) *error = "no timing points";
        return false;
    }

    // Files are supposed to be sorted already; a stable sort keeps equal times in
    // file order, which is what the game logic relies on.
    std::stable_sort(out.timing_points.begin(), out.timing_points.end(),
                     [](const TimingPoint& a, const TimingPoint& b) {
                         if (a.time != b.time) return a.time < b.time;
                         // At identical times the uninherited point must apply first,
                         // then the inherited one modifies its velocity.
                         return a.uninherited && !b.uninherited;
                     });
    std::stable_sort(out.objects.begin(), out.objects.end(),
                     [](const HitObject& a, const HitObject& b) { return a.time < b.time; });

    return true;
}

bool load_osu_file(const char* path, Beatmap& out, std::string* error, size_t max_bytes) {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
        if (error) *error = std::string("cannot open: ") + path;
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size < 0 || static_cast<size_t>(size) > max_bytes) {
        std::fclose(file);
        if (error) *error = "file too large";
        return false;
    }
    std::string text(static_cast<size_t>(size), '\0');
    const size_t got = text.empty() ? 0 : std::fread(text.data(), 1, text.size(), file);
    std::fclose(file);
    if (got != text.size()) {
        if (error) *error = "short read";
        return false;
    }
    return parse_osu(text, out, error);
}

}  // namespace osu
