// CerberusLeaderVoice.asi - Manhunt 1 PC (Steam). Lets the Cerberus leader (mansion_int's CHIEF,
// voice Y1V1) use his own recorded lines in more situations.
//
// How the game picks a hunter's speech sample:
//   - A hunter's voice id is at ped+0x634 (0x44B830). VOICE CERBERUS_LEADER in entityTypeData.ini
//     (pool 0x17) gives voice 0x29 = Y1V1.
//   - A static table in .data at 0x6AF966 maps (voice, speech type) to a range of sfx.lst entries:
//     49 voices x 59 speech types (0x25..0x5F), 4 bytes per cell = uint16 first, uint16 last.
//     0x3713 as "first" means the voice has nothing for that type.
//   - The picker (0x447430) and the player (0x44D053) both return "no speech" for an empty cell.
//     There is no fallback to another type or another voice.
//   - Y1V1 was only recorded for three types: DEFINITE_SIGHTING (6 lines), IDLE_MUMBLING (10) and
//     SHOUT_FOR_ASSISTANCE (6). Every other type the AI asks him for (taunts, combat waits, join
//     attack, search and investigate barks...) plays nothing, which is why he is so rarely heard.
//
// The vanilla table already reuses lines across types when a voice is missing a recording: every
// Cerberus voice (C1V1, C1V2, C2V1) plays its DEFINITE_SIGHTING lines for JOIN_ATTACK, and
// Ramirez plays POSITIVE_TAUNT_COMBAT for NEGATIVE_TAUNT_COMBAT. Y1V1 just never got that.
//
// Fix: at startup, fill empty Y1V1 cells with one of his three recorded ranges, as chosen in
// CerberusLeaderVoice.ini [Map]. Nothing else in the game is touched. The table is checked first
// and nothing is written if it doesn't match this exe.

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>

namespace
{
    constexpr uintptr_t EXPECTED_IMAGE_BASE = 0x400000;
    constexpr uintptr_t TABLE = 0x6AF966;      // uint16 first, uint16 last per cell
    constexpr int TYPES_PER_VOICE = 59;        // speech types 0x25..0x5F
    constexpr int FIRST_TYPE = 0x25;
    constexpr int VOICE_COUNT = 0x31;
    constexpr uint16_t NONE = 0x3713;

    constexpr int VOICE_C1V1 = 0x25;
    constexpr int VOICE_Y1V1 = 0x29;           // Cerb Leader 1 Voice 1

    struct Range { uint16_t first, last; };
    // Y1V1's recorded ranges (sfx.lst entry numbers).
    constexpr Range Y1V1_CFA = { 6830, 6835 }; // SFX_Y1V1_CALL_FOR_ASSISTANCE_1..6  (Y1V1CFA1-6)
    constexpr Range Y1V1_DS  = { 6836, 6841 }; // SFX_Y1V1_DEFINITE_SIGHTING_1..6    (Y1V1DS1-6)
    constexpr Range Y1V1_GIC = { 6842, 6851 }; // SFX_Y1V1_IDLE_MUMBLING_1..10       (Y1V1GIC1-10)

    // Speech types that can be set from the ini. Names follow the exe's own debug names (string
    // table at 0x6B2FA0); each was checked against the sfx.lst names the table points to for
    // voices that have that type (P1V1, C1V1). Types from 0x46 on (pain, death, grunts, specific
    // speech) are left alone.
    struct TypeName { int type; const char* name; };
    constexpr TypeName TYPE_NAMES[] = {
        { 0x25, "NEGATIVE_SEARCH_RESULT" },
        { 0x26, "DEFINITE_SIGHTING" },
        { 0x27, "PROBABLE_SIGHTING" },
        { 0x28, "PROBABLE_HEARING" },
        { 0x29, "RUN_TO_INVESTIGATE" },
        { 0x2A, "WALK_TO_INVESTIGATE" },
        { 0x2B, "STOP_AND_LISTEN" },
        { 0x2C, "CURIOSITY_NO_RESULT" },
        { 0x2D, "TAUNT_SEARCH" },
        { 0x2E, "TAUNT_COMBAT_POSITIVE" },
        { 0x2F, "TAUNT_COMBAT_NEGATIVE" },
        { 0x30, "TAUNT_COMBAT" },
        { 0x31, "TAUNT_CHASE" },
        { 0x32, "TAUNT_SHORT" },
        { 0x33, "TAUNT_SAFEZONE" },
        { 0x34, "TAUNT_BOUNDARY" },
        { 0x35, "TAUNT_PLAYER_DEAD" },
        { 0x36, "JOIN_ATTACK" },
        { 0x37, "WAIT_ENEMY_ALONE" },
        { 0x38, "WAIT_ENEMY_MULTIPLE" },
        { 0x39, "SNEAK_INVESTIGATE" },
        { 0x3A, "WAIT_IN_COVER" },
        { 0x3B, "SURPRISE" },
        { 0x3C, "GREETINGS" },
        { 0x3D, "SEEN_RUNNING_INTO_SAFEZONE" },
        { 0x3E, "CLAIM_TERRITORY" },
        { 0x3F, "IDLE_MUMBLING" },
        { 0x40, "WHISTLING" },
        { 0x41, "CHATTING" },
        { 0x42, "SEARCH_CHAT" },
        { 0x43, "INVESTIGATE_CHAT" },
        { 0x44, "SHOUT_FOR_HELP" },
        { 0x45, "SHOUT_FOR_ASSISTANCE" },
    };

