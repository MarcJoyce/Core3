global_rebel_rear_admiral_theater = Lair:new {
	mobiles = {
		{"fbase_rebel_elite_heavy_trooper_extreme",2},
		{"fbase_rebel_guard_captain_extreme",2},
		{"fbase_rebel_heavy_trooper_extreme",2},
		{"fbase_rebel_rifleman_extreme",2}
	},
	bossMobiles = {{"rebel_rear_admiral", 1}},
	spawnLimit = 15,
	buildingsVeryEasy = {"object/building/poi/anywhere_rebel_camp_small_1.iff","object/building/poi/anywhere_rebel_camp_small_2.iff","object/building/poi/anywhere_rebel_camp_small_3.iff"},
	buildingsEasy = {"object/building/poi/anywhere_rebel_camp_small_1.iff","object/building/poi/anywhere_rebel_camp_small_2.iff","object/building/poi/anywhere_rebel_camp_small_3.iff"},
	buildingsMedium = {"object/building/poi/anywhere_rebel_camp_small_1.iff","object/building/poi/anywhere_rebel_camp_small_2.iff","object/building/poi/anywhere_rebel_camp_small_3.iff"},
	buildingsHard = {"object/building/poi/anywhere_rebel_camp_small_1.iff","object/building/poi/anywhere_rebel_camp_small_2.iff","object/building/poi/anywhere_rebel_camp_small_3.iff"},
	buildingsVeryHard = {"object/building/poi/anywhere_rebel_camp_small_1.iff","object/building/poi/anywhere_rebel_camp_small_2.iff","object/building/poi/anywhere_rebel_camp_small_3.iff"},
	missionBuilding = "object/tangible/lair/base/objective_banner_rebel.iff",
	mobType = "npc",
	buildingType = "theater",
	faction = "rebel"
}

addLairTemplate("global_rebel_rear_admiral_theater", global_rebel_rear_admiral_theater)
