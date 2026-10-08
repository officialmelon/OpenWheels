// QOL (PC addition): see CharacterChoice.h.
#include "qol/CharacterChoice.h"

#include "cocos2d.h"

#include "LevelSession.h"
#include "Settings.h"
#include "net/race/RaceHooks.h"

USING_NS_CC;

namespace qol {
namespace {

const char* const kAnyCharacter = "qol_any_character";

int g_playerCharacter = 0;     // remembered selection (character id), 0 = none
int g_characterOverride = 0;   // character picked for the selected user level, 0 = none

}  // namespace

bool anyCharacterOnForcedLevels() { return UserDefault::getInstance()->getBoolForKey(kAnyCharacter, true); }
void setAnyCharacterOnForcedLevels(bool on) { UserDefault::getInstance()->setBoolForKey(kAnyCharacter, on); }

bool canChangeForcedCharacter()
{
    return anyCharacterOnForcedLevels() && LevelSession::getInstance()->isUserLevel() && !race::suppressVictoryMenu();
}

void rememberPlayerCharacter()
{
    if (g_playerCharacter == 0) g_playerCharacter = Settings::getInstance()->getSelectedCharacterId();
}

void restorePlayerCharacter()
{
    if (g_playerCharacter == 0) return;
    Settings::getInstance()->setSelectedCharacterId(g_playerCharacter);
    g_playerCharacter = 0;
}

int characterOverride() { return g_characterOverride; }
void setCharacterOverride(int characterId) { g_characterOverride = characterId > 0 ? characterId : 0; }

int forcedLevelCharacter(int levelCharacterId)
{
    // Only user levels: a campaign level never plays another character than its own.
    if (g_characterOverride > 0 && LevelSession::getInstance()->isUserLevel()) return g_characterOverride;
    return levelCharacterId;
}

void characterPicked(int characterId)
{
    // Also on levels that don't force their character: the override then equals the selection.
    if (canChangeForcedCharacter()) setCharacterOverride(characterId);
}

}  // namespace qol
