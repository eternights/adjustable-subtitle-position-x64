// Adjustable Subtitle Position - GTA SA 2.10 (64-bit) port
// Original mod: 2.00 / 32-bit, by its original author (see credits).
//
// What it does: hooks the call to CFont::PrintString() inside
// CHud::DrawSubtitles() and replaces the Y position the game picked
// with a distance-from-the-bottom value read from the .ini file.

#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

// Config file name is chosen to match the original mod's ini (AdjustableSubtitle.ini)
MYMODCFGNAME(com.subtitlepos.arm64, Adjustable Subtitle Position, 1.1, Original author and 64-bit port, AdjustableSubtitle)
NEEDGAME(com.rockstargames.gtasa)

/* ---------- 2.10 (arm64-v8a) addresses, relative to libGTASA.so ---------- */
// "bl CFont::PrintString@plt" inside CHud::DrawSubtitles (2.00 32-bit was +0x4391E8)
static constexpr uintptr_t OFS_DRAWSUBTITLES_PRINTSTRING_CALL = 0x51E69C;
// Machine code that must be at that address. If it differs, this is not the
// exact 2.10 build the offset was made for, so we refuse to patch (no crash).
static constexpr uint32_t  EXPECTED_INSN = 0x97F42D49;
// Used only if the "RsGlobal" symbol can't be found by name.
static constexpr uintptr_t OFS_RSGLOBAL_FALLBACK = 0xC9B320;

/* RsGlobal layout in 2.10: +0 appName (8-byte pointer), +8 width, +12 height */
struct RsGlobalLite
{
    void* appName;
    int   maximumWidth;
    int   maximumHeight;
};

static uintptr_t      pGame = 0;
static RsGlobalLite*  pRsGlobal = nullptr;
static ConfigEntry*   pYBottom = nullptr;   // kept alive on purpose (live-editable)

static constexpr float VIRTUAL_HEIGHT = 448.0f; // the game's UI is laid out for 448px tall

static inline float ScreenScaleY(float v) { return v * ((float)pRsGlobal->maximumHeight / VIRTUAL_HEIGHT); }
static inline float ScreenScaleFromBottom(float v) { return (float)pRsGlobal->maximumHeight - ScreenScaleY(v); }

// Same signature as CFont::PrintString(float x, float y, unsigned short* text)
DECL_HOOKv(DrawSubtitle, float x, float y, unsigned short* text)
{
    (void)x; (void)y; // the game's own values are replaced
    DrawSubtitle((float)pRsGlobal->maximumWidth / 2.0f,
                 ScreenScaleFromBottom(pYBottom->GetFloat()),
                 text);
}

extern "C" void OnModLoad()
{
    logger->SetTag("SubtitlePos");

    pGame = aml->GetLib("libGTASA.so");
    if(!pGame)
    {
        logger->Error("libGTASA.so not found, mod disabled");
        return;
    }

    // Settings (written to the .ini on first run)
    pYBottom = cfg->Bind("Up/Down", 45.0f, "Subtitle");
    cfg->Save();

    // Find the game's screen-size struct by symbol name (falls back to a fixed offset)
    pRsGlobal = (RsGlobalLite*)aml->GetSym(pGame, "RsGlobal");
    if(!pRsGlobal) pRsGlobal = (RsGlobalLite*)(pGame + OFS_RSGLOBAL_FALLBACK);

    // Safety check: only patch if the expected instruction is really there
    uintptr_t callAddr = pGame + OFS_DRAWSUBTITLES_PRINTSTRING_CALL;
    uint32_t  insn = *(uint32_t*)callAddr;
    if(insn != EXPECTED_INSN)
    {
        logger->Error("Unexpected code at libGTASA+0x%lX (found 0x%08X, expected 0x%08X). "
                      "Not GTA SA 2.10 arm64? Mod disabled.",
                      (unsigned long)OFS_DRAWSUBTITLES_PRINTSTRING_CALL, insn, EXPECTED_INSN);
        return;
    }

    HOOKBL(DrawSubtitle, callAddr);
    logger->Info("Subtitle hook installed (screen %dx%d, bottom offset %.1f)",
                 pRsGlobal->maximumWidth, pRsGlobal->maximumHeight, pYBottom->GetFloat());
}
