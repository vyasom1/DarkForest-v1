# DarkForest V1

**DarkForest V1** is the first public portfolio build of an ongoing Unreal Engine 5.3 and C++ third-person melee project. You play a hero defending a forest village, learning to deflect goblin attacks, and finally confronting the Goblin King in a three-phase boss fight.

## Download (Windows)

- **Release:** [DarkForest v1.0.0 — Windows x64](https://github.com/vyasom1/DarkForest-v1/releases/download/v1.0.0/DarkForest-v1-Windows-x64.zip)
- **How to launch:**
  1. Download `DarkForest-v1-Windows-x64.zip` and extract it to a folder of your choice.
  2. Run `MyProject.exe` (or `MyProject-Win64-Shipping.exe` once the Shipping package is published) from the extracted folder. No Unreal Engine installation is required.

> Note: the Windows Shipping package is being prepared and will be attached to the v1.0.0 release. The source snapshot in this repository is available for review today.

## Gameplay screenshot

![DarkForest gameplay](docs/shots/darkforest-portfolio.png)

## Controls

| Input | Action |
| --- | --- |
| WASD, mouse | Move and look |
| Left mouse button | Attack |
| Right mouse button | Guard |
| F | Timed deflect |
| Q | Lock on |
| E | Interact, advance scenes, or deathblow when prompted |
| V | Skip a scene |
| P | Pause |

## What I implemented (C++)

- Three-phase boss encounter with segmented health, posture breaks, and deathblow-driven phase transitions.
- Posture and health HUD, windup/recovery cues, and punish windows.
- Combat tutorial, checkpoint saves, Continue, and skippable opening/ending cinematics.

Direct links to the C++:

| System | Source |
| --- | --- |
| Phase transitions, posture and deathblows | [GoblinKingPhaseComponent.cpp](Source/MyProject/Enemy/GoblinKingPhaseComponent.cpp) |
| Attack selection, windup and recovery | [GoblinKing.cpp](Source/MyProject/Enemy/GoblinKing.cpp) |
| Player input, lock-on and combat feedback | [DarkForestHero.cpp](Source/MyProject/Player/DarkForestHero.cpp) |
| Segmented health, posture and phase UI | [BossHealthWidget.cpp](Source/MyProject/UI/BossHealthWidget.cpp) |
| Story progression and cinematics | [DarkForestStory.cpp](Source/MyProject/World/DarkForestStory.cpp) |
| Checkpoint and Continue state | [DemoFrontendState.cpp](Source/MyProject/UI/DemoFrontendState.cpp) |
| Boss regression tests | [BossAutomationTests.cpp](Source/MyProject/Tests/BossAutomationTests.cpp) |

## Design decision: posture break → deathblow → phase advance

The Goblin King has three health segments and a posture meter. Taking posture damage fills the meter; when it breaks, the King staggers and a deathblow window opens. Landing the deathblow removes the current health segment and advances the boss to the next phase (new attacks, faster cadence). This makes timing and defense matter: a patient player creates the opening, then spends it to progress the fight.

## Credits

- Om Vyas developed the project and integrated the gameplay, art, and cinematics.
- Opening and ending cinematics use Google Gemini/Veo video; illustrated fallbacks are included.
- Third-party character, animation, environment, and audio sources are documented in [ATTRIBUTION.md](ATTRIBUTION.md) and [docs/ASSET_MANIFEST.json](docs/ASSET_MANIFEST.json).

## What's next

- Further combat polish (hit-stop, camera feel, additional enemy variety).
- A gameplay reel showing a timed deflect, a posture break, and a deathblow that changes the phase.
- Measured performance work on a GTX 1650 target (30 FPS at 1920×1080, 75% render scale).