    // Used when the .ini is missing. Same as the shipped CerberusLeaderVoice.ini.
    struct Default { const char* name; const char* value; };
    constexpr Default DEFAULTS[] = {
        { "JOIN_ATTACK",         "DS"  },  // what every other Cerberus voice does
        { "SHOUT_FOR_HELP",      "CFA" },
        { "TAUNT_CHASE",         "DS"  },
        { "TAUNT_SHORT",         "DS"  },
        { "WAIT_ENEMY_ALONE",    "DS"  },
        { "WAIT_ENEMY_MULTIPLE", "CFA" },  // "Let's finish this prick right here, boys!"
        { "WAIT_IN_COVER",       "DS"  },
        { "TAUNT_COMBAT",          "DS"  },
        { "TAUNT_COMBAT_POSITIVE", "DS"  },
        { "TAUNT_COMBAT_NEGATIVE", "DS"  },
        { "SURPRISE",              "DS"  },
        { "TAUNT_SEARCH",          "GIC" },  // searching: mumbling, not sighting lines
        { "NEGATIVE_SEARCH_RESULT","GIC" },
        { "CURIOSITY_NO_RESULT",   "GIC" },
        { "STOP_AND_LISTEN",       "GIC" },
        { "RUN_TO_INVESTIGATE",    "GIC" },
        { "WALK_TO_INVESTIGATE",   "GIC" },
        { "SNEAK_INVESTIGATE",     "GIC" },
        { "CHATTING",              "GIC" },  // chat with the bodyguards
        { "SEARCH_CHAT",           "GIC" },
        { "INVESTIGATE_CHAT",      "GIC" },
        { "GREETINGS",             "GIC" },
    };

    bool g_log = false;
    char g_ini[MAX_PATH] = {};
    bool g_iniFound = false;

    void Log(bool always, const char* fmt, ...)
    {
        if (!g_log && !always) return;
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (char* slash = strrchr(path, '\\'))
            strcpy_s(slash + 1, MAX_PATH - (slash + 1 - path), "cerberusleadervoice_asi.log");
        FILE* f = nullptr;
        if (fopen_s(&f, path, "a") != 0 || !f) return;
        va_list args; va_start(args, fmt); vfprintf(f, fmt, args); va_end(args);
        fputc('\n', f); fclose(f);
    }

    Range* Cell(int voice, int type)
    {
        return reinterpret_cast<Range*>(TABLE + 4 * (voice * TYPES_PER_VOICE + (type - FIRST_TYPE)));
    }

