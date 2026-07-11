# FACS facial animation

Ekman & Friesen **Facial Action Coding System** Action Units drive the engine face layer (`runtime/game/g_facial.c`). AUs map to flex controllers, which map to IQM/glTF morph target names.

## Toggle

| Cvar | Default | Purpose |
|------|---------|---------|
| `com_faceFacs` | `1` | Apply FACS AU intensities into flex/morphs |

Requires `USE_GAME_AI_MIDDLEWARE` (default ON) so `Face_Init` runs. Startup log:

`Facial animation initialized (… N FACS AUs …; com_faceFacs 1)`

## Supported Action Units

| AU | Name | Primary flex / morph |
|----|------|----------------------|
| AU1 | Inner Brow Raiser | `browRaiseInner_L/R` |
| AU2 | Outer Brow Raiser | `browRaiseOuter_L/R` |
| AU4 | Brow Lowerer | `browLower_L/R` |
| AU5 | Upper Lid Raiser | `eyeWide_L/R` |
| AU6 | Cheek Raiser | `cheekRaise_L/R` |
| AU7 | Lid Tightener | `eyeSquint_L/R` |
| AU9 | Nose Wrinkler | `noseWrinkle` |
| AU10 | Upper Lip Raiser | `lipUpperRaise` |
| AU12 | Lip Corner Puller | `lipCornerPull_L/R` |
| AU14 | Dimpler | `dimple_L/R` |
| AU15 | Lip Corner Depressor | `lipCornerDepress_L/R` |
| AU16 | Lower Lip Depressor | `lipLowerDrop` |
| AU17 | Chin Raiser | `chinRaise` |
| AU18 | Lip Pucker | `lipPucker` |
| AU20 | Lip Stretcher | `lipStretch` |
| AU22 | Lip Funneler | `lipPucker` (scaled) |
| AU23 | Lip Tightener | `jawClench` (scaled) |
| AU24 | Lip Pressor | `jawClench` |
| AU25 | Lips Part | `jawOpen` (light) |
| AU26 | Jaw Drop | `jawOpen` |
| AU27 | Mouth Stretch | `jawOpen` + `lipStretch` |
| AU43 | Eyes Closed | `eyeBlink_L/R` |

Morph submission also emits the AU name itself (`AU12`, …) so assets can author either convention. Needs `r_morph 1`.

## C API

```c
Face_SetAU(handle, FACS_AU12, 0.8f);
Face_SetAUSide(handle, FACS_AU12, FACS_SIDE_LEFT, 0.5f);
Face_GetAU(handle, FACS_AU12);
Face_ClearAUs(handle);
Face_AUFromName("AU12");   /* also "12", "au_12" */
Face_ApplyMorphsToEntity(entityNum, refEnt, applyFn);
```

## Lua

```lua
local h = Engine.Face.create(entityNum)
Engine.Face.setAU(h, Engine.Face.AU12, 0.8)   -- or setAU(h, "AU12", 0.8)
Engine.Face.setAUSide(h, "AU12", Engine.Face.SIDE_LEFT, 0.4)
print(Engine.Face.getAU(h, "AU12"))
Engine.Face.clearAUs(h)
```

Emotion presets (`setExpression`) remain available; they are authored to match common FACS recipes (e.g. happy ≈ AU12+AU6).

## VoIP

Voice lip flap (`cl_voipLipFlap`) drives jaw/mouth morphs independently. Combine with `Face_SetAU(h, FACS_AU26, …)` for scripted jaw when not on VoIP.
