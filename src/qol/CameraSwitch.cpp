#include "qol/CameraSwitch.h"

#include <vector>

#include "CharacterB2D.h"
#include "IrresponsibleDad.h"
#include "LevelB2D.h"
#include "MopedCouple.h"
#include "Session.h"
#include "Settings.h"
#include "StageCamera.h"
#include "restored/IrresponsibleMom.h"
#include "restored/SantaClaus.h"

namespace qol {

namespace {

std::vector<CharacterB2D*> targets()
{
    std::vector<CharacterB2D*> out;
    Session* session = Settings::getInstance()->getCurrentSession();
    LevelB2D* level = session ? session->getLevel() : nullptr;
    if (!level) return out;
    for (CharacterB2D* main : level->getCharacters())
    {
        if (!main) continue;
        out.push_back(main);
        auto add = [&out](CharacterB2D* other) {
            if (other && other->getFocus()) out.push_back(other);
        };
        if (auto* dad = dynamic_cast<IrresponsibleDad*>(main)) add(dad->cameraKid());
        if (auto* couple = dynamic_cast<MopedCouple*>(main)) add(couple->cameraGirl());
        if (auto* mom = dynamic_cast<IrresponsibleMom*>(main))
        {
            add(mom->cameraSon());
            add(mom->cameraDaughter());
        }
        if (auto* santa = dynamic_cast<SantaClaus*>(main))
        {
            add(santa->cameraElf(0));
            add(santa->cameraElf(1));
        }
    }
    return out;
}

}  // namespace

int cameraTargetCount()
{
    return (int)targets().size();
}

bool cycleCameraFocus()
{
    const std::vector<CharacterB2D*> all = targets();
    if (all.size() < 2) return false;
    Session* session = Settings::getInstance()->getCurrentSession();
    StageCamera* camera = session ? session->getCamera() : nullptr;
    if (!camera) return false;
    size_t current = 0;
    for (size_t i = 0; i < all.size(); i++)
    {
        if (all[i]->getFocus() == camera->getFocus())
        {
            current = i;
            break;
        }
    }
    for (size_t step = 1; step <= all.size(); step++)
    {
        CharacterB2D* next = all[(current + step) % all.size()];
        if (next->getFocus() && next->getFocus() != camera->getFocus())
        {
            camera->setFocus(next->getFocus());
            return true;
        }
    }
    return false;
}

}  // namespace qol
