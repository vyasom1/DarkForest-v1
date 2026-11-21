# DarkForest story cinematics

New Game plays the 30-second Veo opening through the festival, the raid, Lio's loss and the survivors' camp. The illustrated tutorial scene introduces Mara and Iven before the player talks to them. Mara teaches attack and guard. Iven introduces timed deflection. E jumps to the tutorial during the opening movie and V skips the cinematic. Existing checkpoints continue from their saved stage.

The King's final deathblow starts the ending. The rebuilt-village illustration leads into a 10-second Veo clip of the hero with Lio's ribbon in the lantern-lit village. The story then saves completion and returns the player to the village.

The six AI-assisted illustrations remain available if video playback fails. The game loads the original Veo picture and audio tracks from `Content/Movies`. The 30-second export `d456362e` covers the same story beats as the shorter `7a411eec` cut and continues into the camp. `f9f5f5d0` is byte-identical to `d456362e`. The separate `c1b28d7e` clip supplies the finale.

## Verification

- UE 5.3 MyProjectEditor Win64 Development built successfully.
- Saw both Veo clips play in Play In Editor and confirmed they finished at their expected durations. Using injected inputs and scripted damage, completed both tutorial conversations, cleared the raid and forest encounters, activated the shrine, and triggered the ending on the third King deathblow. Confirmed that the story advanced to Complete after the finale.
- Restored the user's original editor-play save and confirmed that the standalone save was unchanged by comparing SHA-256 hashes.
- A conventional playthrough, packaged Windows playback and packaged Continue are pending. The user's earlier progression problem remains unconfirmed.
