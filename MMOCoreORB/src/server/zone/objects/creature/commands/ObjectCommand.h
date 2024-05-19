/*
Copyright <SWGEmu>
See file COPYING for copying conditions.*/

#ifndef OBJECTCOMMAND_H_
#define OBJECTCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/loot/LootManager.h"
#include "server/zone/managers/crafting/CraftingManager.h"
#include "server/zone/managers/crafting/ComponentMap.h"
#include "server/zone/objects/tangible/terminal/characterbuilder/CharacterBuilderTerminal.h"
#include "server/zone/packets/scene/AttributeListMessage.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "templates/tangible/SharedWeaponObjectTemplate.h"
#include "server/zone/objects/tangible/pharmaceutical/StimPack.h"
#include "server/zone/objects/tangible/pharmaceutical/RangedStimPack.h"
#include "server/zone/objects/tangible/pharmaceutical/EnhancePack.h"
#include "server/zone/objects/tangible/pharmaceutical/WoundPack.h"
#include "server/zone/objects/tangible/pharmaceutical/CurePack.h"
#include "server/zone/objects/tangible/pharmaceutical/DotPack.h"


class ObjectCommand : public QueueCommand {
public:

	ObjectCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		StringTokenizer args(arguments.toString());

		try {
			String commandType;
			args.getStringToken(commandType);

			if (commandType.beginsWith("createitem")) {
				String objectTemplate;
				args.getStringToken(objectTemplate);

				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

				if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
					creature->sendSystemMessage("Templates must be tangible objects, or descendants of tangible objects, only.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Your inventory is full, so the item could not be created.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<TangibleObject*> object = (server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1)).castTo<TangibleObject*>();

				if (object == nullptr) {
					creature->sendSystemMessage("The object '" + commandType + "' could not be created because the template could not be found.");
					return INVALIDPARAMETERS;
				}

				Locker locker(object);

				object->createChildObjects();

				// Set Crafter name and generate serial number
				// String name = "Generated with Object Command";
				// object->setCraftersName(name);

				StringBuffer customName;
				// customName << object->getDisplayedName() <<  " (System Generated)";
				customName << object->getDisplayedName();

				object->setCustomObjectName(customName.toString(), false);

				String serial = craftingManager->generateSerial();
				object->setSerialNumber(serial);

				int quantity = 1;

				if (args.hasMoreTokens())
					quantity = args.getIntToken();

				if(quantity > 1 && quantity <= 100)
					object->setUseCount(quantity);

				// load visible components
				while (args.hasMoreTokens()) {
					String visName;
					args.getStringToken(visName);

					uint32 visId = visName.hashCode();
					if (ComponentMap::instance()->getFromID(visId).getId() == 0)
						continue;

					object->addVisibleComponent(visId, false);
				}

				if (inventory->transferObject(object, -1, true)) {
					inventory->broadcastObject(object, true);
					creature->info(true) << "/object createitem " << objectTemplate << " created oid: " << object->getObjectID() << " \"" << object->getDisplayedName() << "\"";
				} else {
					object->destroyObjectFromDatabase(true);
					creature->sendSystemMessage("Error transferring object to inventory.");
				}
			} else if (commandType.beginsWith("createloot")) {
				String lootGroup;
				args.getStringToken(lootGroup);

				int level = 1;

				if (args.hasMoreTokens())
					level = args.getIntToken();

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Your inventory is full, so the item could not be created.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();

				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				TransactionLog trx(TrxCode::ADMINCOMMAND, creature);
				trx.addState("commandType", commandType);
				if (lootManager->createLoot(trx, inventory, lootGroup, level) > 0) {
					creature->info(true) << "/object creatloot " << lootGroup << " trxId: " << trx.getTrxID();
					trx.commit(true);
				} else {
					trx.abort() << "createLoot failed for lootGroup " << lootGroup << " level " << level;
				}
			} else if (commandType.beginsWith("createresource")) {
				String resourceName;
				args.getStringToken(resourceName);

				int quantity = 100000;

				if (args.hasMoreTokens())
					quantity = args.getIntToken();

				ManagedReference<ResourceManager*> resourceManager = server->getZoneServer()->getResourceManager();
				resourceManager->givePlayerResource(creature, resourceName, quantity);
			} else if (commandType.beginsWith("createarealoot")) {
				String lootGroup;
				args.getStringToken(lootGroup);

				int range = 32;
				if (args.hasMoreTokens())
					range = args.getIntToken();

				if( range < 0 )
					range = 32;

				if( range > 128 )
					range = 128;

				int level = 1;
				if (args.hasMoreTokens())
					level = args.getIntToken();

				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();
				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				Zone* zone = creature->getZone();
				if (zone == nullptr)
					return GENERALERROR;

				// Find all objects in range
				SortedVector<TreeEntry*> closeObjects;
				CloseObjectsVector* closeObjectsVector = (CloseObjectsVector*) creature->getCloseObjects();
				if (closeObjectsVector == nullptr) {
					zone->getInRangeObjects(creature->getPositionX(), creature->getPositionZ(), creature->getPositionY(), range, &closeObjects, true);
				} else {
					closeObjectsVector->safeCopyTo(closeObjects);
				}

				// Award loot group to all players in range
				for (int i = 0; i < closeObjects.size(); i++) {
					SceneObject* targetObject = static_cast<SceneObject*>(closeObjects.get(i));

					if (targetObject->isPlayerCreature() && creature->isInRange(targetObject, range)) {

						CreatureObject* targetPlayer = cast<CreatureObject*>(targetObject);
						Locker tlock( targetPlayer, creature );

						ManagedReference<SceneObject*> inventory = targetPlayer->getSlottedObject("inventory");
						if (inventory != nullptr) {
							TransactionLog trx(creature, targetPlayer, nullptr, TrxCode::ADMINCOMMAND);
							trx.addState("commandType", commandType);
							if (lootManager->createLoot(trx, inventory, lootGroup, level) > 0) {
								creature->info(true) << "/object creatlootarea " << lootGroup << " trxId: " << trx.getTrxID();
								trx.commit(true);
								targetPlayer->sendSystemMessage( "You have received a loot item!");
							} else {
								trx.abort() << "createLoot failed for lootGroup " << lootGroup << " level " << level;
							}
						}

						tlock.release();
					}
				}
			} else if (commandType.beginsWith("checklooted")) {
				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();
				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				creature->sendSystemMessage("Number of Legendaries Looted: " + String::valueOf(lootManager->getLegendaryLooted()));
				creature->sendSystemMessage("Number of Exceptionals Looted: " + String::valueOf(lootManager->getExceptionalLooted()));
				creature->sendSystemMessage("Number of Magical Looted: " + String::valueOf(lootManager->getYellowLooted()));

			} else if (commandType.beginsWith("characterbuilder")) {
				if (!ConfigManager::instance()->getBool("Core3.CharacterBuilderEnabled", true)) {
					creature->sendSystemMessage("characterbuilder is not enabled on this server.");
					return GENERALERROR;
				}

				ZoneServer* zserv = server->getZoneServer();

				String blueFrogTemplate = "object/tangible/terminal/terminal_character_builder.iff";
				ManagedReference<CharacterBuilderTerminal*> blueFrog = ( zserv->createObject(blueFrogTemplate.hashCode(), 0)).castTo<CharacterBuilderTerminal*>();

				if (blueFrog == nullptr)
					return GENERALERROR;

				Locker clocker(blueFrog, creature);

				float x = creature->getPositionX();
				float y = creature->getPositionY();
				float z = creature->getPositionZ();

				ManagedReference<SceneObject*> parent = creature->getParent().get();

				blueFrog->initializePosition(x, z, y);
				blueFrog->setDirection(creature->getDirectionW(), creature->getDirectionX(), creature->getDirectionY(), creature->getDirectionZ());

				if (parent != nullptr && parent->isCellObject())
					parent->transferObject(blueFrog, -1);
				else
					creature->getZone()->transferObject(blueFrog, -1, true);

				creature->info(true) << "/object characterbuilder " << " created oid: " << blueFrog->getObjectID() << " \"" << blueFrog->getDisplayedName() << "\" as " << creature->getWorldPosition() << " on " << creature->getZone()->getZoneName();
			} else if (commandType.beginsWith("generateweapon")) {
				ManagedReference<TangibleObject*> object = nullptr;
				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Inventory is full");
					return INVALIDPARAMETERS;
				}

				//START
				String objectTemplate;
				args.getStringToken(objectTemplate);

				Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

				if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
					creature->sendSystemMessage("Invalid object template");
					return INVALIDPARAMETERS;
				}

				object = server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1).castTo<TangibleObject*>();

