#pragma once
// QOL (PC addition): the player's character choice and levels that force a character.
//
// * Keeping the player's choice: the original selects a forcing level's character for good
//   (LevelB2D::addInfo -> Settings::setSelectedCharacterId), so character select afterwards starts
//   on that character. LevelB2D::addInfo / LevelSession::applyToSettings first remember the
//   player's own selection (rememberPlayerCharacter); it comes back when the player picks again
//   (CharacterSelectLayer), when a user level is left (main menu, online browser, Your Levels) and
//   when a user level that lets the player pick starts. In between the forced character stays
//   selected, as the game code reads the selection while the level runs (controls type, gore,
//   recorded browser replays, the level select's stamp after a campaign level).
//
// * "any character" (QoL page, UserDefault "qol_any_character", on by default): on user / online
//   levels (LevelSession chapter 5000 / 5001) that force a character, the pause and victory menus
//   offer CHANGE CHARACTER anyway (the original denies it), and so does the online browser before
//   playing. The level starts with its own character; one picked in character select replaces it
//   (characterOverride) on restarts, VIEW REPLAY and later character changes until another level
//   is selected (LevelSession::clearLevelData / setLevelDataWithManagedObject clear it). Campaign
//   levels and ghost races always keep their forced character.

namespace qol {

bool anyCharacterOnForcedLevels();
void setAnyCharacterOnForcedLevels(bool on);

// True when the running level's forced character may be changed (the option is on, a user level
// is selected and no ghost race runs).
bool canChangeForcedCharacter();

// Remembers the player's current selection unless one is already remembered (a forcing level is
// about to select its character).
void rememberPlayerCharacter();
// Selects the remembered character again (if any) and forgets it.
void restorePlayerCharacter();

// The character picked for the selected user level (0: none, the level's own).
int characterOverride();
void setCharacterOverride(int characterId);
// LevelB2D::addInfo: the character a forcing level starts with (its own, or the override).
int forcedLevelCharacter(int levelCharacterId);
// CharacterSelectLayer confirmed `characterId`: on a user level it also replaces a forced one.
void characterPicked(int characterId);

}  // namespace qol