    bool Readable(uintptr_t addr, size_t len)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        return addr + len <= reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }

    bool Same(const Range* c, Range r) { return c->first == r.first && c->last == r.last; }

    // Checks cells whose vanilla values are known, so a different exe is never written to.
    bool TableLooksRight()
    {
        const uintptr_t start = reinterpret_cast<uintptr_t>(Cell(0, FIRST_TYPE));
        const uintptr_t end = reinterpret_cast<uintptr_t>(Cell(VOICE_COUNT - 1, FIRST_TYPE + TYPES_PER_VOICE - 1)) + 4;
        if (!Readable(start, end - start)) return false;
        return Same(Cell(VOICE_Y1V1, 0x45), Y1V1_CFA)
            && Same(Cell(VOICE_Y1V1, 0x26), Y1V1_DS)
            && Same(Cell(VOICE_Y1V1, 0x3F), Y1V1_GIC)
            && Cell(VOICE_Y1V1, 0x36)->first == NONE
            && Same(Cell(VOICE_C1V1, 0x26), Range{ 776, 781 })    // C1V1 DEFINITE_SIGHTING
            && Same(Cell(VOICE_C1V1, 0x36), Range{ 776, 781 });   // C1V1 JOIN_ATTACK = its DS
    }

    // Accepts a whole set ("DS"), one line ("DS3" = Y1V1DS3.WAV) or consecutive lines ("CFA4-6"),
    // or "OFF". Line n of a set is sfx entry first + n - 1, in the same order as the file numbers.
    bool ParseValue(const char* v, Range& out, bool& off)
    {
        off = false;
        if (!_stricmp(v, "OFF")) { off = true; out = { NONE, NONE }; return true; }

        struct Set { const char* name; Range r; };
        static const Set sets[] = { { "CFA", Y1V1_CFA }, { "DS", Y1V1_DS }, { "GIC", Y1V1_GIC } };
        for (const Set& s : sets) {
            const size_t n = strlen(s.name);
            if (_strnicmp(v, s.name, n)) continue;
            const char* rest = v + n;
            if (!*rest) { out = s.r; return true; }
            if (strspn(rest, "0123456789-") != strlen(rest)) return false;
            const int count = s.r.last - s.r.first + 1;
            int a = 0, b = 0; char extra = 0;
            const int got = sscanf_s(rest, "%d-%d%c", &a, &b, &extra, 1);
            if (got == 1) b = a;
            else if (got != 2) return false;
            if (a < 1 || b < a || b > count) return false;
            out = { static_cast<uint16_t>(s.r.first + a - 1), static_cast<uint16_t>(s.r.first + b - 1) };
            return true;
        }
        return false;
    }

    const char* DefaultFor(const char* name)
    {
        for (const Default& d : DEFAULTS)
            if (!strcmp(d.name, name)) return d.value;
        return "";
    }

    void Apply()
    {
        if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != EXPECTED_IMAGE_BASE) {
            Log(true, "ABORT: image base is not 0x400000, nothing changed");
            return;
        }
        if (!TableLooksRight()) {
            Log(true, "ABORT: speech table at 0x6AF966 doesn't match this exe's expected values, nothing changed");
            return;
        }

        const uintptr_t start = reinterpret_cast<uintptr_t>(Cell(VOICE_Y1V1, FIRST_TYPE));
        const size_t len = 4 * TYPES_PER_VOICE;
        DWORD oldProt = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(start), len, PAGE_READWRITE, &oldProt)) {
            Log(true, "ABORT: VirtualProtect failed (%lu), nothing changed", GetLastError());
            return;
        }

        int changed = 0;
        for (const TypeName& t : TYPE_NAMES) {
            char v[32] = {};
            if (g_iniFound)
                GetPrivateProfileStringA("Map", t.name, "", v, sizeof(v), g_ini);
            else
                strcpy_s(v, DefaultFor(t.name));
            // trim spaces and trailing comments
            if (char* sc = strchr(v, ';')) *sc = 0;
            char* p = v; while (*p == ' ' || *p == '\t') ++p;
            char* e = p + strlen(p); while (e > p && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            if (!*p) continue;   // blank = leave vanilla

            Range r{}; bool off = false;
            if (!ParseValue(p, r, off)) {
                Log(true, "ignored %s=%s (use DS, CFA, GIC, one line like DS3, lines like CFA4-6, or OFF)", t.name, p);
                continue;
            }
            Range* c = Cell(VOICE_Y1V1, t.type);
            const Range before = *c;
            *c = r;
            ++changed;
            Log(false, "Y1V1 %-22s (0x%02X): %u-%u -> %s (%u-%u)", t.name, t.type,
                before.first, before.last, p, r.first, r.last);
        }

        VirtualProtect(reinterpret_cast<void*>(start), len, oldProt, &oldProt);
        Log(false, "patched: %d speech types set for the Cerberus leader (Y1V1)", changed);
    }

    void Init(HMODULE self)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(self, path, MAX_PATH);
        if (char* dot = strrchr(path, '.')) strcpy_s(dot, MAX_PATH - (dot - path), ".ini");
        strcpy_s(g_ini, path);
        g_iniFound = GetFileAttributesA(g_ini) != INVALID_FILE_ATTRIBUTES;
        g_log = g_iniFound && GetPrivateProfileIntA("General", "Log", 0, g_ini) != 0;
        Log(false, "CerberusLeaderVoice.asi loaded (%s)", g_iniFound ? g_ini : "no .ini, using defaults");
        Apply();
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        Init(module);
    }
    return TRUE;
}