				if (!object->isWeaponObject()) {
					creature->sendSystemMessage("Not a weapon");
					return INVALIDPARAMETERS;
				}

				bool errors = false;

				int damageType = 1, apType = 0, condition = 1000, maxCondition = 1000, 
					skillModValue01 = 0, skillModValue02 = 0, skillModValue03 = 0, skillModValue04 = 0, 
					pbacc = 0, idealacc = 0, mracc = 0, idealrange = 6, maxRange = 64, 
					hac = 100, aac = 100, mac = 100, dotType = 0, dotAttribute = 0, dotStrength = 0, 
					dotDuration = 0, dotPotency = 0, dotUses = 0;
				float speed = 1.0, minDamage = 100, maxDamage = 100, wound = 0, forceCost = 1.0;
				String skillModType01 = "none", skillModType02 = "none", skillModType03 = "none", skillModType04 = "none";

				// Damage Type
					// KINETIC = 1, ENERGY = 2, BLAST = 4, STUN = 8, LIGHTSABER = 16
					// HEAT = 32, COLD = 64, ACID = 128, ELECTRICITY = 256
				validateDamageTypeErrors(&args, creature, &errors, &damageType);

				// AP
				if (args.hasMoreTokens()) apType = args.getIntToken();
				if (apType < 0 || apType > 3) {
					creature->sendSystemMessage("Invalid armor piercing (0-3)");
					errors = true;
				}

