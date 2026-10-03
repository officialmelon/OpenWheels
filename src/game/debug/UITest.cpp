#include "UITest.h"

#include "GameText.h"

#include "ui/UIScrollView.h"

USING_NS_CC;

// @00637324
UITest::UITest()
{
}

// @00637354 (D2), @00637368 (D0)
UITest::~UITest()
{
}

// @0063738c
Scene* UITest::createScene()
{
    Scene* scene = Scene::create();
    UITest* layer = UITest::create();
    scene->addChild(layer);
    return scene;
}

// @00637434
bool UITest::init()
{
    if (!Layer::init())
    {
        return false;
    }
    Size visibleSize = Director::getInstance()->getVisibleSize();
    addChild(LayerColor::create(Color4B(50, 50, 50, 255), visibleSize.width, visibleSize.height));
    scrollableNodeExample();
    return true;
}

// @006374dc
void UITest::scrollableNodeExample()
{
    Vec2 scrollViewPosition = Vec2(100.0f, 100.0f);

    LayerColor* background = LayerColor::create(Color4B::ORANGE, 520.0f, 520.0f);
    background->setAnchorPoint(Vec2::ZERO);
    background->setPosition(Vec2(90.0f, 90.0f));
    addChild(background);

    Label* label = Label::createWithTTF(OW_GAMETEXT(uitest_scroll_text, 0x003fbde7), "fonts/Arial Bold.ttf", 50.0f);
    label->setDimensions(500.0f, 0.0f);
    label->setAnchorPoint(Vec2::ZERO);
    label->setTextColor(Color4B::BLACK);
    float labelHeight = label->getContentSize().height;

    ui::ScrollView* scrollView = ui::ScrollView::create();
    scrollView->setScrollBarColor(Color3B::WHITE);
    scrollView->setBounceEnabled(true);
    scrollView->setContentSize(Size(500.0f, 500.0f));
    scrollView->setPosition(scrollViewPosition);
    scrollView->setInnerContainerSize(Size(500.0f, labelHeight));
    scrollView->addChild(label);
    addChild(scrollView);
}

// @00637774
void UITest::centerNodeToScreen(Node* node)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    node->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f));
}

// @006377e8
void UITest::labelWrapExample()
{
    Label* label = Label::createWithTTF(OW_GAMETEXT(uitest_wrap_text, 0x003f5fad), "fonts/Arial Bold.ttf", 50.0f);
    label->setDimensions(200.0f, 0.0f);
    Size visibleSize = Director::getInstance()->getVisibleSize();
    label->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f));
    addChild(label);
}
