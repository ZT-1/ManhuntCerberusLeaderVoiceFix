# Manhunt (2003, PC/Steam): Cerberus leader voice fix

The Cerberus leader (the `CHIEF` in **mansion_int**, model record `Cer_Leader`) has
his own voice set, `Y1V1`, but you almost never hear it. There are two causes:

1. **Vanilla gives him the wrong voice pool.** His `entityTypeData.ini` record says
   `VOICE CERBERUS`, the bodyguards' pool, instead of `VOICE CERBERUS_LEADER`.
2. **Even with the right pool, the game only knows three kinds of line for him.**
   So he's still rare once the pool is fixed.
   The game's speech table (in the exe) gives Y1V1 sound files for only 3 of the
   roughly 30 speech types other hunters use. For every other type the game plays
   nothing. An ASI plugin in this mod fills those gaps with his own lines.

## Cause 1: the voice pool (data)

- `VOICE` names become pool ids at `0x43E9F6` (record `+0xF8`): `CERBERUS` 0x13,
  `CERBERUS_LEADER` 0x17.
- With no scripted voice, `0x44B830` picks one (switch at `0x44B880`, jump table
  `0x671C20`, indexed by pool - 1) and stores it at ped `+0x634`:
  - `CERBERUS` → `0x25 + skin % 3` = C1V1 / C1V2 / C2V1
  - `CERBERUS_LEADER` → always `0x29` = **Y1V1** (case at `0x44B9DE`)
- Vanilla `Cer_Leader` has no `SKIN_TEX` lines, so his skin index is always 0 and the
  vanilla leader always speaks **C1V1**, the same voice as a bodyguard on skin A.
  `mansion_int.mls` has no `AISetEntityVoiceID` or skin pin for him.
- The 4 scripted Y1V1 lines (`SFX_Y1V1_SCRIPTED_MANSION_INT_1-4`, `Y1V1MAN1-4.WAV`,
  "CHIEF talking") are played by the level script with `LoadScriptAudioSlot` /
  `PlayScriptAudioSlotOneShotFromPos`, so they play whatever his voice pool is.
  They're separate from the `Y1V1` folder's 22 files, which only the AI plays.

## Cause 2: the speech table (exe)

The game picks a bark from a static table in `.data` at **`0x6AF966`**: 49 voices ×
59 speech types (0x25 to 0x5F), 4 bytes per cell (`uint16 first, uint16 last`, an
inclusive range of `sfx.lst` entries). `0x3713` means the voice has nothing for that
type. Both the sample picker (`0x447430`) and the player (`0x44D053`) return "no
speech" for an empty cell. They don't fall back to another type or another voice.

Y1V1's row (voice 0x29) has only these bark cells:

| speech type | sfx.lst entries | files |
|---|---|---|
| `DEFINITE_SIGHTING` (0x26) | 6836-6841 | `Y1V1DS1-6` |
| `IDLE_MUMBLING` (0x3F) | 6842-6851 | `Y1V1GIC1-10` |
| `SHOUT_FOR_ASSISTANCE` (0x45) | 6830-6835 | `Y1V1CFA1-6` |

