# Flash overhaul recon: no-web lock and after-images (2026-11-19)

Game build **4.0630.0.0 only**; addresses are `exe+RVA`. Tags: **[V]** verified in game, **[RE]** read from the exe or the archive (static; nothing here
was run in game unless marked **[V]**), **[I]** inference, **[?]** unknown. Tools: `tools/sigtools/fx.py` (disassembly, DDL, enums), `tools/assets/extract.py`,
`tools/sigtools/actor2json.py`. Extracted assets were read from scratch folders and are **not** in the repository.

The Flash overhaul wants five things; three are solved by existing mods (lightning FX, super speed, Quantum Break Renewed's bullet time). This file covers the two
that do not exist: **disabled webs** and **after-images** (hypersense).

| Item | Verdict |
|---|---|
| Disabled webs | **Cheap.** The game's own lock component does it. Recipe below, one new build and one in-game test to confirm. |
| After-images | **Possible, not cheap.** The engine has a Spider-Man "hologram" actor and every piece of a fading ghost (material override, alpha fade, suit cloning), but no pose copy and no trail of past frames. Staged plan below; the first probe needs no new code. |

## 1. Disabled webs

### 1.1 What to lock

`HeroAbilityLock` (full table, layers, methods: [EndlessArena/docs/RECON.md](../EndlessArena/docs/RECON.md) section 1). For "no webs" lock these on **layer 0**
(boolean; layer 2 is the game's reference count, do not touch it):

| ability | index | blocks |
|---|---|---|
| kWebSwing | 24 | swinging |
| kWebZip | 21 | zip to point; the vent, object and perch zips too **[I]** (the traversal code at `exe+8e7540` asks it) |
| kWebSnap | 22 | web snap |
| kZipJump | 25 | zip-jump |
| kPointLaunch | 28 | point launch |
| kZipToSurface | 35 | zip to a wall or ceiling |
| kShoot | 15 | the web shooter and every gadget shot (it gates the weapon manager); also cancels a current aim |
| kGadgetWheel | 38 | the gadget wheel; lock it too if gadgets (web bomb, impact web...) should go |

Call: `lock->vtable[+0x50](lock, ability, 0, 0.0f)`; undo `vtable[+0x58](lock, ability, 0)`. A timed lock (`seconds > 0`) re-armed every second needs no cleanup
after a respawn or crash. Finding the component: `exe+16799a0(hero+0x80, exe+6daad90)` (or `exe+167ab40(hero+0x58, ...)`). One call that does it the way missions do:
`exe+9189e0 SetAllowedHeroMoves(hero, allowedMask[4])` (everything in the 27-entry toggleable list that is not in the mask gets locked on layer 0). **[RE]**

### 1.2 Web moves in combat are a separate lock

Abilities do not cover web strike, yanks, throws, web bomb and so on. Those go through `HeroCombatActionLocker` (type object `exe+6dcc950`), driven by the mission
node `RestrictHeroCombatAction` (execute `exe+9b1780`; data `RestrictionType {kPreventActions, kSetActions}`, `CombatActions` flags, `SpecificAttacks`). **[RE]**

The `CombatActions` flag set is at `exe+5e0f418` (it is the field type of `RestrictHeroCombatActionPrius.CombatActions`, type hash `0x02fed076`, also used by
`HeroCombatActionEvent.ActionFlag`). Web-related bits:

| flag | value | | flag | value |
|---|---|---|---|---|
| kTraversalStrike | 0x1 | | kElectricWeb | 0x200 |
| kWebStrike | 0x2 | | kWebBomb | 0x1000 |
| kWebPounce | 0x4 | | kWebLine | 0x2000 |
| kWebYankBot | 0x8 | | kWebThrowWeapon | 0x200000 |
| kWebYankBotAttachment | 0x10 | | kWebBlast | 0x400000 |
| kWebYankEnviro | 0x20 | | kSpiderDrone | 0x2000000 |
| kWebThrowObject | 0x40 | | kImpactWeb | 0x80 |

Mask for "all web combat" (everything in the table except kSpiderDrone) = `0x1|0x2|0x4|0x8|0x10|0x20|0x40|0x80|0x200|0x1000|0x2000|0x200000|0x400000 = 0x6032FF`.
Re-derive it from `fx.py flags kWebStrike` before using it, in case the set was misread. In the node's execute, `Enable` with `kPreventActions` calls locker vtable `+0x60(mask)`; `kSetActions` first
calls `+0x68(mask)` and then `+0x60(~mask)`. The exact semantic of `+0x60`/`+0x68` was read, not run **[RE]**. The per-move calls are `exe+8bce80`/`exe+8bcfc0`
(records of 0x14 bytes, 7 slots at `locker+0xf8`, count `locker+0x190`), used for `SpecificAttacks`; the flag path is enough here.

Simplest route if the locker's vfuncs prove awkward: since `kMelee` stays allowed, leaving web strike on is a design choice. A Flash with no webs probably wants
**web strike and yanks off too** (they are web-based) but melee, dodge and takedowns on.

### 1.3 Evidence and open questions

- **[V] 2026-11-07**: writing layer-0 counter bytes at `comp+0x1c8+ability` blocked the matching actions in game (the bisect in `EndlessArena/docs/REWARD_DESIGN.md`).
  Its index labels were off by the table above, but it shows the write path works from a mod. The component lives on the hero entity; a respawn that recreates the
  hero gives a clean component, so **re-apply after `HeroSpawnedEvent`** (`0x183d74d2`).
- **Precedent**: the game already removes webs in missions (Harry's research station, the user recalls). It does it with `ToggleHeroMovesAction` (execute `exe+9b8020`)
  or `SetAllowedHeroMoves`. Which mission data does it is **[?]**; not needed for the mod.
- **[?] Falling.** With swing and zip locked there is no web-catch. Whether the game still has a landing roll, fall damage or an auto-recovery, and how a long fall
  from a roof feels, needs a play test. `kAirTrick` (36), `kLongJump` (6), `kWallRun` (18), `kWallRunLeap` (19), `kWallCrawl` (23) stay unlocked and are the
  Flash's traversal (`kFallDive` is an input action, not an ability).
- **[?] Prompts.** The on-screen "zip" prompts (prompt-type enum `f541cd98` has `kWebZip`, `kWebStrike`...) may still appear on targets; whether the lock suppresses them
  is unknown. If they stay, they are cosmetic, since the lock refuses the action.
- **[?] Mission and scripted web use** (zip QTEs, forced swings) is not blocked by the ability lock if a script pushes the hero into a state directly. Free roam and the arena do not do this.
- **[?]** Layer 0 is the mission layer: if a mission's own `ToggleHeroMovesAction` runs it will overwrite our bytes. Use a timed lock refreshed each second, or layer 1.

### 1.4 First test (needs one build)

A dev command `lock <ability|all-web> [on|off]` in the command file that applies 1.1 on layer 0, then in free roam: try swing, zip to point, zip to a rooftop edge, web
shooter, web strike, yank, and a fall from a tall building; note what the prompts do. Nothing in the repository does this yet.

## 2. After-images (hypersense)

Goal: a train of increasingly faded copies of Spider-Man trailing him while he runs. Two ways to read it: **copies at past *positions* in an approximate pose**, or
**copies of past *frames* (exact poses)**. The first is within reach. The second needs access to the skinning data.

### 2.1 What the engine already has

**The Holo Decoy is a Spider-Man ghost actor [RE].** The suit power spawns `equipment/projectile/proj_HoloDecoy/proj_HoloDecoy.actor` (the thrown shot, a
`SpawnedShotConfig` with `ShotSpiderHoloDecoyPrius.HoloDecoyAsset`), which becomes `equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor`
(lifetime 20 s, max 30). The deployed actor's components (read with `actor2json.py`):

| component | data | why it matters |
|---|---|---|
| **`HeroLoadoutCloner`** | `HeroCharacter: "Spider-Man"` | **the actor wears the player's current suit** (code at `exe+d31ad0` onward copies `SkinManager` parts, `exe+20c18f0`/`exe+20c9f20`) |
| **`StaticCompositeMaterial`** | `NewMaterial: material/visualeffect/characters/hero/hero_spideysense_attacker_grounddecal/hero_perk_holodecoy_post.material`, **`MaterialAlpha: 1`**, `IsPreMaterial: false` | the hologram look is a composite material with an **alpha** value: a ready fade lever |
| `AnimControllerComponent` | animset `characters/hero/hero_spiderman/spiderman_holodecoy.animset` | the pose: the decoy plays one named clip, `HoloDecoy_Pose` (hashed in `exe+d31820`, played through `exe+15bae90`; clip `holo_decoy_idle_spiderman.animclip`), i.e. **a fixed idle pose, not a copy of the hero's pose** |
| `HeadManager`, `TargetData` (`kHero`, `Bit9` = **kDecoy**, `kBlueTeam`), `GroupDialogCoordinatorStaged` (`SPID_GEN_HOLO_DECOY_*` lines), `ConduitComponent`, `PhysicsComponent` | | **side effects to strip for a cosmetic ghost**: it is a decoy target (enemies attack it), it talks, it blocks, and `SpiderHoloDecoy` (`DamageRadius 5`, knockdown, electric damage) explodes |

Other decoy data: model `jr_spiderman_holodecoypose.model`, material `hero_Prog_SuitPower_Holodecoy_ReverseDepth.material`, post material graph
`hero_perk_holodecoy_post.materialgraph`, material variables `holospiderFadeIn` / `holospideyFadeIn`. The visual effects `prog_suitpower_holodecoy_intro` and `..._flare` are the spawn FX.

**The last-known-position silhouette [RE].** `HeroLastKnownPositionManager` (vtable `exe+38b2b58`; creator `exe+93f520`) spawns an actor named
"HeroLastKnownPositionManager Silhouette Actor", adds an `AnimControllerComponent`, and applies a material override with `exe+1772530`. The material path in the exe
(`exe+38b2ae0`, hashed from the pointer at `exe+5d9f750`) is `material/characters/hero/hero_spideysense_ar_attacker_highlight/hero_spideysense_ar_weapon_highlight.material`.
`MemorySyncConfig` holds the data twins (`+0x28 SilhouetteActor`, `+0x40 SilhouetteMaterial`). This is a second, simpler "ghost Spider-Man" with its own material;
its actor path was not resolved.

**Material override on any model [RE].** `exe+1772530(ModelInst*, slotId, material*, flag)` writes an entry into the model instance's override list
(`ModelInst+0xa0`, 16-byte entries, count `u8 +0x9f`; 70 callers). The script node `OverrideAllMaterialsAction` (execute `exe+bea4d0`; pins `Override`/`Reset`, value
`TargetActors`, data `MaterialOverride@0x10`; uses `SkinManager` and `HeadManager`, so it reaches every part including the head) is the actor-level form.

**Fading [RE].** `FadeModelAlphaAction` (execute `exe+20511a0`; value pins `Target`, `Alpha`, `Time`; data `Dissolve`; uses `AlphaSync`), component `ActorFade`
(`FadeInTime`, `FadeOutTime`, `DestroyWhenFadedOut`, `kAlpha`/`kDissolve`), `FadeMaterial`, `FadeOverTime{SlotIndex, LifeTime, FadeTime}`, and per-material constants
(`AnimateShaderConstantAction`, `RenderOverrideConstantComponent{ConstantName, StartValue, EndValue, TransitionTime}`).

**A built-in "spawn an actor every N metres or seconds" status effect [RE].** `StatusEffectActorTrail` (component type object `exe+7c5bc00`; DDL
`StatusEffectActorTrailPrius`, size 0x98): `DecayTime`, `Shader` (file) with `ShaderTValRampUpTime/RampDownTime/RampDownEndTime`, **`Actor` (file)**, `TimeDelayInitialSpawn`,
`DistanceForInitialSpawn`, `TimeBetweenSpawns`, `DistanceBetweenSpawns`, `DropOnGround`. Its spawn handler `exe+20e87d0` spawns the `Actor` at the owner with
`SpawnRaw`-style calls (`exe+190cfa0`, `exe+159db20`, `exe+159e590`), and `SetupSpawnChecks` (`exe+20e8660`) shape-casts to the ground (profile 6 `kGround`) for
`DropOnGround`. Nothing in the exe tells which enemy uses it (the data does). It is a ready **trail scheduler with a shader fade ramp**. Whether it can be put on the
hero is **[?]**: components are added with `AddComponent exe+159cb80(entity, &typeObject, &prius)` (the silhouette code does this for `AnimControllerComponent`), and the
`Prius` layout is in `recon/ddl_types_4.0630.txt`, so building one by hand is possible in principle **[I]**.

**Related speed FX [RE]**: `HeroSpeedFXConfig` (`StageOneSpeed`, `StageTwoSpeed`, `MotionBlurBonusMaxSwing/NonSwing`, `MotionBlurRampSpeedMin/Max`,
`MotionBlurExposureScaleMax`), events `HeroSpeedFXContrailsOn/OffEvent{RightHand}` (senders `exe+863c70` / `exe+85e290`), `HeroScreenSpeedFXOn/OffEvent` (`exe+8639d0`),
`HeroAudioSpeedFXOn/OffEvent` (`exe+862430`, `exe+863ff0`). These are the streak/blur layer the game already drives from hero speed; raising the motion-blur numbers is a
free partial substitute for a ghost train.

**Skinning data (for exact pose copies) [RE].** A `ModelInst` holds: `+0x88` its model resource, **`+0xd8` an array of 64-byte joint matrices** (count
`u16 [[[ModelInst+0x88]+0x200]+2]`; allocated and bind-pose-initialised by `exe+1773210` ("ModelInst Joint Mats"; callers `exe+1778c80`, `exe+17791a0`, `exe+177c590`;
freed by `exe+1775200`), `+0xa0` the material override list, `+0xc0` AnimVert, `+0xd0` cloth. Rendering submits through `ModelSubmitJob::SubmitModelInst`
(`exe+17991a0`, 15 KB) and a `SkinningBuffer` that keeps **current and previous** frame skinning indices (`m_SkinningIndex`, `m_SkinningIndexPrev`) for motion vectors;
`ComputePrevFrameAnimJointQstFromHavokCloth` shows previous-frame joints exist for cloth. The renderer is D3D11 (`D3D11CreateDevice`, GFSDK SSAO D3D11) with an
embedded D3D12 backend (D3D12MA strings); which one is live was not checked. The hero is several `ModelInst`s (`SkinManager` parts: body, head, hands...), how many is **[?]**.

### 2.2 Designs, cheapest first

| | Design | Pose | Fade | Cost / risk |
|---|---|---|---|---|
| **S0** | Spawn the deployed-decoy actor (or the silhouette actor) at the hero, override its material, fade it, despawn. Proves the ghost look and its suit cloning. | fixed idle pose | `StaticCompositeMaterial.MaterialAlpha` or `FadeModelAlphaAction` | **No new code**: `spawn <path>` already exists in the command file. |
| **S1** | A ring buffer of the hero's position and heading, sampled every `k` ms; a pool of ghost actors placed at the oldest samples with alpha falling by age. | approximate: fixed pose, or a clip chosen by hero state | per-ghost alpha | Medium: the ghost must be cosmetic (strip `TargetData` decoy bit, dialog, damage), and the pool avoids spawn cost (20-38 ms per spawn, [GAME_INTERNALS](GAME_INTERNALS.md)). Model capacity: ghosts share the hero's assets, so they should not hit the enemy-model limit, **[?]**. |
| **S1b** | Same, scheduled by a hand-built `StatusEffectActorTrail` on the hero. | same | the status effect's shader ramp | Unknown whether a hero can host it; saves writing the scheduler. |
| **S2** | **Exact pose**: ghosts whose joint-matrix arrays (`ModelInst+0xd8`) are overwritten each frame from a delayed copy of the hero's. Needs ghosts with the same skeleton and **no animation controller of their own**, so nothing overwrites our writes. | exact | per-ghost alpha | High: the ghost's skeleton must match (suit parts), and the write must land after the game's skinning update and before submit; where that is is **[?]**. |
| **S3** | Re-submit the hero's `ModelInst` through `SubmitModelInst` with stored skinning and a ghost material, or hook D3D draw calls. | exact | blend state | Very high; deep renderer work. Last resort. |
| **S4** | Not ghosts: raise `HeroSpeedFXConfig` motion blur and contrails while the speed mod runs. | n/a | n/a | Trivial; a streak, not a figure. Good complement to S1 or S2. |

Recommendation: S0, then S1 with the pose chosen from the hero's state, judged against how it looks at Flash speeds; at sprint speed the eye barely reads pose, so S1 may
be all that is needed. S2 only if S1 looks wrong.

### 2.3 Unknowns, in the order they block work

1. **Can the decoy actor be spawned bare and be made cosmetic?** Its `SpiderHoloDecoy`, `TargetData` and dialog components are in the actor data. Run `spawn` on it and `comps`, then see what enemies do.
2. **Does the hologram material/alpha work on a decoy that was not thrown?** The throw path sets up a few things (`HoloDecoy_Pose` is played from `exe+d31820`, state at `+0x93/+0x94`).
3. **Which pose can a ghost play?** The animset is `spiderman_holodecoy.animset`; whether `AnimControllerComponent` can be pointed at the hero's locomotion animset at runtime, or a modified actor is needed (an Overstrike `.stage` asset replacement), is unknown.
4. **Does a ghost-only actor with the hero's skeleton exist** without an animation controller (for S2)? The decoy's model is `jr_spiderman_holodecoypose.model`, a posed variant: **[?]** whether its skeleton matches the suit parts.
5. **The write point for S2**: the function that fills `ModelInst+0xd8` each frame (the writers of that field were not enumerated; start from the callers of `exe+1773210` and `exe+17991a0`).

### 2.4 First probes

- **P1 (no code, one command each; in game).** With EndlessArena's DLL installed, in free roam:
  `spawn equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor 4`, then `comps <index>`. Watch: does it appear, wearing the current suit, translucent?
  Do enemies target it? Does it speak? When does it disappear? (The `spawn` command waits for dependencies first.)
- **P2 (code).** Dev command to set the decoy's material alpha or call `FadeModelAlphaAction`'s logic (`exe+20511a0`) on a spawned entity, and one that despawns it.
- **P3 (code).** A ring buffer of hero position/heading and a pool of N spawned ghosts placed on it: the S1 prototype.

### 2.5 Not done

- The silhouette actor's asset path (`MemorySyncConfig` data), the `StatusEffectActorTrail` users' data, and the animset's clip list.
- A scan for every asset referencing the decoy is partial (`grepall` over `asset_archive`, about 10 of 208 archives). The assets already read: `A89C14AA30828C24` (decoy visual
  data), `BC5737193A998762` (shot config), `A673E003A44576EF` (pose model/animclip), `9E1DD8EE72B8BA97` (reverse-depth material), `ADB8DE9086364A24` (decoy visual effects).
- Whether the existing Overstrike suit mods or the "Iron Arms" Holo-Decoy replacement (see the LEDGER) change the decoy on this install: the player's install can differ from stock.

## 3. What the MSM2 mod "Edge of Time Hypersense" teaches (2026-11-19)

A third-party Overstrike `.script` for Spider-Man 2 (v1.4.0 package, DLL banner "AfterimageHypersense 2.2.0", depends on ModSettings). **Read statically only** (`pefile`, strings);
it was not run, and it is MSM2-only, so it cannot be tested here. It does what we want, so its strings are a map of what the engine offers. Everything below is **[RE]** of that
DLL's text and says nothing certain about MSMR.

**MSMR 4.0630 has no `AfterimageComponent`.** The MSM2 mod does not draw ghosts itself: it adds the game's own **`AfterimageComponent`** to the hero (via `AddComponent`, then
`Actor::Enable` to run `OnActivate`) and configures it with five values: `MaterialPath`, `MaterialMappingName`, `ExpireDuration`, `Interval`, `NumAfterimages` (an
`AfterimageObjectList` holds the echoes). Searching the MSMR exe and the DDL dump finds no such class or field, so **S1/S2 in section 2.2 must be built by us**; there is no
engine trail to switch on. (The `StatusEffectActorTrail` of section 2.1 is still the nearest native scheduler.)

What its development history says about doing it ourselves:

- **The engine design is "one clone of each hero skin piece per echo".** Its probe log talks about "hero piece N: handle, `ModelInst`, model, joints, morphInfo", "echo clones", a **ring** of echoes, and a
  later "part-double" path (`CreateModelInst`, `DriveEcho`, `PopulateLook`, `ApplyLookState`, `CompositeAddMat`, `IncludeInDatabase`, `DestroyInst`, `GetPartModel`): create a visible `ModelInst`
  per skin part, drive its joint data from the hero, add a material, destroy it on expiry. This is our S2, confirmed as the approach the developers' own component takes. In MSMR the same
  pieces exist by name: `ModelInst`, `SkinManager`, `exe+1773210` (joint mats), `exe+1772530` (material override), `exe+1775200` (free).
- **The pitfall that cost it the most time: clones sharing the hero's vertex-animation ("morph") block.** The echo copy step overwrote a clone's block with a copy that still pointed at the hero;
  destroying the clones then corrupted memory ("this is the moment the old build corrupted memory"). Its fix: patch one branch (`74 23`/`EB 23`, found by signature, must match exactly once)
  so clones keep their own block, plus a crash guard and a "hero in flux (transition/streaming): suspend the trail" rule. In MSMR the equivalent is `ModelInst+0xc0` (AnimVert), `+0xd0` (cloth),
  and the 64-byte joint array at `+0xd8`: **a clone must own all three and never alias the hero's.**
- **Suit changes break it**: it hooks `SkinManager::ApplySwitchGroup` to suspend the trail while suit parts swap (MSMR: `SkinManager Activate` / `ApplySwitchGroup` strings at `exe+4f83290`..`+4f83318`),
  and re-applies when the hero object changes (character swap, respawn).
- **Frame hook**: it drives echoes from `SceneManager::PostUpdate` ("main-thread pump"), i.e. after the scene update, not from the gameplay tick. For our S2 that argues for hooking after skinning rather than at
  the start of the gameplay tick (`exe+1f745f0`); MSMR's counterpart is not located.
- **Material choices that read as a ghost** (MSM2 paths, listed as presets): the camo "invisible" lit/post materials (`mj_invisible_01`, `camo_wraith_invisible_post/lit`), hologram materials, the
  Negative-energy trail material, `blackcat_trail_distortion_infinite`, `hero_suitpower_fearitself_trail`, chromatic-aberration Wraith material. MSMR has its own: the Holo Decoy
  material, the spidey-sense highlight, and a Black Cat distortion trail (`ADB8DE9086364A24` lists `hero_melee_punch_distortion_trail`); the MSM2 paths themselves were not looked up in the MSMR archive.
- **"Hypersense" is three layers in that mod**: the afterimage trail; a **time-scale split** (`HeroTimeScaleManager`, "world X, hero Y", applied through `Actor::SetTimeScale`, the same lever
  QuantumBreak Renewed uses); and a **pushed animset** (`HeroAnimsetPusherSystem.AddAnimset/RemoveAnimset`, e.g. a boss-style moveset while active). The animset pusher answers open question 3 of
  section 2.3 for MSM2 (a ghost's or the hero's moveset can be swapped at runtime); `HeroAnimsetPusherSystem` was not found by name in the MSMR exe, so the MSMR route is **[?]**.
- **Trigger and plumbing**: double-tap Circle (via `HeroButtonCache`, with a quiet-start rule so finishers and dodges do not trip it) or F8; settings through ModSettings; a log and an ini written to
  `Downloads`; text paste from the clipboard for material/animset paths (the `OpenClipboard`/`GetClipboardData` imports are that feature, nothing else in the strings suggests more).
  It also installs a first-chance exception handler that skips faulting "poison" calls inside the game; do not copy that, it hides corruption.

Net effect on the plan: the approach (clone each skin part's `ModelInst`, own all buffers, drive from a ring buffer, fade by material, suspend on suit change and character change) is
validated by a shipped mod, and the main hazard (aliased animation/morph buffers) is known up front. It does not remove the need to find MSMR's write point and clone-creation call.

## 4. Built from this recon (2026-11-19)

Two mods came out of it: **`DisableWebs/`** (section 1; README there) and the **After-images option of QuantumBreak Renewed** (a first version of S1; QBR lives in its own,
gitignored folder, option module "After-images", F8 in game, optional ini `Documents\...\QuantumBreakRenewed\QuantumBreakRenewed.ini`). Neither has been played yet. Facts they rely on, read later the same day **[RE]**:

- **Combat lock is a bitmask.** `HeroCombatActionLocker` vtable `+0x60` (`exe+1e89a50`) is `[this+0xe8] |= mask`, `+0x68` (`exe+1e89a60`) is `[this+0xe8] &= ~mask`
  (the class derives from `GeneralStateLockComponent`; its query at `+0x70` ORs the persistent mask with the timed ones). No other state to keep.
- **Model alpha is a byte.** `FadeModelAlphaAction` (`exe+20511a0`) writes `alpha*255` to **`byte [ModelInst+0xbe]`** (dissolve variant: `+0xbd`), then calls `exe+191bf60(ModelInst, -1)`
  (mark dirty). The entity's first field `[entity+0]` is its `ModelInst` (also the transform). `[ModelInst+0x5f] != 0` means invalid.
- **Material override from code.** The silhouette code (`exe+93f520`) hashes the path with `exe+1bb2680(out, path)`, looks the u64 up with `exe+1bcf4e0(exe+6058208, key)` (entry = u32 index) and gets
  `material = [exe+6058240] + index * [exe+6058248]`, then `exe+1772530(ModelInst, 0, material, 0)`. The material must be loaded (the spidey-sense highlight material is in the boot config).
- **Memory-pose actors**: `objects/test/JNB/Memory_Poses/jnb_spiderman_memory_pose_Stand.actor` (1.1 KB) and `..._perch.actor` (0.7 KB) are bare posed Spider-Man models (no animation controller,
  no decoy/dialog/damage components), referenced by `MemorySyncConfig` (`SilhouetteActor`). They are the ghost actor in the prototype instead of the Holo Decoy.
- Whether a model whose transform is rewritten re-renders where it was moved is not known (`exe+191bf90`/`exe+191c010` look like the transform setters), so the prototype spawns and despawns instead of moving.
- Checked later: the renderer's submit function (`exe+17991a0`) reads `byte [ModelInst+0xbe]` (`exe+1799657`) and skips the model when it is 0, so the alpha byte the prototype writes is consumed where it should be. Whether it blends or dithers is seen only in game.

### 4.1 After the first play test (2026-11-20)

- The user played "always" mode: the ghosts looked good but were a static A-pose, and with the hero standing still ghosts kept spawning at his position (the log shows 803 spawns / 795 despawns, 23 ms per spawn, material and alpha reported applied). Fixed by gating spawns on distance moved (`spacing`).
- **Why the pose is an A-pose, offline [RE]:** `jnb_spiderman_memory_pose_Stand.model` has **0 joints**: it is a baked static mesh, so there is nothing to pose. The hero's `hero_spiderman_body.model` has **212 joints**. An exact-pose ghost therefore needs a skinned ghost on the hero's skeleton (S2), not another pose model. The decoy actor (`HeroLoadoutCloner`) is the only ready-made skinned Spider-Man copy found, but it plays its own animation (see 2.1).
- Stop-gaps added: a forward **lean** (rewrites the model transform rows after the spawn), an **effect** material layered as a post-composite override (`enm_electrified_status_post_red`, hologram or shield presets, F10 cycles; which are loaded is logged), and opacity **flicker**. The override-type flag value 2 for post-composite is inferred from `MaterialOverrideMap.Type` and the decoy's `StaticCompositeMaterial.IsPreMaterial=false`; unconfirmed.
- The first ghost spawn now logs joint counts and joint-matrix pointers for both the ghost and the hero model, to settle the S2 questions.

### 4.2 Second play test (2026-11-20): the log answered the pose question

- **The ghost and the hero share one skeleton [verified in the log]:** the first ghost logs `model=...e4c0 joints=237 joint mats=...` for both the ghost and the hero (`[mi+0x88]` is the same pointer, 237 joints, each with its own joint-matrix array). The A-pose is simply the bind pose `exe+1773210` fills in at creation; nothing rewrites the ghost's matrices afterwards.
- So **pose tracking by copying** is the next test: at spawn, copy the hero's `[mi+0xd8]` array (237 x 64 bytes) onto the ghost's and copy the transform's three orientation rows (the position stays at the spawn point). Built into QuantumBreakRenewed (`pose_copy`, on by default; the lean is only the fallback). Unknowns: whether the renderer re-reads the array after the write (we call SetModified), whether the matrices are model-space (expected) or world-space, and tearing, since the hero's array is read while the game may be updating it. The ghost is only the primary model of the memory-pose actor; the hero is also made of skin-part model instances (`OverrideAllMaterialsAction` loops them with `exe+20c9f20(SkinManager, index, 0)`), which the ghost does not have.
- **Effects:** `enm_electrified_status_post_red` is not loaded in free roam; the holo and shield materials are, but layered with override flag 2 they showed nothing. `OverrideAllMaterialsAction` always passes flag 0. The look is now chosen as the *base* material instead (F10 cycles highlight / holo / shield / electric / own); whether the "post" materials render as a base override is the open question. `StaticCompositeMaterial` (the decoy's hologram) only stores data (path at `+0x48`, alpha `+0x148`, pre flag `+0x14a`); the code that applies it was not found.

### 4.3 Why the looks never changed (2026-11-20)

- The override list is consumed by the submit code: for each submesh it calls `exe+1775ea0(ModelInst, materialIndex)`, which takes the first override entry whose id is 0 or equals the submesh's original material mapping id and uses that material **as the surface material** (`exe+1799f10`). The materials tried first (the spidey-sense highlight, the decoy's hologram post material, the defensive shield) are *composite/post* materials (`[material+0x60]` bit 12; the readback showed `composite=1`), not surface materials, so swapping them in changed nothing visible. `own` (no override) looked identical, which is how the user's F10 test showed it.
- New looks are surface materials that exist in the archive (checked with `toctool check`): `hologram_city_01`, `weather_station_frequency_L44_plane_scanlines_mat`, `light_red`, `test_grey_medium`, and the two distortion trail materials; whether each is loaded in a level is logged (`ghosts: look ... loaded / NOT loaded`). A material that is not loaded cannot be used.

## 5. Hyper-Sense crash record (2026-11-20 17:17, one occurrence, not fixed)

- Dump `Marvel's Spider-Man Remastered-v4.630.0.0 2026-11-20-17-17-11.mdmp`: access violation, game thread, `exe+319e019` (function `exe+319df00..319e33b`), write of `0x33bc0`. The function walks a linked list of render objects (next pointer at `[node+0x98]`) and pushes each into a growable array (`exe+31a81f0` resizes). The array base was null (`r15 = 0`) while the item count was `0x6779` = 26,489, so the write went to `0 + 0x6778*8`. Either the list is corrupt/cyclic (26k nodes when a few hundred exist) or the resize failed; which, is `[?]`.
- Context from `HyperSense.log`: the last look in use was **DISTORTION** (`blackcat_trail_distortion_infinite`), 25 s of clones, then slow motion was switched off and all clones despawned at once (10 despawns in 0.2 s); the crash came 1.2 s later. Spawn and despawn counts balanced (2910/2910), so no leak. HOLOGRAM, SCANLINES, RED GLOW and FLAT GREY ran the same way and ended cleanly.
- Suspect `[?]`: trail/distortion materials are drawn by a special pass whose list keeps entries for model instances that were despawned. Per the one-off rule nothing was changed; if DISTORTION (or FEAR, the other trail material) crashes again, remove them from the LOOK row or unregister the model before despawn.
- Side note: at SLOWEST (0.001x) the game clock barely advances, so the "game frozen / running again" log lines (31) flap and clones are briefly held; harmless but worth a smarter freeze test if SLOWEST is kept.

### 1.7 Transitions the ability locks do not cover (2026-11-20, static only, nothing played)

`Hero::HeroTransitionManager` (vtable `exe+38b4350`, 186 slots) has one "try to enter state X" function per transition, each returning bool. Listing the slots by the `HeroState*` type objects they reference gives the map
(slot: rva: state): 106 `980af0` TraversalStrikeFlight, 34 `971720` MeleeAir/Melee, 81 `97c470` Swing, 82 `97c9d0` SwingJump, 181 `98c9f0` AirTrick, 125 `984a10` WallRunLeap, **126 `984be0` WallRunCornerSling**,
128 `985610` and 137 `987420` WallRunJump, 129 `985a10` WallRunTopMount, 130 `985c90` WallRunToLedge, 132 `986150` WallRunTopCrest, 133 `986490` WallRunRangedEntry.
- **Corner tether = slot 126.** It builds a request for HeroStateWallRunCornerSling and passes it to the state machine (`[machine+0x50]`), returning the result. Signature `(this, vec3*, vec3*, vec3*, bool)`. DisableWebs 0.2.0 hooks it (MinHook, prologue-checked) and returns false while the web lock is on. Untested.
- **Swing kick and wall-run pull-up: not identified.** Candidates: swing kick among 34/106/81 (the air melee and traversal-strike entries; `kTraversalStrike` 0x1 is already in the combat mask), pull-up among 129/130/132. Locking the wrong one would break
  ordinary wall-running, so they wait for a log: hook the candidates, log which one returns true while the player does the move.
- **Log result (2026-11-20):** pressing JUMP while wall-running made slots 132 (WallRunTopCrest) and 128 (WallRunJump) return true in the same instant; swing/air melee showed up as slot 34. DisableWebs 0.3.0 refuses 132 with the lock on (a guess). The swing kick is kept (maintainer's call), so the logging hooks were removed (they seemed to have broken it).
- **2026-11-20 later:** refusing slot 132 left the pull-up working (hook confirmed active in the log), so DisableWebs 0.3.1 refuses slot 128 (WallRunJump) instead; the normal wall-run leap is slot 125.
- **Third log:** with slot 128 refused, slot 125 (WallRunLeap) fired right after and the pull-up still happened. DisableWebs 0.3.3 refuses both 128 and 125 (the plain wall jump goes too).
- **Confirmed in game (2026-11-20):** DisableWebs 0.3.3 refusing slots 128 and 125 removes the wall-run pull-up (it also removes the plain wall jump). The corner tether block (slot 126) was active in the log and the rooftop slingshot was reported working.

## 6. Running on water in slow motion (static recon 2026-11-21, nothing run in game; SHELVED by the maintainer the same day, no probe was ever run)

**Goal:** while Hyper-Sense slow motion is on, Spider-Man runs or walks on water; slow motion off puts the normal behaviour back (fall in, tread water).

**What the game does today [RE]**
- Entering water is a state transition, not a physics effect: `HeroTransitionManager` slot 161 (`exe+98aac0`) tries `HeroStateSwim` when the hero's water info says he is deep enough, slot 162 (`exe+98abc0`) does it when riding a moving surface. Both end in `[stateMachine+0x50](HeroStateSwim, params)` like every other transition, so they can be refused the way DisableWebs refuses slots 125/126/128.
- Slot 161's test: a hero component reference at `hero+0x78c` resolves to an object; at `+0x2a4` of it there is a water block with floats at `+0x184` and `+0x188` and a flag byte at `+0x19c`. The test is `[+0x184] - [+0x188] > 1.4` (the constant at `exe+5d9f878`), unless `[+0x188] > 0`, or the flag is set and the caller asked for it. Also refused when a `NoSwimVolume` is hit (`exe+98f9a0`). What the two floats mean (water depth under the feet, surface height?) is **[?]** until read live.
- While swimming the hero uses `MoveProcessorWaterStandard` (vtable `exe+4f70e90`); it issues its own water raycasts (`SubmitRequest`, `exe+1fd7e90`).
- Water is something rays can hit: the collision profiles include `kWater` and `kGroundAndWater` (enum `cd43dfed`), `WaterCollideChecker` issues water tests, and many events carry `HitWater` / `IsUnderwater`. The hero's own ground checks use profiles such as `kHeroGround`, so the water surface is probably not ground for him. **[I]**
- The sea itself is a rendered FFT simulation (IGWater) with a read-back; whether the physics water surface is flat is **[?]**.

**Ways to do it, cheapest first [I]**
1. **Refuse the swim transition (slots 161, 162) while slow motion is on and give him something to stand on.** Refusing alone only makes him fall through the water, so it needs a floor.
2. **The floor:** a large, thin, invisible static collider kept at the water surface under him (EndlessArena already spawns actors; a flat prop with collision would do), moved with him each tick and removed when slow motion ends. He then runs on ordinary ground with ordinary animation and footsteps, and removing the collider puts him back into the normal fall-and-tread behaviour. Needs: a prop actor that is flat and solid, hiding it, and the surface height (from the water block above, or a downward water-profile raycast).
3. **Alternative, no collider:** each tick, if he is over water and slow motion is on, clamp his Y to the surface and zero his vertical velocity. Cheaper, but the game will think he is airborne (fall animation), so it probably looks wrong.
4. **Alternative, data:** make the hero's ground raycast use `kGroundAndWater`. Found as a profile enum only; where the hero's ground check picks its profile is not found yet.

**Next step (needs one in-game probe, no code risk):** a log-only build that prints the hero's water block (`+0x184`, `+0x188`, `+0x19c`) and position each second, and logs when slots 161/162 succeed, while the player stands on a pier, wades and swims. That fixes what the floats mean and what height to put the floor at. Then build option 1+2.

## 7. Accelerated Decoy (recon 2026-11-21, static only, nothing run in game)

Mod name: **Echoes of Time** (decided 2026-11-21; Split Second and others were considered; Hyper-Sense is now one of its two abilities, and nobody had installed the standalone 1.0.0). Both abilities come from *Spider-Man: Edge of Time*: **Hyper-Sense** is the Amazing Spider-Man's (slow motion + after-images, as before) and **Accelerated Decoy** is Spider-Man 2099's: one decoy of him at his position and pose that enemies shoot at. The mod has a top row, ABILITY, that switches between them.

**The game already has this ability [RE]: the Holo Decoy suit power, and enemies are built to prefer it.**
- Standard enemy targeting (`configs/StandardEnemyTargetingConfig/StandardEnemyTargetingConfig.config`, a `TieredAggroTargetingGamePrius`, used by 213 of the 287 enemy actors read: thugs, grenadiers, riflemen, whip/electric thugs, prison thugs and the rest): **tier 1 is `TargetTypes: Bit9` = `kDecoy` (0x200)**, range 500 m, aggro at range 20/40/100, `ProximityAggroRate 200`; tier 2 is `kHero`; tier 3 is `kBot`/`kAlly`. So any target carrying the kDecoy bit is attacked before the hero. `StartAggro 15`, `SwitchAggro 5`: an enemy already shooting at him switches once the decoy's aggro beats his by 5. The Shocker's config is `Bit9 | kHero`.
- **Sniper config has no decoy tier** (`kHero | kBot+kAlly`): snipers keep shooting Spider-Man. Electro, Black Cat and Taskmaster configs have no tier list of their own (different structure, not read). Unknown for the other bosses.
- The decoy actor `equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor` has `TargetData` with kDecoy and `kBlueTeam`, so no new data is needed for targeting. Its `SpiderHoloDecoy` component (update `exe+d31820`, vtable `exe+39083f0`) each frame: plays `HoloDecoy_Pose` once, takes the **owner's allegiance** (`exe+2114d70` of the ref at `+0x4c`, falling back to its own actor) and writes it into `TargetData` (`exe+2117e00`, plus `exe+2117f10` with `[+0x98]`), and ends via `exe+d315d0` (explosion: `DamageRadius`, knockdown, electric damage) when elapsed (`+0x88`) passes `MinLifetime` (`+0x7c`) and either `MaxLifetime` (`+0x80`), `MaxEngageTime` (`+0x84`) or a health test says so. Config fields (`SpiderHoloDecoyPrius`): MinLifetime, MaxLifetime, MaxEngageTime, Health, DamageRadius, DamageType, DamageAmount, StatusAmount, KnockbackAmount, KnockbackLevel, **MaxTargeters**. The instance offsets of Health and MaxTargeters are not read yet.

**Design [I]**
1. **Targetable part:** spawn the game's own deployed decoy actor at Spider-Man's position (the spawn path of EndlessArena/Hyper-Sense already does this). It is the thing enemies shoot at. Its own model can be made invisible with the alpha path the ghosts use, or left alone if its look is acceptable.
2. **Visible part:** the existing after-image ghost (exact pose copy, hologram look as the default) at the same spot, so the decoy shows his real pose rather than the fixed `HoloDecoy_Pose` clip (the decoy's own animation controller would overwrite a pose copy; the ghost has none).
3. **Lifetime:** our own timer (the new knob). When it runs out, or when combat is re-entered, despawn both actors ourselves, which also avoids the end-of-life explosion. Before that, raise `MaxLifetime`/`MaxEngageTime`/`Health` on the instance (offsets to read) so the component does not blow it up first when enemies shoot it.
4. **"Re-enters combat" trigger:** watch the event stream (`events::StartTracking` already hooks `SendEvent`, exe+16d5e80) for the hero's attacks: `HeroCombatActionEvent` (an attack was chosen, senders `exe+b873c0`, `exe+b87540`), `HeroComboUpdateEvent` (`exe+919330`, fires as the combo count changes) and `DamageDealtEvent` with `Damager` = the hero. Web and gadget damage are not combo hits, so `DamageDealtEvent` by the hero is the broadest. Which one fires on a plain landed hit is **[?]**: log it first.
5. **Fullscreen signal [?]:** ready-made pieces: the decoy's own spawn effects (`prog_suitpower_holodecoy_intro`, `..._flare`), the screen fade messages (`FadeOutMsg`/`FadeInMsg` through `MsgSend`, as EndlessArena uses; a very short out-and-in is a flash), the speed-streak screen FX (`HeroScreenSpeedFXOnEvent`), and the hologram post material `hero_perk_holodecoy_post.material`. Not tried; the fade pulse is the cheapest, the post material the nicest.
6. **No slow motion in this mode:** the mode switch just keeps `HeroTimeScaleManager` untouched and routes D-pad Right to "place decoy".

**Unknowns, in order (each is one in-game look)**
1. **P1 (no code, EndlessArena's `spawn` command):** `spawn equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor` next to a group of ordinary thugs: do they switch to it, is it translucent, does it wear the suit, how long does it live, what ends it? This settles the whole idea; if it works, the rest is plumbing.
2. Instance offsets of `Health` and `MaxTargeters` (read `exe+d315d0` and the component's init `exe+d31360`/`exe+d311c0` offline).
3. Whether a decoy placed in combat is shot by enemies already mid-attack (switch rule above says yes after the aggro difference).
4. The right event to detect "he hit someone" (log probe).
5. Snipers and bosses keep targeting him (known for snipers). Say so in the knob's help text rather than fix it.

**Mod structure [I]:** a top row `MODE` (Hyper-Sense / Accelerated Decoy) ahead of PRESET; each mode keeps its own saved rows (separate ini sections, as the preset system already stores labels), and the rows below the mode row are the mode's own list (ModMenu already supports a rebuild via `take_rebuild`). Presets stay per mode. New decoy knobs: DURATION, LOOK (default HOLOGRAM), and the fullscreen effect choice.

### 7.x P1 result (in game, 2026-11-21)

Spawned `equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor` (EndlessArena `spawn`, 6 to 10 m from the hero) with three engaged `enm_pistolthug`.
- **Targeting works.** With the hero out of sight, the thugs keep attacking the decoy. With the hero in view they shot both.
- The game's decoy looks translucent and hologram-like, lived a limited time and ended in a particle burst.
- **Design decisions (user):** our decoy is Spider-Man himself, not a hologram. Use the existing after-image `default` look with flicker on high, and flicker out when it ends instead of bursting. The deployed actor is only for targeting.

### 7.y Echoes of Time: Accelerated Decoy built and confirmed in game (2026-11-21)

Mod name: **Echoes of Time** (folder `EchoesOfTime/`, ABILITY row: Hyper-Sense / Accelerated Decoy, version 0.1.0, not published).
How it works: the game's `proj_HoloDecoy_Deployed` actor is spawned where he stands and hidden (opacity 0, composite-overlay flag bit 3 of `+0x80` cleared on the model and skin parts, every game tick); an after-image ghost shows his pose in his own suit and flickers. Everything below was confirmed in game.
- **Targeting works** with no change to Spider-Man's own target data (an attempt to clear his kHero type at `TargetData+0x4c` made enemies ignore both and put him back in stealth; removed). Enemies pick by aggro: see the aggro-by-distance reading in the answer given to the maintainer (decoy tier outranks hero at equal distance; a hero within ~7 m can still outscore a decoy 15 m away).
- **Keep it alive:** `SpiderHoloDecoy` instance `+0x80` (MaxLifetime), `+0x84` (MaxEngageTime) and `+0x9c` (health) are set to 1e9 each tick; our own timer ends it.
- **Voicelines:** the 22 `SPID_GEN_HOLO_DECOY_001..022` Wwise events are dropped in a `PostEvent` hook (by FNV-1 id; the game objects are pooled pointers, so dropping by object does not work).
- **A-pose:** its first frames are an A-pose. Spawning it 1500 m up and moving it in (mi+0x30) hid the pose but enemies did not target it afterwards; zeroing the transform's orientation rows for 12 ticks (scale 0) at its real position hides it and keeps targeting. 
- **Destruction:** sending it 1500 m up before despawning removes the blue hologram blink (and its particles).
- **Not working / dropped:** player opacity flicker (the game overwrites it; row removed), fullscreen flash (never showed; replaced with a ~0.35 s slow-motion beat), attack cancels the decoy (dropped: time-based only), "enemies ignore me".
- **Untested lever:** `BonusAggro` component (script node `ModifyBotAggroAction`) to raise the decoy's pull.
