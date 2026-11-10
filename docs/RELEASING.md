# Releasing

Releases are GitHub Releases with one asset per mod: `EndlessArena.script` or `ManhattanSkies.script` (the Overstrike package built by the mod's `pack.ps1`). Alphas and betas are marked **pre-release**. The commands below use Endless Arena; Manhattan Skies is the same with its own paths (`ManhattanSkies/mod/build.cmd`, `ManhattanSkies/mod/pack.ps1`, `ManhattanSkies/docs/releases/`).

## Checklist

1. `git status` is clean and `main` is pushed.
2. `EndlessArena\build.cmd` prints `BUILD OK`. The `.script` must be built from the commit you tag: check that no file in `src/` is newer than
   `build\EndlessArena.dll`.
3. Test the exact `.script` you will upload: install it with Overstrike into the game and play one Campaign and one Horde run.
4. Update the version in `EndlessArena/pack.ps1` (`info.json` inside the package) if it changed. Keep it plain `x.y.z`; Overstrike compares versions.
5. Write the notes in `EndlessArena/docs/releases/<tag>.md` (what's in it, known issues with issue numbers, what is untested). Leave the line `SHA-256 of ...: __SHA256__` as is.
6. Create the release as a **draft** first, review it on GitHub, then publish:

```
sha256sum EndlessArena/build/EndlessArena.script
sed "s/__SHA256__/<hash>/" EndlessArena/docs/releases/v0.1.0-alpha.1.md > notes.md        (don't commit notes.md)
gh release create v0.1.0-alpha.1 EndlessArena/build/EndlessArena.script --draft --prerelease --target main --title "EndlessArena 0.1.0 alpha 1" --notes-file notes.md
```

Publishing the draft creates the tag at `main`. To see/edit/publish: the repository's **Releases** page, or `gh release edit <tag> --draft=false`.

## Tag names

Endless Arena (the first mod here): `v<major>.<minor>.<patch>[-alpha.N | -beta.N]`, for example `v0.2.0-beta.1`. **Every other mod is prefixed with its name**: `manhattan-skies-v0.1.0-alpha.1`. (The unprefixed `v0.1.0-alpha.1` already belongs to Endless Arena; reusing a tag would collide, which nearly happened with Manhattan Skies' first alpha.) The game build the release targets is stated in the notes; if a future release supports another build, say
so in the title.
