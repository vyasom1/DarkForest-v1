# DarkForest release verification

## Evidence carried forward

- Unreal Engine 5.3 Development Editor compiled.
- In Play In Editor, New Game played the Veo opening and handed off to the tutorial. A scripted run reached the Goblin King, triggered the finale on the third deathblow, and reached the Complete state.
- The above editor run used injected dialogue inputs, teleports, and scripted damage.

## Windows package

The Windows Shipping package is being prepared. The cook commandlet currently terminates during global shader compilation in this environment; the package will be attached to the v1.0.0 release once the cook succeeds and the playthrough checks pass.

## Public release

The source-review snapshot is published separately from the Windows ZIP. Keep local saves, generated files, unused source packs, and development credentials out of the public repository.