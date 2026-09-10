#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <capeling.garage-stats-menu/include/stats_api.hpp>

using namespace geode::prelude;

template <geode::utils::string::ConstexprString S, typename T>
    T const& getSettingFast() {
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

    constexpr std::array<std::string_view, 3> kIgnorePrefixes = {
        "geometry.ach.world", "geometry.ach.subzero", "geometry.ach.md"};

    int earned = 0;
    for (auto object : am->m_allAchievements->asExt<CCObject>()) {
        auto dictionary = static_cast<CCDictionary*>(object);
        for (const auto& [key, val] : dictionary->asExt<std::string, CCString>()) {
            if (key != "identifier") continue;

            const std::string_view str = val->getCString();
            bool ignored = std::ranges::any_of(
                kIgnorePrefixes, [&](std::string_view prefix) { return str.starts_with(prefix); });

            if (!ignored && am->isAchievementEarned(val->getCString())) {
                earned++;
            }
        }
    }
    return earned;
}

static int getStat(ZStringView key) {
    return GameStatsManager::sharedState()->getStat(key.c_str());
}

static std::vector<StatItem> makeItems(bool checkSettings = true) {
    const int bonus1 =
        std::min({getStat("16"), getStat("17"), getStat("18"), getStat("19"), getStat("20")});
    const int bonus2 =
        std::min({getStat("23"), getStat("24"), getStat("25"), getStat("26"), getStat("27")});

    const std::array items = {
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

    if (!checkSettings) return {items.begin(), items.end()};

    std::vector<StatItem> enabledItems;
    for (const auto& item : items) {
        if (Mod::get()->getSettingValue<bool>(item.sprite)) {
            enabledItems.push_back(item);
        }
    }
    return enabledItems;
}

static void registerItem(const StatItem& item) {
    stats_api::registerStatItem(
        item.itemID,
        [sprite = item.sprite]() {
            return CCSprite::createWithSpriteFrameName(sprite.c_str());
        },
        item.number,
        item.scale);
}

static void unregisterItem(const StatItem& item) {
    stats_api::unregisterStatItem(item.itemID);
}

static void applyRegistrationForSetting(std::string_view settingKey, bool enabled) {
    for (const auto& item : makeItems(false)) {
        if (item.setting != settingKey) continue;
        if (enabled) {
            registerItem(item);
        } else {
            unregisterItem(item);
        }
    }
}

void updateCreatorPointsUI(int creatorPoints) {
    if (!Mod::get()->getSettingValue<bool>("creator-points")) return;
    stats_api::setDisplayedNumber("creator-points"_spr, creatorPoints);
}

static void fetchAndDisplayCreatorPoints() {
    auto gjam = GJAccountManager::get();
    auto glm = GameLevelManager::get();
    if (!gjam || !glm || gjam->m_accountID == 0) return;

    if (auto cachedScore = glm->userInfoForAccountID(gjam->m_accountID)) {
        updateCreatorPointsUI(cachedScore->m_creatorPoints);
    } else {
        glm->getGJUserInfo(gjam->m_accountID);
    }
}

$on_mod(Loaded) {
    for (const auto& item : makeItems()) {
        registerItem(item);
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
        if (!gjam) return;

        auto score = GameLevelManager::get()->userInfoForAccountID(gjam->m_accountID);
        if (score && score->m_accountID == gjam->m_accountID) {
            updateCreatorPointsUI(score->m_creatorPoints);
        }
    }
};

$on_game(Loaded) {
    if (!Mod::get()->getSettingValue<bool>("creator-points")) return;
    fetchAndDisplayCreatorPoints();
}
