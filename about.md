This mod fixes a bug I kept running into while making Geometry Dash levels: certain levels would just refuse to load in normal play mode. The loading circle would finish and then nothing would happen — no crash, no error.

I tracked it down to a function called GJBaseGameLayer::shouldExitHackedLevel() returning true right before the game silently quit out. Looking into GD's internal data structures through the public Geode bindings, I found the engine keeps a lookup table of trigger groups sized to hold 10,000 slots, and there's a hardcoded loop elsewhere in the engine that indexes into that table assuming it's always full-sized. In the levels that were failing, the highest group ID used was landing right up near that 9,000–10,000 boundary — so the game's own "is this level corrupted/hacked" safety check was tripping as a false positive on a legitimately large, dense level, not on any actual hack or corruption.

The fix is two small, targeted changes:

Track the highest group ID actually used as the level loads.
If shouldExitHackedLevel() would return true and that max group ID is high enough to match the exact condition I diagnosed (≥9000), override the result to false instead, letting the level continue loading normally.

This doesn't touch anything else shouldExitHackedLevel() might legitimately catch — it only overrides the result for this specific, evidenced condition, so levels with this particular problem load correctly without disabling the safety check as a whole.
