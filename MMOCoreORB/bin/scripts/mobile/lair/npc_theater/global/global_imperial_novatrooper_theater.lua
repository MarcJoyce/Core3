global_imperial_novatrooper_theater = Lair:new {
	mobiles = {
		{"fbase_elite_dark_trooper_extreme",2},
		{"fbase_elite_sand_trooper_extreme",2},
		{"fbase_imperial_army_captain_extreme",2},
		{"fbase_imperial_exterminator_extreme",2}
	},
	bossMobiles = {{"elite_novatrooper_commander",1}},
	spawnLimit = 15,
	buildingsVeryEasy = {"object/building/poi/lok_imperial_large2.iff","object/building/poi/anywhere_imperial_base_large_1.iff"},
	buildingsEasy = {"object/building/poi/lok_imperial_large2.iff","object/building/poi/anywhere_imperial_base_large_1.iff"},
	buildingsMedium = {"object/building/poi/lok_imperial_large2.iff","object/building/poi/anywhere_imperial_base_large_1.iff"},
	buildingsHard = {"object/building/poi/lok_imperial_large2.iff","object/building/poi/anywhere_imperial_base_large_1.iff"},
	buildingsVeryHard = {"object/building/poi/lok_imperial_large2.iff","object/building/poi/anywhere_imperial_base_large_1.iff"},
	missionBuilding = "object/tangible/lair/base/objective_banner_imperial.iff",
	mobType = "npc",
	buildingType = "theater",
	faction = "imperial"
}

addLairTemplate("global_imperial_novatrooper_theater", global_imperial_novatrooper_theater)
