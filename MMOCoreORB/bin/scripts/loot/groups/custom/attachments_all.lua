attachments_all = {
	description = "",
	minimumLevel = 0,
	maximumLevel = -1,
	lootItems = {
		-- Common (1/2)
		{itemTemplate = "armor_attachments", chance = 3300000},
        {itemTemplate = "clothing_attachments", chance = 3400000},
        {itemTemplate = "jedi_clothing_attachments", chance = 3300000}
	}
}

addLootGroupTemplate("attachments_all", attachments_all)