				// Condition
				validateIntTokenErrors(&args, creature, &errors, &condition, 1, 999999, "Condition");
				validateIntTokenErrors(&args, creature, &errors, &maxCondition, condition, 999999, "Max Condition");

				// Skill Mods
				validateSkillModTokens(&args, creature, &errors, &skillModType01);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue01, 0, 25, "Skill Mod #1 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType02);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue02, 0, 25, "Skill Mod #2 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType03);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue03, 0, 25, "Skill Mod #3 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType04);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue04, 0, 25, "Skill Mod #4 Value");

				// Damage Values
				validateFloatTokenErrors(&args, creature, &errors, &speed, 0.1, 16, "Attack Speed", 1);
				validateFloatTokenErrors(&args, creature, &errors, &minDamage, 1, 999999, "Min Damage", 0);
				validateFloatTokenErrors(&args, creature, &errors, &maxDamage, minDamage, 999999, "Max Damage", 0);
				validateFloatTokenErrors(&args, creature, &errors, &wound, 0, 999, "Wounds", 2);

				// Range Modifiers
				validateIntTokenErrors(&args, creature, &errors, &pbacc, -50, 100, "Point Blank Acc");
				validateIntTokenErrors(&args, creature, &errors, &idealacc, -50, 100, "Ideal Acc");
				validateIntTokenErrors(&args, creature, &errors, &mracc, -50, 100, "Max Range Acc");

				// Range Distance
				validateIntTokenErrors(&args, creature, &errors, &idealrange, 0, 255, "Ideal Range");
				validateIntTokenErrors(&args, creature, &errors, &maxRange, 0, 255, "Max Range");

				// Special Attack Cost
				validateIntTokenErrors(&args, creature, &errors, &hac, 0, 9999, "Health Attack Cost");
				validateIntTokenErrors(&args, creature, &errors, &aac, 0, 9999, "Action Attack Cost");
				validateIntTokenErrors(&args, creature, &errors, &mac, 0, 9999, "Mind Attack Cost");

				// Force Cost for Lightsaber
				validateFloatTokenErrors(&args, creature, &errors, &forceCost, 0, 999, "Force Cost", 2);

				// Dots
				validateIntTokenErrors(&args, creature, &errors, &dotStrength, 0, 999999, "DoT Strength");
				validateIntTokenErrors(&args, creature, &errors, &dotDuration, 0, 999999, "DoT Duration");
				validateIntTokenErrors(&args, creature, &errors, &dotPotency, 0, 999999, "DoT Potency");
				validateIntTokenErrors(&args, creature, &errors, &dotUses, 0, 999999, "DoT Uses");
				validateIntTokenErrors(&args, creature, &errors, &dotType, 0, 4, "DoT Type");
				validateIntTokenErrors(&args, creature, &errors, &dotAttribute, 0, 8, "DoT Attribute");

				// If errors, do not generate weapon
				if (errors) return GENERALERROR;

				Locker locker(object);

				object->createChildObjects();

				// Set weapon name
				StringBuffer customName;
				customName << object->getDisplayedName();
				object->setCustomObjectName(customName.toString(), false);

				// Serial number
				object->setSerialNumber(craftingManager->generateSerial());

				// Set condition if less than max
				object->setMaxCondition(maxCondition);
				if (condition < maxCondition)
					object->setConditionDamage(maxCondition - condition);
				
				if (skillModType01 != "none" && skillModValue01 > 0)
					object->addSkillMod(SkillModManager::WEARABLE, skillModType01, skillModValue01);
				if (skillModType02 != "none" && skillModValue02 > 0)
					object->addSkillMod(SkillModManager::WEARABLE, skillModType02, skillModValue02);
				if (skillModType03 != "none" && skillModValue03 > 0)
					object->addSkillMod(SkillModManager::WEARABLE, skillModType03, skillModValue03);
				if (skillModType04 != "none" && skillModValue04 > 0)
					object->addSkillMod(SkillModManager::WEARABLE, skillModType04, skillModValue04);

