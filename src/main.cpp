#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/ui/Label.hpp>

#include <unordered_map>

using namespace geode::prelude;

// i wanted to have the levels from the sheet as well but there's no level id for it to pull from on the google sheet (probably not a good idea to pull from the sheet anyway)
enum class RatedPlusLevelStatus {
	InSheet,
	Polled,
	Selected,
	Placed,
	Legacy,
};

class RatedPlusLevelCache {
public:
	static RatedPlusLevelCache& getInstance() {
		static RatedPlusLevelCache cache;
		return cache;
	}

	RatedPlusLevelCache(const RatedPlusLevelCache&) = delete;
    RatedPlusLevelCache& operator=(const RatedPlusLevelCache&) = delete;

	bool contains(std::int32_t levelID) {
		return m_levelStatusMap.contains(levelID);
	}

	void insert(std::int32_t levelID, RatedPlusLevelStatus levelStatus) {
		m_levelStatusMap.insert({levelID, levelStatus});
	}

	RatedPlusLevelStatus get(std::int32_t levelID) {
		return m_levelStatusMap.at(levelID);
	}

	bool empty() {
		return m_levelStatusMap.empty();
	}

private:
	std::unordered_map<std::int32_t, RatedPlusLevelStatus> m_levelStatusMap;

	RatedPlusLevelCache() : m_levelStatusMap{} {}
	~RatedPlusLevelCache() {}

};

void drawDemon(GJGameLevel& level, CCLayer& layer) {
	//layer.getChildByID("difficulty-sprite")->setScale(0.5f);
	//layer.getChildByID("difficulty-sprite")->setPositionX(150);
	auto demonSprite{ CCSprite::create("ratedplusdemon.png"_spr) };
	demonSprite->setID("rated-plus-difficulty"_spr);
	demonSprite->setPosition({ 184.5, 216.0 });

	layer.addChild(demonSprite);
}

void drawServerDifficulty(GJGameLevel& level, CCLayer& layer) {
	layer.getChildByID("difficulty-sprite")->setVisible(false);
	
	GJDifficultySprite* serverDifficulty{ GJDifficultySprite::create(level.getAverageDifficulty(), GJDifficultyName::Short) };
	serverDifficulty->setID("server-difficulty"_spr);
	serverDifficulty->setPosition({150, 221});
	serverDifficulty->setScale(0.5f);

	layer.addChild(serverDifficulty);
}

void drawStatusLabel(GJGameLevel& level, CCLayer& layer) {
	RatedPlusLevelCache& statusCache {RatedPlusLevelCache::getInstance()};
	RatedPlusLevelStatus levelStatus {statusCache.get(level.m_levelID)};

	auto statusLabel { Label::create("bigFont.fnt") };
	statusLabel->setID("status-label"_spr);
	statusLabel->setScale(0.2f);

	if (levelStatus == RatedPlusLevelStatus::Placed) {
		statusLabel->setString("Status: Placed");	
	} else if (levelStatus == RatedPlusLevelStatus::Legacy) {
		statusLabel->setString("Status: Legacy");	
	}
	
	statusLabel->setPosition({ 184.5, 186.0 });
	layer.addChild(statusLabel);
}

void drawInfo(GJGameLevel& level, CCLayer& layer) {
	drawDemon(level, layer);	
	drawStatusLabel(level, layer);
	drawServerDifficulty(level, layer);
}

void insertLevelStatusFromJson(std::string&& rawJson, int32_t levelID) {
	//fsdafasdifajk may god help us all
	rawJson.erase(0, 1);
	rawJson.erase(rawJson.size()-1, 1);

	auto parsedJson{ matjson::parse(rawJson).unwrapOrDefault() };
	RatedPlusLevelCache& statusCache {RatedPlusLevelCache::getInstance()};

	if (parsedJson.contains("rated") && !parsedJson["rated"].asBool().unwrap()) {
		
		if (parsedJson["position"].asInt().unwrap() <= 150) {
			statusCache.insert(levelID, RatedPlusLevelStatus::Placed);
		} else {
			statusCache.insert(levelID, RatedPlusLevelStatus::Legacy);
		}
	}
}

class $modify(RPlusLevelInfoLayer, LevelInfoLayer) {
	struct Fields {
		async::TaskHolder<web::WebResponse> m_listListener;
	};

	bool init(GJGameLevel* level, bool challenge) {
		if (!LevelInfoLayer::init(level, challenge)) {
			return false;
		}

		auto& statusCache {RatedPlusLevelCache::getInstance()};

		if (!statusCache.empty() &&statusCache.contains(level->m_levelID)) {
			drawInfo(*level, *this);
			return true;
		}

		auto request = web::WebRequest();
		request.userAgent("ratedplusdemon on geode");
		request.acceptEncoding("application/json");

		m_fields->m_listListener.spawn(
			request.get(fmt::format("https://pointercrate.com/api/v2/demons/listed?level_id={}", level->m_levelID)),
			[this, &statusCache](web::WebResponse result) {
				insertLevelStatusFromJson(std::move(result.string().unwrapOrDefault()), this->m_level->m_levelID);

				if (statusCache.contains(this->m_level->m_levelID)) {
					drawInfo(*this->m_level, *this);
				}
			}
	    );
		return true;
	}
};

void drawDemonOnLevelCell(GJGameLevel* level, CCLayer& layer) {
	auto difficultyContainer = layer.getChildByIDRecursive("difficulty-container");

	CCSprite* demonSprite{ CCSprite::create("ratedplusdemon2.png"_spr) };
	demonSprite->setPosition({0, 0 });
	demonSprite->setID("rated-plus-difficulty"_spr);

	difficultyContainer->getChildByID("difficulty-sprite")->setVisible(false);
	difficultyContainer->addChild(demonSprite);
	difficultyContainer->updateLayout();
}

class $modify(RPlusLevelCell, LevelCell) {
	struct Fields {
		async::TaskHolder<web::WebResponse> m_listListener;
	};

	bool init() {
		if (!LevelCell::init()) {
			return false;
		}

		return true;
	}

	void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
		
		int levelID {level->m_levelID.value()};
		auto& statusCache {RatedPlusLevelCache::getInstance()};
		
		if (!statusCache.empty() &&statusCache.contains(level->m_levelID)) {
			drawDemonOnLevelCell(level, *this);
			return;
		}

		auto request = web::WebRequest();
		request.userAgent("ratedplusdemon on geode");
		request.acceptEncoding("application/json");

		m_fields->m_listListener.spawn(
			request.get(fmt::format("https://pointercrate.com/api/v2/demons/listed?level_id={}", level->m_levelID)),
			[level, this, &statusCache](web::WebResponse result) {
				insertLevelStatusFromJson(std::move(result.string().unwrapOrDefault()), this->m_level->m_levelID);

				if (statusCache.contains(level->m_levelID)) {
					drawDemonOnLevelCell(level, *this);
				}
			}
	    );
    }
};