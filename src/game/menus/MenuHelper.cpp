#include "MenuHelper.h"

USING_NS_CC;

// @005ecb24
void MenuHelper::loadSprites()
{
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    std::string plist = "menus/main/menu_main.plist";
    if (!cache->isSpriteFramesWithFileLoaded(plist))
    {
        cache->addSpriteFramesWithFile(plist);
    }
}

// @005ecbf8
Sprite* MenuHelper::addBg(Node* parent, int zOrder)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;

    Texture2D::PixelFormat previousFormat = Texture2D::getDefaultAlphaPixelFormat();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);

    Sprite* bg = Sprite::create("menus/menu_main_bg.png");
    bg->setAnchorPoint(Vec2(0.0f, 0.0f));
    bg->setScale(visibleSize.width / bg->getTextureRect().size.width,
                 visibleSize.height / bg->getTextureRect().size.height);
    parent->addChild(bg, zOrder);

    Texture2D::setDefaultAlphaPixelFormat(previousFormat);
    return bg;
}

// @005ecd40
LayerColor* MenuHelper::addOverlay(Node* parent, int zOrder)
{
    LayerColor* overlay = LayerColor::create(Color4B(0, 0, 0, 175));
    parent->addChild(overlay, zOrder);
    return overlay;
}

// @005ecdd0
MenuItemSprite* MenuHelper::addBackBtn(Node* parent, int zOrder, const std::function<void(Ref*)>& callback)
{
    loadSprites();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    std::string normalFrame = "menu_main_back_light.png";
    std::string selectedFrame = "menu_main_back_dark.png";

    MenuItemImage* btn = MenuItemImage::create("", "", callback);
    Sprite* normal = Sprite::createWithSpriteFrameName(normalFrame);
    Sprite* selected = Sprite::createWithSpriteFrameName(selectedFrame);
    Size size = normal->getContentSize();
    btn->setNormalImage(normal);
    btn->setSelectedImage(selected);
    btn->setPosition(Vec2(origin.x + size.width * 0.5f + 90.0f, origin.y + size.height * 0.5f + 90.0f));
    btn->setName("ow_back");  // PAD (PC addition): the controller's back button presses it (input/MenuFocus.h)

    Menu* menu = Menu::create(btn, nullptr);
    menu->setPosition(Vec2(0.0f, 0.0f));
    parent->addChild(menu, zOrder);
    return btn;
}

// @005ed078
MenuItemSprite* MenuHelper::addConfirmBtn(Node* parent, int zOrder, const std::function<void(Ref*)>& callback)
{
    loadSprites();
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    std::string normalFrame = "menu_main_confirm_light.png";
    std::string selectedFrame = "menu_main_confirm_dark.png";

    MenuItemImage* btn = MenuItemImage::create("", "", callback);
    Sprite* normal = Sprite::createWithSpriteFrameName(normalFrame);
    Sprite* selected = Sprite::createWithSpriteFrameName(selectedFrame);
    Size size = normal->getContentSize();
    btn->setNormalImage(normal);
    btn->setSelectedImage(selected);
    btn->setPosition(Vec2(origin.x + visibleSize.width - size.width * 0.5f - 90.0f,
                          origin.y + size.height * 0.5f + 90.0f));

    Menu* menu = Menu::create(btn, nullptr);
    menu->setPosition(Vec2(0.0f, 0.0f));
    parent->addChild(menu, zOrder);
    return btn;
}