				ManagedReference<WeaponObject*> weaponObj = (object).castTo<WeaponObject*>();
				weaponObj->setDamageType(damageType);
				weaponObj->setArmorPiercing(apType);
				weaponObj->setAttackSpeed(speed);
				weaponObj->setMinDamage(minDamage);
				weaponObj->setMaxDamage(maxDamage);
				weaponObj->setWoundsRatio(wound);
								
				weaponObj->setPointBlankAccuracy(pbacc);
				weaponObj->setIdealAccuracy(idealacc);
				weaponObj->setMaxRangeAccuracy(mracc);

				weaponObj->setIdealRange(idealrange);
				weaponObj->setMaxRange(maxRange);

				weaponObj->setHealthAttackCost(hac);
				weaponObj->setActionAttackCost(aac);
				weaponObj->setMindAttackCost(mac);

				if (weaponObj->isJediWeapon())
					weaponObj->setForceCost(forceCost);

				if (dotType > 0 && dotType < 5 && dotAttribute >= 0 && dotAttribute <= 8 && dotStrength > 0 && dotDuration > 0 && dotPotency > 0 && dotUses > 0) {
					weaponObj->addDotType(dotType);
					weaponObj->addDotAttribute(dotAttribute);
					weaponObj->addDotStrength(dotStrength);
					weaponObj->addDotDuration(dotDuration);
					weaponObj->addDotPotency(dotPotency);
					weaponObj->addDotUses(dotUses);
				}

