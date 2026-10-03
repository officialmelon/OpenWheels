#include "DebugLayer.h"

#include "AdController.h"
#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "UserProgress.h"
#include "Vehicle.h"

#include "Box2D/Box2D.h"

#include <sstream>

USING_NS_CC;
USING_NS_CC_EXT;

namespace
{
// Tag of the command label inside each table cell.
const int kCellLabelTag = 911;
} // namespace

// Cell indices (= _labels order):
//   0 complete Most Levels   1 level victory      2 scale session up   3 scale session down
//   4 character test func    5 debug draw         6 drag               7 stats
//   8 gravity                9 slow mo            10 impulse           11..26 joint breaks / smashes
//   27 random                28 clear IAP

// @005a879c
DebugLayer::DebugLayer()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    _cellHeight = 80.0f;
    _cellWidth = 800.0f;
    _tableHeight = visibleSize.height * 0.7f;
    Session* session = Settings::getInstance()->getCurrentSession();
    _unk0x348 = 1.0f;
    _impulse = -1.0f;

    _labels.push_back("complete Most Levels");
    _labels.push_back("level victory");
    _labels.push_back("scale session up");
    _labels.push_back("scale session down");
    _labels.push_back("character test func");
    if (!session->debugDrawVisible())
    {
        _labels.push_back("debug draw: OFF");
    }
    else
    {
        _labels.push_back("debug draw: ON");
    }
    if (!session->getDrag())
    {
        _labels.push_back("drag: OFF");
    }
    else
    {
        _labels.push_back("drag: ON");
    }
    if (!Director::getInstance()->isDisplayStats())
    {
        _labels.push_back("stats: OFF");
    }
    else
    {
        _labels.push_back("stats: ON");
    }
    if (session->getWorld()->GetGravity().y == 0.0f)
    {
        _labels.push_back("gravity: OFF");
    }
    else
    {
        _labels.push_back("gravity: ON");
    }
    if (Settings::getInstance()->getCurrentSession()->getTimeStep() == 1.0f / 60.0f)
    {
        _labels.push_back("slow mo: OFF");
    }
    else
    {
        _labels.push_back("slow mo: ON");
    }
    _labels.push_back("impulse: WEAK");
    _labels.push_back("neck Break");
    _labels.push_back("shoulder Break 1");
    _labels.push_back("shoulder Break 2");
    _labels.push_back("elbow Break 1");
    _labels.push_back("elbow Break 2");
    _labels.push_back("torso Break");
    _labels.push_back("hip Break 1");
    _labels.push_back("hip Break 2");
    _labels.push_back("knee Break 1");
    _labels.push_back("knree Break 2");
    _labels.push_back("head Smash");
    _labels.push_back("chest Smash");
    _labels.push_back("pelvis Smash");
    _labels.push_back("foot 1 Smash");
    _labels.push_back("foot 2 Smash");
    _labels.push_back("helmet smash");
    _labels.push_back("random");
    _labels.push_back("clear IAP");

    addMenu();
}

// @005a97e0
void DebugLayer::addMenu()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    float width = _cellWidth;
    float height = _tableHeight;

    TableView* tableView = TableView::create(this, Size(width, height));
    tableView->setDataSource(this);
    tableView->setViewSize(Size(width, height));
    tableView->setPosition(origin.x + visibleSize.width * 0.5f - width * 0.5f,
                           origin.y + visibleSize.height * 0.5f - height * 0.5f);
    tableView->setDirection(ScrollView::Direction::VERTICAL);
    tableView->setVerticalFillOrder(TableView::VerticalFillOrder::TOP_DOWN);
    tableView->setDelegate(this);

    LayerColor* background = LayerColor::create(Color4B(0, 0, 0, 125), width, height);
    background->setPosition(tableView->getPosition());
    addChild(background);
    addChild(tableView);
    tableView->reloadData();
}

// @005a9a00 (D1), @005a9aa4 (D0); thunks @005a9a94, @005a9a9c, @005a9ac8, @005a9af0
DebugLayer::~DebugLayer()
{
}

