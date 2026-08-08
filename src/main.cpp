#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------------
// Experimental Bugfix's
//
// THE BUG: dense levels with a late StartPos (lots of accumulated group
// usage by the time the StartPos position is reached) would silently
// fail to open in normal mode - the loading circle finishes, then
// nothing happens, no crash, no error. Playtesting from the editor
// was unaffected.
//
// ROOT CAUSE (found via extensive hook-based diagnostics against the
// real GeometryDash.bro bindings): GJBaseGameLayer::shouldExitHackedLevel()
// returns true and the engine self-quits (via onQuit) as a safety
// mechanism, triggered once the highest group ID used in the level gets
// close to/at the internal group table's capacity (GJBaseGameLayer's
// m_groups vector, hardcoded to 10000 slots and iterated unchecked by
// sortAllGroupsX() - confirmed in the bindings). We verified m_groups
// itself is NOT undersized/out-of-bounds at that point, so this is a
// deliberate but overly conservative threshold check misfiring on
// legitimately large, dense levels - not a reaction to real corruption.
//
// THE FIX: override shouldExitHackedLevel()'s result to false, but ONLY
// when our own tracked max group ID is high enough (>= 9000) to match
// the exact condition we diagnosed - not a blanket suppression of every
// possible reason this function might fire.
// ---------------------------------------------------------------------

static bool debugOverlayEnabled() {
    return Mod::get()->getSettingValue<bool>("show-debug-overlay");
}

// ---------------------------------------------------------------------
// Optional on-screen debug overlay - only active if the user enables it
// in mod settings. Off by default for normal use.
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
// The actual fix
// ---------------------------------------------------------------------
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

        // Only override when we believe the high-group-count condition we
        // diagnosed is actually why this fired - not a blanket suppression.
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
            // make sure a previously-shown overlay doesn't linger if the
            // user disabled the setting mid-session
            DiagOverlay::get()->setHidden(true);
        }

        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }
};