				if (inventory->transferObject(weaponObj, -1, true))
					inventory->broadcastObject(weaponObj, true);
				else {
					weaponObj->destroyObjectFromDatabase(true);
				}

			} else if (commandType.beginsWith("generatearmor")) {
				ManagedReference<TangibleObject*> object = nullptr;
				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Inventory is full");
					return INVALIDPARAMETERS;
				}

				bool errors = false;

				int apType = 0, condition = 1000, maxCondition = 1000, sockets = 0,
					skillModValue01 = 0, skillModValue02 = 0, skillModValue03 = 0, skillModValue04 = 0, 
					healthEncumbrance = 0, actionEncumbrance = 0, mindEncumbrance = 0;
				float kinetic = 10.0, energy = 10.0, blast = 10.0, heat = 10.0,
					cold = 10.0, acid = 10.0, electricity = 10.0, stun = 10.0, lightSaber = 10.0;
				String skillModType01 = "none", skillModType02 = "none", skillModType03 = "none", skillModType04 = "none";

				// AP
				if (args.hasMoreTokens()) apType = args.getIntToken();
				if (apType < 0 || apType > 3) {
					// StringIdChatParameter errMsgAP("custom/commands/generateEquipment", "invalid_armor_piercing");
					// errMsgAP.setDI(apType);
					// creature->sendSystemMessage(errMsgAP);
					errors = true;
				}

				// Condition
				validateIntTokenErrors(&args, creature, &errors, &condition, 1, 999999, "Condition");
				validateIntTokenErrors(&args, creature, &errors, &maxCondition, condition, 999999, "Max Condition");

				// Sockets
				validateIntTokenErrors(&args, creature, &errors, &sockets, 0, 4, "Sockets");

				// Skill Mods
				validateSkillModTokens(&args, creature, &errors, &skillModType01);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue01, 0, 25, "Skill Mod #1 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType02);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue02, 0, 25, "Skill Mod #2 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType03);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue03, 0, 25, "Skill Mod #3 Value");
				validateSkillModTokens(&args, creature, &errors, &skillModType04);
				validateIntTokenErrors(&args, creature, &errors, &skillModValue04, 0, 25, "Skill Mod #4 Value");

				// Armor Resists
					// Note: ArmorObjectImplementation::getTypeValue() hard caps armor values. Default 80
				validateFloatTokenErrors(&args, creature, &errors, &kinetic, 0.0, 100.0, "Kinetic", 1);
				validateFloatTokenErrors(&args, creature, &errors, &energy, 0.0, 100.0, "Energy", 1);
				validateFloatTokenErrors(&args, creature, &errors, &blast, 0.0, 100.0, "Blast", 1);
				validateFloatTokenErrors(&args, creature, &errors, &heat, 0.0, 100.0, "Heat", 1);
				validateFloatTokenErrors(&args, creature, &errors, &cold, 0.0, 100.0, "Cold", 1);
				validateFloatTokenErrors(&args, creature, &errors, &acid, 0.0, 100.0, "Acid", 1);
				validateFloatTokenErrors(&args, creature, &errors, &electricity, 0.0, 100.0, "Electricity", 1);
				validateFloatTokenErrors(&args, creature, &errors, &stun, 0.0, 100.0, "Stun", 1);
				validateFloatTokenErrors(&args, creature, &errors, &lightSaber, 0.0, 100.0, "Lightsaber", 1);

				// Encumbrance
				validateIntTokenErrors(&args, creature, &errors, &healthEncumbrance, 0, 1000, "Health Encumbrance");
				validateIntTokenErrors(&args, creature, &errors, &actionEncumbrance, 0, 1000, "Action Encumbrance");
				validateIntTokenErrors(&args, creature, &errors, &mindEncumbrance, 0, 1000, "Mind Encumbrance");

				// If errors, do not generate armor
				if (errors) return GENERALERROR;

				object = nullptr;
				bool armorError = false;
				while (args.hasMoreTokens()) {
					object = getObjectTemplate(&args, creature);
					if (object != nullptr) {
						armorError = generateArmorObject(object, creature, craftingManager, inventory, apType, condition, maxCondition, sockets, skillModType01, skillModValue01, skillModType02, skillModValue02, skillModType03, skillModValue03, skillModType04, skillModValue04, kinetic, energy, blast, heat, cold, acid, electricity, stun, lightSaber, healthEncumbrance, actionEncumbrance, mindEncumbrance);
						if (armorError)
							break;
					}
				}

			} else if (commandType.beginsWith("generatewearable")) {
				ManagedReference<TangibleObject*> object = nullptr;
				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Inventory is full");
					return INVALIDPARAMETERS;
				}

				String objectTemplate;
				args.getStringToken(objectTemplate);

				Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

				if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
					// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_object_tangible");
					return INVALIDPARAMETERS;
				}
			
				object = server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1).castTo<TangibleObject*>();

				// Invalid template
				if (object == nullptr) {
					// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_template");
					// errMsgTemplate.setTT(objectTemplate);
					// creature->sendSystemMessage(errMsgTemplate);
					return INVALIDPARAMETERS;
				}

				// Invalid object wearable
				if (!object->isWearableObject()) {
					// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_wearable");
					// errMsgTemplate.setTT(objectTemplate);
					// creature->sendSystemMessage(errMsgTemplate);
					return INVALIDPARAMETERS;
				}

				// Log any errors
				bool errors = false;

				int condition = 1000, maxCondition = 1000, sockets = 0;

				// Condition
				validateIntTokenErrors(&args, creature, &errors, &condition, 1, 999999, "Condition");
				validateIntTokenErrors(&args, creature, &errors, &maxCondition, condition, 999999, "Max Condition");

				// Sockets
				validateIntTokenErrors(&args, creature, &errors, &sockets, 0, 4, "Sockets");

				// If errors, do not generate armor
				if (errors)
					return GENERALERROR;

				Locker locker(object);

				object->createChildObjects();

				// Set wearable name
				StringBuffer customName;
				customName << object->getDisplayedName();
				object->setCustomObjectName(customName.toString(), false);

				// Serial number
				object->setSerialNumber(craftingManager->generateSerial());

				// Set condition if less than max
				object->setMaxCondition(maxCondition);
				if (condition < maxCondition)
					object->setConditionDamage(maxCondition - condition);

				while (args.hasMoreTokens())
					addWearableSkillMods(&args, creature, object);

				ManagedReference<WearableObject*> wearableObj = object.castTo<WearableObject*>();
				// Set Sockets
				wearableObj->setMaxSockets(sockets);

				// Set wearable to no trade (Need to update repo)
				//ManagedReference<SceneObject*> wearableSceneObj = wearableObj.castTo<SceneObject*>();
				//wearableSceneObj->setForceNoTrade(true);

				if (inventory->transferObject(wearableObj, -1, true))
					inventory->broadcastObject(wearableObj, true);
				else {
					wearableObj->destroyObjectFromDatabase(true);
					// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax_wearable");
				}
			} else if (commandType.beginsWith("generatemedicine")) {
				ManagedReference<TangibleObject*> object = nullptr;
				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Inventory is full");
					return INVALIDPARAMETERS;
				}

				String objectTemplate;
				args.getStringToken(objectTemplate);

				Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

				if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
					// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_object_tangible");
					return INVALIDPARAMETERS;
				}
			
				object = server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1).castTo<TangibleObject*>();

				// Invalid template
				if (object == nullptr) {
					// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_template");
					// errMsgTemplate.setTT(objectTemplate);
					// creature->sendSystemMessage(errMsgTemplate);
					return INVALIDPARAMETERS;
				}

				// Invalid object pharmaceutical
				if (!object->isPharmaceuticalObject()) {
					// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_pharmaceutical");
					// errMsgTemplate.setTT(objectTemplate);
					// creature->sendSystemMessage(errMsgTemplate);
					return INVALIDPARAMETERS;
				}

				PharmaceuticalObject* pharmaObj = object.castTo<PharmaceuticalObject*>();

				// Pet or Droid stimpack, TBD if included.
				if (pharmaObj->isPetStimPack() || pharmaObj->isDroidRepairKit()) {
					// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_pharmaceutical_pet_droid");
					// errMsgTemplate.setTT(objectTemplate);
					// creature->sendSystemMessage(errMsgTemplate);
					return INVALIDPARAMETERS;
				}

				// Log any errors
				bool errors = false;

				int useCount = 10, effectiveness = 500, medUse = 5, range = 15, area = 0, duration = 1800, potency = 350, absorption = 0, attribute = -1;

				// Generate pharmaceutical
				if (pharmaObj->isRangedStimPack()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");
					validateIntTokenErrors(&args, creature, &errors, &range, 1, 256, "Range");

					// If errors, do not generate
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					RangedStimPack* rangedStimObj = object.castTo<RangedStimPack*>();

					// Set Ranged Stimpack atts
					rangedStimObj->setEffectiveness(effectiveness);
					rangedStimObj->setMedicineUseRequired(medUse);
					rangedStimObj->setRange(range);

					if (pharmaObj->isArea()) {
						validateIntTokenErrors(&args, creature, &errors, &area, 1, 256, "Area");
						if (errors) return GENERALERROR;
						rangedStimObj->setArea(area);
					}

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				} else if (pharmaObj->isStimPack()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");

					// If errors, do not generate
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					StimPack* stimObj = object.castTo<StimPack*>();

					// Set Stimpack atts
					stimObj->setEffectiveness(effectiveness);
					stimObj->setMedicineUseRequired(medUse);

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				} else if (pharmaObj->isCurePack()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");

					// If errors, do not generate
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					CurePack* curePackObj = object.castTo<CurePack*>();

					// Set Stimpack atts
					curePackObj->setEffectiveness(effectiveness);
					curePackObj->setMedicineUseRequired(medUse);

					if (curePackObj->isArea()) {
						args.shiftTokens(1);
						validateIntTokenErrors(&args, creature, &errors, &area, 1, 256, "Area");
						if (errors) return GENERALERROR;
						curePackObj->setArea(area);
					}

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				} else if (pharmaObj->isEnhancePack()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");
					args.shiftTokens(2);
					validateIntTokenErrors(&args, creature, &errors, &duration, 1, 999999, "Duration");

					if (args.hasMoreTokens()) {
						args.shiftTokens(2);
						if (args.hasMoreTokens())
							validateIntTokenErrors(&args, creature, &errors, &attribute, -1, 8, "Attribute");
					}

					// If errors, do not generate
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					EnhancePack* enhancePackObj = object.castTo<EnhancePack*>();

					// Set Ranged Stimpack atts
					enhancePackObj->setEffectiveness(effectiveness);
					enhancePackObj->setMedicineUseRequired(medUse);
					enhancePackObj->setDuration(duration);

					if (enhancePackObj->getAbsorption() > 0) {
						args.shiftTokens(2);
						validateIntTokenErrors(&args, creature, &errors, &absorption, 0, 256, "Absorption");
						if (errors) return GENERALERROR;
						enhancePackObj->setAbsorbtion(absorption);
					}

					if (attribute > -1)
						enhancePackObj->setAttribute(attribute);

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				} else if (pharmaObj->asSceneObject()->isDotPackObject()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");
					validateIntTokenErrors(&args, creature, &errors, &range, 1, 256, "Range");
					validateIntTokenErrors(&args, creature, &errors, &area, 0, 256, "Area");
					validateIntTokenErrors(&args, creature, &errors, &duration, 1, 999999, "Duration");
					validateIntTokenErrors(&args, creature, &errors, &potency, 1, 999999, "Potency");

					// If errors, do not generate armor
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					DotPack* dotPackObj = object.castTo<DotPack*>();

					// Set Ranged Stimpack atts
					dotPackObj->setEffectiveness(effectiveness);
					dotPackObj->setMedicineUseRequired(medUse);
					dotPackObj->setRange(range);
					dotPackObj->setDuration(duration);
					dotPackObj->setPotency(potency);

					if (dotPackObj->isArea() && area > 0)
						dotPackObj->setArea(area);

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				} else if (pharmaObj->isWoundPack()) {
					validateIntTokenErrors(&args, creature, &errors, &useCount, 1, 999, "Use Count");
					validateIntTokenErrors(&args, creature, &errors, &effectiveness, 1, 999999, "Effectiveness");
					validateIntTokenErrors(&args, creature, &errors, &medUse, 1, 125, "Medical Use");

					// If errors, do not generate
					if (errors) return GENERALERROR;

					Locker locker(object);

					// Set Use Count
					object->setUseCount(useCount);

					WoundPack* woundPackObj = object.castTo<WoundPack*>();

					// Set Stimpack atts
					woundPackObj->setEffectiveness(effectiveness);
					woundPackObj->setMedicineUseRequired(medUse);

					generateObject(object, creature, craftingManager, inventory, "invalid_syntax_pharmaceutical");
				}
			}

		} catch (Exception& e) {
			creature->sendSystemMessage("SYNTAX: /object createitem <objectTemplatePath> [<quantity>]");
			creature->sendSystemMessage("SYNTAX: /object createresource <resourceName> [<quantity>]");
			creature->sendSystemMessage("SYNTAX: /object createloot <loottemplate> [<level>]");
			creature->sendSystemMessage("SYNTAX: /object createarealoot <loottemplate> [<range>] [<level>]");
			creature->sendSystemMessage("SYNTAX: /object checklooted");
			creature->sendSystemMessage("SYNTAX: /object characterbuilder");

			return INVALIDPARAMETERS;
		}

		return SUCCESS;
	}

	void generateObject(TangibleObject* object, CreatureObject* creature, CraftingManager* craftingManager, SceneObject* inventory, String invalid) const {
		object->createChildObjects();

		// Set object name
		StringBuffer customName;
		customName << object->getDisplayedName();
		object->setCustomObjectName(customName.toString(), false);

		// Serial number
		object->setSerialNumber(craftingManager->generateSerial());

		// Set object to no trade (Need to update repo)
		//ManagedReference<SceneObject*> object = wearableObj.castTo<SceneObject*>();
		//object->setForceNoTrade(true);

		if (inventory->transferObject(object, -1, true))
			inventory->broadcastObject(object, true);
		else {
			object->destroyObjectFromDatabase(true);
			// creature->sendSystemMessage("@custom/commands/generateEquipment:" + invalid);
		}
	}

	ManagedReference<TangibleObject*> getObjectTemplate(StringTokenizer* args, CreatureObject* creature) const {
		String objectTemplate;
		args->getStringToken(objectTemplate);

		Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

		if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_object_tangible");
			return nullptr;
		}

		ManagedReference<TangibleObject*> object = server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1).castTo<TangibleObject*>();

		// Invalid armor template
		if (object == nullptr) {
			// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_template");
			// errMsgTemplate.setTT(objectTemplate);
			// creature->sendSystemMessage(errMsgTemplate);
			return nullptr;
		}

		return object;
	}

	void addWearableSkillMods(StringTokenizer* args, CreatureObject* creature, TangibleObject* object) const {
		bool skillModError = false;
		String skillModType = "none";
		int skillModValue = 0;

		// Skill Mods
		validateSkillModTokens(args, creature, &skillModError, &skillModType);
		validateIntTokenErrors(args, creature, &skillModError, &skillModValue, 0, 25, "Skill Mod Value");

		if (!skillModError && skillModType != "none" && skillModValue > 0)
			object->addSkillMod(SkillModManager::WEARABLE, skillModType, skillModValue);
	}

	bool generateArmorObject(TangibleObject* object, CreatureObject* creature, CraftingManager* craftingManager, SceneObject* inventory, int apType, int condition, int maxCondition, int sockets,
	 String skillModType01, int skillModValue01, String skillModType02, int skillModValue02, String skillModType03, int skillModValue03, String skillModType04, int skillModValue04, 
	 float kinetic, float energy, float blast, float heat, float cold, float acid, float electricity, float stun, float lightSaber, 
	 int healthEncumbrance, int actionEncumbrance, int mindEncumbrance) const {
		if (!object->isWearableObject()) {
			// StringIdChatParameter errMsgTemplate("custom/commands/generateEquipment", "invalid_object_armor");
			// errMsgTemplate.setTT(object->getObjectTemplate()->getFullTemplateString());
			// creature->sendSystemMessage(errMsgTemplate);
			return true;
		}

		Locker locker(object);

		object->createChildObjects();

		// Set armor name
		StringBuffer customName;
		customName << object->getDisplayedName();
		object->setCustomObjectName(customName.toString(), false);

		// Serial number
		object->setSerialNumber(craftingManager->generateSerial());

		// Set condition if less than max
		object->setMaxCondition(maxCondition);
		if (condition < maxCondition)
			object->setConditionDamage(maxCondition - condition);
		
		if (!object->isPsgArmorObject()) {
			ManagedReference<WearableObject*> wearableObj = cast<WearableObject*>(object);
			wearableObj->setMaxSockets(sockets);

			if (skillModType01 != "none" && skillModValue01 > 0)
				object->addSkillMod(SkillModManager::WEARABLE, skillModType01, skillModValue01);
			if (skillModType02 != "none" && skillModValue02 > 0)
				object->addSkillMod(SkillModManager::WEARABLE, skillModType02, skillModValue02);
			if (skillModType03 != "none" && skillModValue03 > 0)
				object->addSkillMod(SkillModManager::WEARABLE, skillModType03, skillModValue03);
			if (skillModType04 != "none" && skillModValue04 > 0)
				object->addSkillMod(SkillModManager::WEARABLE, skillModType04, skillModValue04);
		}

		if (object->isArmorObject()) {
			ManagedReference<ArmorObject*> armorObj = cast<ArmorObject*>(object);

			armorObj->setRating(apType);

			armorObj->setKinetic(kinetic);
			armorObj->setEnergy(energy);
			armorObj->setBlast(blast);
			armorObj->setHeat(heat);
			armorObj->setCold(cold);
			armorObj->setAcid(acid);
			armorObj->setElectricity(electricity);
			armorObj->setStun(stun);
			armorObj->setLightSaber(lightSaber);
							
			armorObj->setHealthEncumbrance(healthEncumbrance);
			armorObj->setActionEncumbrance(actionEncumbrance);
			armorObj->setMindEncumbrance(mindEncumbrance);
	 	}

		if (inventory->transferObject(object, -1, true)) {
			inventory->broadcastObject(object, true);
		} else {
			object->destroyObjectFromDatabase(true);
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax_armor");
		}

		return false;
	}

	// Validate that weapon damage type if valid
	void validateDamageTypeErrors(StringTokenizer* args, CreatureObject* creature, bool* errors, int* damageType) const {
		bool found = false;
		if (args->hasMoreTokens()) 
			*damageType = args->getIntToken();
		else {
			*errors = true;
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax");
			return;
		}

		int damageTypes[9] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
		for (int i = 0; i < 8; ++i) {
			if (damageTypes[i] == *damageType) {
				found = true;
				break;
			}
		}

		if (found == false) {
			// StringIdChatParameter errMsgDamage("custom/commands/generateEquipment", "invalid_damage_type");
			// errMsgDamage.setDI(*damageType);
			// creature->sendSystemMessage(errMsgDamage);
			*errors = true;
		}
	}

	// Validate all integer inputs
	void validateIntTokenErrors(StringTokenizer* args, CreatureObject* creature, bool* errors, int* value, int min, int max, String tokenType) const {
		// Validate more tokens
		if (args->hasMoreTokens()) 
			*value = args->getIntToken();
		else {
			*errors = true;
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax");
			return;
		}

		// Validate Argument
		if (*value < min) {
			*errors = true;
			// StringIdChatParameter errMsg("custom/commands/generateEquipment", "invalid_int_low");
			// errMsg.setTT(tokenType);
			// errMsg.setDI(*value);
			// errMsg.setTU(String::valueOf(min));
			// creature->sendSystemMessage(errMsg);
		} else if (*value > max) {
			*errors = true;
			// StringIdChatParameter errMsg("custom/commands/generateEquipment", "invalid_int_high");
			// errMsg.setTT(tokenType);
			// errMsg.setDI(*value);
			// errMsg.setTU(String::valueOf(max));
			// creature->sendSystemMessage(errMsg);
		}
	}

	// Validate all float inputs
	void validateFloatTokenErrors(StringTokenizer* args, CreatureObject* creature, bool* errors, float* value, float min, float max, String tokenType, int precision = 1) const {
		// Validate more tokens
		if (args->hasMoreTokens()) 
			*value = args->getFloatToken();
		else {
			*errors = true;
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax");
			return;
		}

		// Validate Argument
		if (*value < min) {
			*errors = true;
			// StringIdChatParameter errMsg("custom/commands/generateEquipment", "invalid_float_low");
			// errMsg.setTT(tokenType);
			// errMsg.setDF(*value);
			// errMsg.setDI(min);
			// creature->sendSystemMessage(errMsg);
		} else if (*value > max) {
			*errors = true;
			// StringIdChatParameter errMsg("custom/commands/generateEquipment", "invalid_float_high");
			// errMsg.setTT(tokenType);
			// errMsg.setDF(*value);
			// errMsg.setDI(max);
			// creature->sendSystemMessage(errMsg);
		}
	}

	// Validate all skillmod inputs
	void validateSkillModTokens(StringTokenizer* args, CreatureObject* creature, bool* errors, String* value) const {
		// Validate more tokens
		if (args->hasMoreTokens()) 
			*value = args->getStringToken();
		else {
			*errors = true;
			// creature->sendSystemMessage("@custom/commands/generateEquipment:invalid_syntax");
		}
	}

};

#endif //OBJECTCOMMAND_H_
