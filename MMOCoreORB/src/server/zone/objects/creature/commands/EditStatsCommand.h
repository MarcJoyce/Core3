/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef EDITSTATSCOMMAND_H_
#define EDITSTATSCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/player/PlayerObject.h"

class EditStatsCommand : public QueueCommand {
public:

	EditStatsCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		ManagedReference<SceneObject*> object = server->getZoneServer()->getObject(target);

		if (object != nullptr) {
			if (!object->isCreatureObject()) {
				TangibleObject* tangibleObject = dynamic_cast<TangibleObject*>(object.get());

				if (tangibleObject != nullptr) {
					object = creature;
				} else {
					creature->sendSystemMessage("@healing_response:healing_response_77"); // Target must be a player or a creature pet in order to apply enhancements.
					return GENERALERROR;
				}
			}
		} else {
			object = creature;
		}

		CreatureObject* patient = object->asCreatureObject();

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		StringTokenizer args(arguments.toString());
		
		try {
			String commandType;
			args.getStringToken(commandType);

			if (commandType.beginsWith("buff")) {
				String modifier;
				args.getStringToken(modifier);

				int amount;
				amount = args.getIntToken();

				if (modifier == "health" || modifier == "strength" || modifier == "constitution" ||
					modifier == "action" || modifier == "stamina" || modifier == "quickness" ||
					modifier == "mind" || modifier == "focus" || modifier == "willpower") {
					patient->addMaxHAM(CreatureAttribute::getAttribute(modifier), amount);
				} else if (modifier == "all") {
					patient->addMaxHAM(CreatureAttribute::getAttribute("health"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("strength"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("constitution"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("action"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("stamina"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("quickness"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("mind"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("focus"), amount);
					patient->addMaxHAM(CreatureAttribute::getAttribute("willpower"), amount);
				}
			}
			else if (commandType.beginsWith("skill")) {
				String state;
				args.getStringToken(state);

				if (state.beginsWith("temp")) {

				String skillMod;
				args.getStringToken(skillMod);

				int amount;
				amount = args.getIntToken();
				int currentSkillModValue = patient->getSkillMod(skillMod);
				int skillModDelta = amount - currentSkillModValue;

				patient->addSkillMod(SkillModManager::BUFF, skillMod, skillModDelta, true);
				} else if (state.beginsWith("perm")) {
					String skillMod;
					args.getStringToken(skillMod);

					int amount;
					amount = args.getIntToken();

					int currentSkillModValue = patient->getSkillMod(skillMod);
					int skillModDelta = amount - currentSkillModValue;

					patient->addSkillMod(SkillModManager::SKILLBOX, skillMod, skillModDelta, true);
				}
			}
			else if (commandType.beginsWith("vis")) {
				int vis;
				vis = args.getIntToken();

				Reference<PlayerObject*> ghost = patient->getSlottedObject("ghost").castTo<PlayerObject*>();

				ghost->setVisibility(vis);
			}

		} catch (Exception& e) {
			creature->sendSystemMessage("Syntax: /editStats [buff/skill/vis] [ham/all/skillMod] amount");
		}

		return SUCCESS;
	}

};

#endif //EDITSTATSCOMMAND_H_
