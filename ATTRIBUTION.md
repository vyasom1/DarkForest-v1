# Asset attribution

## Death animations integrated 20 November 2026

Player `Hero_Death` is retargeted to Manny from Quaternius's Universal Animation Library Standard `Death01` (CC0-1.0), previously downloaded from https://quaternius.com/packs/universalanimationlibrary.html. Goblin death uses the existing `Die-jump-ground-face4_test` action from the Danimal/xGhostx7/Wind astella release credited below. DarkForest adds skeleton-compatible runtime playback and holds the ending pose. Source and derivative SHA-256 values are recorded in `docs/ASSET_MANIFEST.json`.

“Goblin animated by Motion capture” by Danimal is distributed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). The source listing credits xGhostx7 for the original [CC0 goblin model](https://www.blendswap.com/blends/view/72394) and Wind astella for the [CC BY knife model](https://opengameart.org/content/wind-weapon-pack-1). DarkForest has extracted the original Blender file without modifying it. [Release listing](https://lpc.opengameart.org/content/goblin-animated-by-motion-capture)

## Derived goblin FBX files

`RawAssets/Models/Goblin/Goblin_LOD0.fbx`, `Goblin_LOD1.fbx`, and `Goblin_LOD2.fbx` are derivatives of that CC BY 3.0 release, exported with Blender 4.2.9 LTS. Changes: scale normalized to 30.48 cm per Blender unit; LOD1 and LOD2 decimated to 50% and 25% of LOD0 triangles; for seven published actions, 18 finger-curve channels per action were removed because the staged 25-bone rig has no finger bones. Body motion is preserved and no action was deleted. The original `.blend` is unchanged. SHA-256 values are recorded in `docs/ASSET_MANIFEST.json`. The existing Quinn mesh and Mannequin AnimBP were not overwritten.