The rest are pain, death and grunt sounds (the generic Cerberus ones; `PAIN_LONG`
points at C2V1's lines) and `SILENCE`. Compare a bodyguard's C1V1 row, which has
25 bark types: taunts, chase, search, investigate, waits, chat, join attack, and so
on. `sfx.lst` and `aud\V1\VOCALS\Y1V1\` hold exactly those 22 bark files, so
nothing recorded is missing from the table. The lines were simply never recorded.

So in a fight the AI keeps asking him for `TAUNT_SHORT`, `TAUNT_CHASE`,
`JOIN_ATTACK`, `WAIT_IN_COVER` and the like, and gets silence. You only hear him
on the rare sighting, call for assistance or mumble.

The vanilla table already covers missing recordings for other voices: every Cerberus
voice (C1V1, C1V2, C2V1) plays its `DEFINITE_SIGHTING` lines for `JOIN_ATTACK`, C1V2
uses C1V1's `PAIN_LONG`, and Ramirez plays `POSITIVE_TAUNT_COMBAT` for
`NEGATIVE_TAUNT_COMBAT`. Y1V1 just never got this treatment.

## The fix

### `CerberusLeaderVoice.asi` (needed by everyone)

At game start it writes his own recorded ranges into empty cells of his row, as set
in `CerberusLeaderVoice.ini`. Only the Y1V1 row changes; every other voice and
level is untouched. It checks the table against known vanilla values first and does
nothing (and logs `ABORT`) if they don't match. `manhunt.exe` is never modified.

Default mapping:

| speech type | gets | why |
|---|---|---|
| `JOIN_ATTACK` | DS (sighting lines) | what every other Cerberus voice does |
| `SHOUT_FOR_HELP` | CFA (call for assistance) | same meaning |
| `TAUNT_CHASE`, `TAUNT_SHORT`, `TAUNT_COMBAT`, `TAUNT_COMBAT_POSITIVE`, `TAUNT_COMBAT_NEGATIVE`, `SURPRISE` | DS | combat barks |
| `WAIT_ENEMY_ALONE`, `WAIT_IN_COVER` | DS | combat barks |
| `WAIT_ENEMY_MULTIPLE` | CFA | several hunters closing in: "Let's finish this prick right here, boys!" |
| `TAUNT_SEARCH`, `NEGATIVE_SEARCH_RESULT`, `CURIOSITY_NO_RESULT`, `STOP_AND_LISTEN`, `RUN_TO_INVESTIGATE`, `WALK_TO_INVESTIGATE`, `SNEAK_INVESTIGATE` | GIC (idle mumbling) | searching: sighting lines would give away that he's found you |
| `CHATTING`, `SEARCH_CHAT`, `INVESTIGATE_CHAT`, `GREETINGS` | GIC | talking with the bodyguards |

That's 22 types, about as many as a bodyguard's C1V1. The mapping is a judgement
call. Everything reuses his 22 recorded lines, and the picker cycles through each
range, least-used first, so expect repeats. Listen to `Y1V1DS1-6` and `Y1V1GIC1-10`,
and turn any row `OFF` in the `.ini` if a line sounds wrong there.

A row can also use part of a set: `DS3` is just `Y1V1DS3.WAV`, and `CFA4-6` is
`Y1V1CFA4` to `CFA6`. Only consecutive lines work, because the game stores each
cell as a first..last range. A value it can't read is logged as `ignored` and that
type is left as vanilla. The `.ini` has `TAUNT_SEARCH=DS3` ("I'm a born hunter. You
can't hide from me.") ready to try, commented out. **Tried in game (2026-11-24):** the
log showed `TAUNT_SEARCH -> DS3 (6838-6838)` and DS3 played after a couple of shadow
searches. The muttering lines suited searching better, so the default stays `GIC`.
That also confirms single-line values work in game. A few rare types
(player dead, safe zone, boundary, probable sighting) are still listed, commented
out, and can be set to `DS`, `CFA` or `GIC` the same way.

### `entityTypeData.ini` with `VOICE CERBERUS_LEADER`

In mansion_int's `entityTypeData.ini`, record `Cer_Leader`, change
`VOICE CERBERUS` to `VOICE CERBERUS_LEADER`. That's the only change. Without it, the
vanilla leader uses the C1V1 voice and the ASI does nothing for him.

The ini is game data, so it isn't in the GitHub repo, which has source only. Edit
your own copy through modloader: put the edited file at
`modloader\Cerberus Leader Voice Fix\levels\mansion_int\entityTypeData.ini`.

**If another mod already overrides mansion_int's `entityTypeData.ini`**, check its
`Cer_Leader` record. If it already says `VOICE CERBERUS_LEADER`, skip this step.
If not, make the change in that mod's copy instead. Two mods overriding one file
can't both win.

### What his lines say (transcribed by ear, 2026-11-24)

| file | line |
|---|---|
| `Y1V1CFA1` | Back me up! Move it, you faggots! |
| `Y1V1CFA2` | Cash is here! Where are you, you limp-dicked assholes? |
| `Y1V1CFA3` | Move in on my position. I got the sorry punk. |
| `Y1V1CFA4` | Let's finish this prick right here, boys! |
| `Y1V1CFA5` | We got him now! |
| `Y1V1CFA6` | He's over here! Get him, boys! |
| `Y1V1DS1` | Hey Cash, I got you in my killer gaze! |
| `Y1V1DS2` | I got you now, you dumb fuck! |
| `Y1V1DS3` | I'm a born hunter. You can't hide from me. |
| `Y1V1DS4` | I see you, Cash. I see you. |
| `Y1V1DS5` | There you are! |
| `Y1V1DS6` | There he is! Get him, you fools! |
| `Y1V1GIC1` | Nothing more dangerous than a lunatic with nothing to lose. |
| `Y1V1GIC2` | Damn! This is all outta control! |
| `Y1V1GIC3` | Starkweather's fucked this up. Looks like it's time to bail. |
| `Y1V1GIC4` | As much of a hindrance as a help. |
| `Y1V1GIC5` | If it wasn't for that psycho prick, everything would be going to plan! |
| `Y1V1GIC6` | This stinks of a setup. |
| `Y1V1GIC7` | I should've known. None of these guys are professionals, we've all been set up. |
| `Y1V1GIC8` | If I get out of this, I'm gonna crucify Starkweather myself! |
| `Y1V1GIC9` | Starkweather has lost it! And Ramirez never had it! Shit. |
| `Y1V1GIC10` | I'll just bag Cash, then I'll disappear. Leave this mess for those assholes. |

So DS is all "I see you / got you", CFA is all "get over here, boys", and GIC is
him muttering to himself about Starkweather and the setup. The mapping above fits:
DS for when he can see you, GIC for when he can't, CFA for calling his men.

## Requirements

- **ASI part:** an ASI loader (e.g. Ultimate ASI Loader).
- **ini part** (unless another mod already sets `VOICE CERBERUS_LEADER`):
  - modloader
  - **[PAKPatch](https://github.com/ermaccer/Manhunt.PAKPatch)** (`scripts\PAKPatch.asi`),
    with `ManHunt.PAK` extracted into the game folder. PAKPatch makes the game read
    the loose files, so the level inis (this one and every other) come from the
    game's `levels\` folder instead of the PAK.
  - The vanilla `levels\mansion_int\entityTypeData.ini` in the game folder (see
    step 2 below).

## Install

The repo is source only, with no prebuilt binary. Build it first (see Build below).

1. Copy the built `bin\CerberusLeaderVoice.asi` and `bin\CerberusLeaderVoice.ini`
   into the game's `scripts\` folder (needs an ASI loader, e.g. Ultimate ASI Loader).
2. Unless another mod already sets `VOICE CERBERUS_LEADER` (see above): put the edited
   `entityTypeData.ini` (see above) in
   `modloader\Cerberus Leader Voice Fix\levels\mansion_int\`. That needs PAKPatch
   and the extracted `ManHunt.PAK` (see Requirements). **The vanilla
   `levels\mansion_int\entityTypeData.ini` must still be in the game folder.** If
   it's missing, modloader doesn't apply its copy and the leader uses the
   bodyguards' voices again (seen in game).
3. Set `Log=1` in the `.ini` to check it applied: `cerberusleadervoice_asi.log`
   appears in the game folder with one line per type changed and a `patched:` line.

## Build

`build.cmd` builds `bin\CerberusLeaderVoice.asi` with Visual Studio's `cl` (Win32)
and copies the `.ini` next to it. Edit the `VCVARS` line if Visual Studio is
somewhere else.

## Testing

**Checked outside the game (2026-11-23):** a 32-bit test host loaded the ASI with the
real table bytes copied from `manhunt.exe` to `0x6AF966`. The default mapping, a
custom `.ini` (including a bad value, which was logged and skipped) and the no-`.ini`
defaults all wrote the expected Y1V1 cells and left the other voices alone. A
modified table made it abort without writing anything.

**Confirmed in game (2026-11-23):** the log showed all 7
default types patched, and the CHIEF spoke somewhat more often than before. The
defaults were then extended from 7 to 22 types (combat taunts, searching, chat).

**22-type defaults confirmed in game (2026-11-24):**

| situation | heard |
|---|---|
| shooting at the player, player in sight | `Y1V1DS1` |
| searching the shadows | `Y1V1GIC8`, `GIC6`, `GIC2`, `GIC9` |
| finding a thrown severed head | `Y1V1CFA1` |
| spotting the player again | `Y1V1DS4` |
| player runs out of his sight | `Y1V1DS3` |

The player was happy with every one, including mumbling lines while he searches the
shadows.