// @005a9b18 (thunk @005aa804)
void DebugLayer::tableCellTouched(TableView* table, TableViewCell* cell)
{
    ssize_t idx = cell->getIdx();
    cell->setColor(Color3B::BLUE);

    switch (idx)
    {
    case 0:
        Settings::getInstance()->getUserProgress()->addDebugCompletionTimes();
        break;
    case 1:
        Settings::getInstance()->getCurrentSession()->getLevel()->levelCompleted();
        break;
    case 2:
    {
        Session* session = Settings::getInstance()->getCurrentSession();
        session->setScale(session->getScale() + 0.1f);
        break;
    }
    case 3:
    {
        Session* session = Settings::getInstance()->getCurrentSession();
        session->setScale(session->getScale() + -0.1f);
        break;
    }
    case 4:
        Settings::getInstance()->getCurrentSession()->getLevel()->getCharacter()->debugFunc(0.0f, 0.0f, 0.0f);
        break;
    case 5:
    {
        Session* session = Settings::getInstance()->getCurrentSession();
        bool visible = session->debugDrawVisible();
        session->setDebugDrawVisible(!visible);
        std::string text;
        text = visible ? "debug draw: OFF" : "debug draw: ON";
        static_cast<Label*>(cell->getChildByTag(kCellLabelTag))->setString(text);
        break;
    }
    case 6:
    {
        Session* session = Settings::getInstance()->getCurrentSession();
        bool drag = session->getDrag();
        session->setDrag(!drag);
        Label* label = static_cast<Label*>(cell->getChildByTag(kCellLabelTag));
        if (!drag)
        {
            label->setString("drag: ON");
        }
        else
        {
            label->setString("drag: OFF");
        }
        break;
    }
    case 7:
    {
        Director* director = Director::getInstance();
        bool displayStats = director->isDisplayStats();
        director->setDisplayStats(!displayStats);
        Label* label = static_cast<Label*>(cell->getChildByTag(kCellLabelTag));
        if (!displayStats)
        {
            label->setString("stats: ON");
        }
        else
        {
            label->setString("stats: OFF");
        }
        break;
    }
    case 8:
    {
        b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();
        float gravityY = world->GetGravity().y;
        Label* label = static_cast<Label*>(cell->getChildByTag(kCellLabelTag));
        if (gravityY == 0.0f)
        {
            world->SetGravity(b2Vec2(0.0f, -10.0f));
            label->setString("gravity: ON");
        }
        else
        {
            world->SetGravity(b2Vec2(0.0f, 0.0f));
            label->setString("gravity: OFF");
        }
        break;
    }
    case 9:
    {
        float timeStep = Settings::getInstance()->getCurrentSession()->getTimeStep() == 1.0f / 60.0f
                             ? 1.0f / 120.0f
                             : 1.0f / 60.0f;
        Settings::getInstance()->getCurrentSession()->setTimeStep(timeStep);
        std::stringstream stream;
        stream << timeStep;
        static_cast<Label*>(cell->getChildByTag(kCellLabelTag))->setString("Toggle Slow Mo: " + stream.str());
        break;
    }
    case 10:
    {
        const char* text;
        if (_impulse == -1.0f)
        {
            _impulse = 1.0f;
            text = "impulse: STRONG";
        }
        else
        {
            _impulse = -1.0f;
            text = "impulse: WEAK";
        }
        std::string labelText;
        labelText.assign(text);
        static_cast<Label*>(cell->getChildByTag(kCellLabelTag))->setString(labelText);
        break;
    }
    case 11:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugNeckBreak(_impulse);
        }
        break;
    }
    case 12:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugShoulderBreak1(_impulse);
        }
        break;
    }
    case 13:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugShoulderBreak2(_impulse);
        }
        break;
    }
    case 14:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugElbowBreak1(_impulse);
        }
        break;
    }
    case 15:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugElbowBreak2(_impulse);
        }
        break;
    }
    case 16:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugTorsoBreak(_impulse);
        }
        break;
    }
    case 17:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugHipBreak1(_impulse);
        }
        break;
    }
    case 18:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugHipBreak2(_impulse);
        }
        break;
    }
    case 19:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugKneeBreak1(_impulse);
        }
        break;
    }
    case 20:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugKneeBreak2(_impulse);
        }
        break;
    }
    case 21:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugHeadSmash(_impulse);
        }
        break;
    }
    case 22:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugChestSmash(_impulse);
        }
        break;
    }
    case 23:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugPelvisSmash(_impulse);
        }
        break;
    }
    case 24:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugFoot1Smash(_impulse);
        }
        break;
    }
    case 25:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugFoot2Smash(_impulse);
        }
        break;
    }
    case 26:
    {
        std::vector<CharacterB2D*> characters = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->debugHelmetSmash(_impulse);
        }
        break;
    }
    case 27:
    {
        Vehicle* vehicle = Settings::getInstance()->getCurrentSession()->getLevel()->getCharacter()->getVehicle();
        if (vehicle)
        {
            vehicle->debugFunction(0);
        }
        break;
    }
    case 28:
    {
        UserDefault* userDefault = UserDefault::getInstance();
        userDefault->setBoolForKey("remove_ads", false);
        userDefault->flush();
        Settings::getInstance()->getAdController()->setAdsRemoved(false);
        break;
    }
    default:
        break;
    }
}

// @005aa80c (thunk @005aaaa8)
TableViewCell* DebugLayer::tableCellAtIndex(TableView* table, ssize_t idx)
{
    TableViewCell* cell = table->dequeueCell();
    if (!cell)
    {
        cell = TableViewCell::create();
        Label* label = Label::createWithTTF(getLabel(idx), "fonts/Arial.ttf", 60.0f);
        label->setTag(kCellLabelTag);
        label->setAlignment(TextHAlignment::CENTER);
        label->setAnchorPoint(Vec2(0.5f, 0.5f));
        // Unused.
        Size labelSize = label->getContentSize();
        (void)labelSize;
        label->setPosition(_cellWidth * 0.5f, _cellHeight * 0.5f);
        cell->addChild(label);
    }
    else
    {
        Label* label = static_cast<Label*>(cell->getChildByTag(kCellLabelTag));
        label->setString(getLabel(idx));
    }
    return cell;
}

// @005aaa94
std::string DebugLayer::getLabel(ssize_t idx)
{
    return _labels[idx];
}

// @005aaab0 (thunk @005aaac0)
Size DebugLayer::tableCellSizeForIndex(TableView* table, ssize_t idx)
{
    return Size(_cellWidth, _cellHeight);
}

// @005aaad0 (thunk @005aaaf0)
ssize_t DebugLayer::numberOfCellsInTableView(TableView* table)
{
    return _labels.size();
}
