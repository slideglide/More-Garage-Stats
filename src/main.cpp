#include <Geode/binding/AchievementManager.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GameStatsManager.hpp>

#include <Geode/cocos/cocoa/CCDictionary.h>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/cocos/cocoa/CCString.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>

#include <Geode/modify/GameLevelManager.hpp>

#include <Geode/utils/ZStringView.hpp>
#include <Geode/utils/string.hpp>

#include <capeling.garage-stats-menu/include/stats_api.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

using namespace geode::prelude;

template <geode::utils::string::ConstexprString S, typename T>
const T& getSettingFast() {
    static T value = (
        geode::listenForSettingChanges<T>(S.data(), [](T val) {
            value = std::move(val);
        }),
        geode::getMod()->getSettingValue<T>(S.data())
    );
    return value;
}

struct StatItem {
    std::string itemID;
    ZStringView sprite;
    std::string_view setting;
    int number;
    float scale = 0.5f;
};

static int getEarnedAchievementCount() {
    auto am = AchievementManager::sharedState();
    if (!am || !am->m_allAchievements) return 0;

    static constexpr std::array<std::string_view, 3> kIgnorePrefixes = {
        "geometry.ach.world", "geometry.ach.subzero", "geometry.ach.md"
    };

    int earned = 0;
    for (auto object : am->m_allAchievements->asExt<CCObject>()) {
        auto dictionary = static_cast<CCDictionary*>(object);
        auto identifier = static_cast<CCString*>(dictionary->objectForKey("identifier"));
        if (!identifier) continue;

        const std::string_view str = identifier->getCString();
        bool ignored = std::ranges::any_of(kIgnorePrefixes, [&](std::string_view prefix) {
            return str.starts_with(prefix);
        });

        if (!ignored && am->isAchievementEarned(identifier->getCString())) {
            earned++;
        }
    }
    return earned;
}

static int getStat(ZStringView key) {
    auto gsm = GameStatsManager::sharedState();
    return gsm ? gsm->getStat(key.c_str()) : 0;
}

static auto makeItems() {
    const int bonus1 = std::min({
        getStat("16"), getStat("17"), getStat("18"), getStat("19"), getStat("20")
    });
    const int bonus2 = std::min({
        getStat("23"), getStat("24"), getStat("25"), getStat("26"), getStat("27")
    });

    return std::array{
        StatItem{"demons"_spr, "GJ_demonIcon_001.png", "demons", getStat("5")},
        StatItem{"demon-keys"_spr, "GJ_bigKey_001.png", "demon-keys", getStat("21"), 0.375f},
        StatItem{"gold-keys"_spr, "GJ_bigGoldKey_001.png", "gold-keys", getStat("43"), 0.375f},

        StatItem{"achievements"_spr, "rankIcon_top10_001.png", "achievements", getEarnedAchievementCount(), 0.5f},
        StatItem{"creator-points"_spr, "GJ_hammerIcon_001.png", "creator-points", 0, 0.5f},

        StatItem{"fire-shards"_spr, "fireShardBig_001.png", "shards", getStat("18"), 0.35f},
        StatItem{"ice-shards"_spr, "iceShardBig_001.png", "shards", getStat("19"), 0.35f},
        StatItem{"poison-shards"_spr, "poisonShardBig_001.png", "shards", getStat("17"), 0.35f},
        StatItem{"shadow-shards"_spr, "shadowShardBig_001.png", "shards", getStat("16"), 0.35f},
        StatItem{"lava-shards"_spr, "lavaShardBig_001.png", "shards", getStat("20"), 0.35f},
        StatItem{"bonus-shards-one"_spr, "bonusShardSmall_001.png", "completed-shards", bonus1, 0.5f},

        StatItem{"earth-shards"_spr, "shard0201ShardBig_001.png", "shards", getStat("23"), 0.35f},
        StatItem{"blood-shards"_spr, "shard0202ShardBig_001.png", "shards", getStat("24"), 0.35f},
        StatItem{"metal-shards"_spr, "shard0203ShardBig_001.png", "shards", getStat("25"), 0.35f},
        StatItem{"light-shards"_spr, "shard0204ShardBig_001.png", "shards", getStat("26"), 0.35f},
        StatItem{"soul-shards"_spr, "shard0205ShardBig_001.png", "shards", getStat("27"), 0.35f},
        StatItem{"bonus-shards-two"_spr, "bonusShard2Small_001.png", "completed-shards", bonus2, 0.5f},
    };
}

static void registerItem(const StatItem& item) {
    stats_api::registerStatItem(
        item.itemID,
        [sprite = item.sprite]() {
            return CCSprite::createWithSpriteFrameName(sprite.c_str());
        },
        item.number,
        item.scale
    );
}

static void applyRegistrationForSetting(std::string_view settingKey, bool enabled) {
    for (const auto& item : makeItems()) {
        if (item.setting != settingKey) continue;

        if (enabled) {
            registerItem(item);
            stats_api::setDisplayedNumber(item.itemID, item.number);
        } else {
            stats_api::unregisterStatItem(item.itemID);
        }
    }
}

void updateCreatorPointsUI(int creatorPoints) {
    if (!getSettingFast<"creator-points", bool>()) return;
    stats_api::setDisplayedNumber("creator-points"_spr, creatorPoints);
}

// this is untested, if it doesn't work then womp womp
// tried not to use manual web requests purely to support GDPS
static void fetchAndDisplayCreatorPoints() {
    auto gjam = GJAccountManager::get();
    auto glm = GameLevelManager::get();
    if (!gjam || !glm || gjam->m_accountID <= 0) return;

    if (auto cachedScore = glm->userInfoForAccountID(gjam->m_accountID)) {
        updateCreatorPointsUI(cachedScore->m_creatorPoints);
    }

    glm->getGJUserInfo(gjam->m_accountID);
}

$on_mod(Loaded) {
    for (const auto& item : makeItems()) {
        if (Mod::get()->getSettingValue<bool>(item.setting)) {
            registerItem(item);
        }
    }

    if (getSettingFast<"creator-points", bool>()) {
        fetchAndDisplayCreatorPoints();
    }

    for (const auto& key : Mod::get()->getSettingKeys()) {
        listenForSettingChanges<bool>(key, [key](bool enabled) {
            applyRegistrationForSetting(key, enabled);
            if (key == "creator-points" && enabled) {
                fetchAndDisplayCreatorPoints();
            }
        });
    }
}

class $modify(GameLevelManager) {
    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        GameLevelManager::onGetGJUserInfoCompleted(response, tag);

        auto gjam = GJAccountManager::get();
        if (!gjam || gjam->m_accountID <= 0) return;

        if (auto score = this->userInfoForAccountID(gjam->m_accountID)) {
            updateCreatorPointsUI(score->m_creatorPoints);
        }
    }
};