#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

static bool debugOverlayEnabled() {
    return Mod::get()->getSettingValue<bool>("show-debug-overlay");
}

class DiagOverlay : public CCLayer {
public:
    static DiagOverlay* get() {
        static DiagOverlay* instance = nullptr;
        if (!instance) {
            instance = DiagOverlay::create();
            if (instance) instance->retain();
        }
        return instance;
    }

    static DiagOverlay* create() {
        auto ret = new DiagOverlay();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() {
        if (!CCLayer::init()) return false;

        m_bg = CCLayerColor::create({0, 0, 0, 160}, 460, 420);
        m_bg->setPosition({4, CCDirector::sharedDirector()->getWinSize().height - 424});
        this->addChild(m_bg);

        m_label = CCLabelBMFont::create("Bugfix debug overlay", "chatFont.fnt");
        m_label->setAnchorPoint({0, 1});
        m_label->setPosition({8, CCDirector::sharedDirector()->getWinSize().height - 8});
        m_label->setScale(0.4f);
        m_label->setAlignment(kCCTextAlignmentLeft);
        this->addChild(m_label);

        this->setZOrder(999999);
        this->setTouchEnabled(false);
        this->ignoreAnchorPointForPosition(true);

        return true;
    }

    void pushLine(std::string const& line) {
        m_lines.push_back(line);
        while (m_lines.size() > 22) m_lines.erase(m_lines.begin());

        std::string joined;
        for (auto& l : m_lines) joined += l + "\n";
        if (m_label) m_label->setString(joined.c_str());
    }

    void attachToRunningScene() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene && this->getParent() != scene) {
            if (this->getParent()) this->removeFromParent();
            scene->addChild(this, 999999);
        }
    }

    void setHidden(bool hidden) {
        this->setVisible(!hidden);
    }

private:
    CCLayerColor* m_bg = nullptr;
    CCLabelBMFont* m_label = nullptr;
    std::vector<std::string> m_lines;
};

static void diagLog(std::string const& line) {
    log::info("[Bugfix] {}", line);
    if (!debugOverlayEnabled()) return;
    auto overlay = DiagOverlay::get();
    overlay->attachToRunningScene();
    overlay->setHidden(false);
    overlay->pushLine(line);
}

class $modify(BugfixGJBaseGameLayer, GJBaseGameLayer) {
    struct Fields {
        int m_maxGroupIDSeen = 0;
    };

    void addToGroup(GameObject* object, int groupID, bool triggerGroup) {
        if (groupID > m_fields->m_maxGroupIDSeen) {
            m_fields->m_maxGroupIDSeen = groupID;
        }
        GJBaseGameLayer::addToGroup(object, groupID, triggerGroup);
    }

    bool shouldExitHackedLevel() {
        bool result = GJBaseGameLayer::shouldExitHackedLevel();

        bool overriding = result && (m_fields->m_maxGroupIDSeen >= 9000);

        if (debugOverlayEnabled()) {
            diagLog(fmt::format("shouldExitHackedLevel() real={} maxGroupID={}{}",
                result, m_fields->m_maxGroupIDSeen,
                overriding ? " -> OVERRIDING to false (high group count fix)" : ""));
        }

        if (overriding) {
            return false;
        }
        return result;
    }
};

class $modify(BugfixPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (debugOverlayEnabled()) {
            diagLog(fmt::format("PlayLayer::init - lvl={} objCount={}",
                level ? level->m_levelName : "null",
                level ? level->m_objectCount.value() : -1));
            auto overlay = DiagOverlay::get();
            overlay->attachToRunningScene();
        } else if (DiagOverlay::get()->getParent()) {
            
            DiagOverlay::get()->setHidden(true);
        }

        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }
};
