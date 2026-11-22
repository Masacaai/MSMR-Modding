## Echoes of Time 1.0.0

Two Spider-Man 2099 powers from *Spider-Man: Edge of Time* for Marvel's Spider-Man Remastered, set entirely from the game's own Settings screen. A row at the top of the tab, **ABILITY**, picks which one the button uses (each keeps its own settings).

> Back up your saves before you try any mod.

### Hyper-Sense

- **Slow motion** on **T** or **D-pad Right**: three strengths from 0.2x down to 0.001x, and Spider-Man himself can run at 1x, 1.5x, 2x or 3x while the world crawls.
- **After-images**: copies of Spider-Man in his exact current pose, left behind while slow motion is on. They fade out and vanish the moment slow motion ends.
- A row for everything: mode, look (the original, hologram, scanlines, red glow, flat grey, highlight, nine colours and RAINBOW), how long the copies last, how their opacity fades, starting opacity, spacing, flicker.
- **Presets**: a PRESET row applies one of the ini files in `Documents\Marvel's Spider-Man Remastered\EchoesOfTime\Presets\`. Eleven ship with the mod (Flash, Reverse Flash, Hologram Trail, Matrix, Shadow Clones and more). Copy a file to make or share your own.
- **Experimental rows**: clone drift, clone size over life, speed blur and speed streaks.

### Accelerated Decoy

- **D-pad Right** (or **T**) leaves **one** copy of Spider-Man where he stands, in his exact pose and suit, flickering. Enemies shoot at it while he repositions. There is no slow motion, only a brief beat when the decoy appears.
- Rows: duration (3 to 60 seconds), look (default: his own suit), opacity, flicker (it also flickers out when it ends), entry slow-motion, messages. Attacking does not end it.
- It uses the game's own Holo Decoy as the thing enemies target, hidden, and shows the copy in its place: its voice lines are muted and its effects hidden.
- Enemies go for whoever has the most aggro, so an enemy standing right next to Spider-Man can still prefer him over a decoy far away. Snipers ignore decoys, and bosses with their own targeting are not covered.

### Requirements

- Marvel's Spider-Man Remastered, **`Spider-Man.exe` version exactly 4.0630.0.0**. On any other build the mod refuses to touch the game.
- Overstrike 1.8.1 with **Enable .script support** and **Add '-scripts' to commandline.txt** on.
- Do not install it together with Quantum Break Renewed or Hyper-Sense: both use the same keys and the same game fields.

### Known Issues

- The DISTORTION and FEAR looks crashed the game once (with a large burst of copies being removed just after slow motion ended). It has not been reproduced; if it happens to you, pick another look. No shipped preset uses them.
- At the slowest strength (0.001x) the game's clock barely moves, so the copies are held still more often than you might expect.
- Speed blur and speed streaks only show while Spider-Man is moving fast.
- The decoy's first frame may show for a moment on a slow machine; the `decoy_hide_spawn` setting in the ini's `[advanced]` section controls the trick that hides it.
- Presets cover Hyper-Sense only.

### Files

SHA-256 of EchoesOfTime.script: __SHA256__
