#pragma once
// NET (PC addition): the ghost race's entry points for game code (MainMenu, Gameplay). Implemented in
// RaceSession.cpp; editor-less verification builds link stubs (tools/parity/EditorlessStubs.cpp).

namespace race {

// Main menu "Race": the open race's lobby, or a new race on a campaign level.
void openRaceMenu();
// True while a ghost race runs: Gameplay skips its VictoryMenu (the race HUD shows the finish).
bool suppressVictoryMenu();

}  // namespace race
