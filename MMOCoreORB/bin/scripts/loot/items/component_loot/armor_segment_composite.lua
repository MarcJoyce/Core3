armor_segment_composite = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "",
	directObjectTemplate = "object/tangible/component/armor/armor_segment_composite.iff",
	craftingValues = {
		{"hit_points",1000,1000,0, true},
		{"armor_effectiveness",5,10,10},
		{"armor_integrity",500,1000,0},
		{"armor_health_encumbrance",13,1,0},
		{"armor_action_encumbrance",13,1,0},
		{"armor_mind_encumbrance",16,1,0},
		{"armor_rating",1,1,0, true},
		{"armor_special_type",256,256,0, true},
		{"armor_special_effectiveness",5,15,0},
		{"armor_special_integrity",100,100,0, true},
		{"useCount",11,22,0},
	},
	customizationStringName = {},
	customizationValues = {}
}


addLootItemTemplate("armor_segment_composite", armor_segment_composite)
