#include "FigureAction_private.h"

#include "Figure.h"
#include "Formation.h"
#include "Routing.h"
#include "Sound.h"

#include "Data/CityInfo.h"
#include "Data/Event.h"
#include "Data/Formation.h"
#include "Data/Grid.h"

static void enemyInitial(int walkerId, struct Data_Walker *f, struct Data_Formation *m)
{
	Figure_updatePositionInTileList(walkerId);
	f->graphicOffset = 0;
	FigureRoute_remove(walkerId);
	f->waitTicks--;
	if (f->waitTicks <= 0) {
		if (f->isGhost && f->indexInFormation == 0) {
			if (m->layout == FormationLayout_Enemy8) {
				Sound_Speech_playFile("wavs/drums.wav");
			} else if (m->layout == FormationLayout_Enemy12) {
				Sound_Speech_playFile("wavs/horn2.wav");
			} else {
				Sound_Speech_playFile("wavs/horn1.wav");
			}
		}
		f->isGhost = 0;
		if (m->recentFight) {
			f->actionState = FigureActionState_154_EnemyFighting;
		} else {
			f->destinationX = m->destinationX + f->formationPositionX;
			f->destinationY = m->destinationY + f->formationPositionY;
			if (Routing_getGeneralDirection(f->x, f->y, f->destinationX, f->destinationY) < 8) {
				f->actionState = FigureActionState_153_EnemyMarching;
			}
		}
	}
	if (f->type == Figure_Enemy43_Spear || f->type == Figure_Enemy46_Camel ||
		f->type == Figure_Enemy51_Spear || f->type == Figure_Enemy52_MountedArcher) {
		// missile throwers
		f->waitTicksMissile++;
		int xTile, yTile;
		if (f->waitTicksMissile > Constant_FigureProperties[f->type].missileFrequency) {
			f->waitTicksMissile = 0;
			if (FigureAction_CombatEnemy_getMissileTarget(walkerId, 10, Data_CityInfo.numSoldiersInCity < 4, &xTile, &yTile)) {
				f->attackGraphicOffset = 1;
				f->direction = Routing_getDirectionForMissileShooter(f->x, f->y, xTile, yTile);
			} else {
				f->attackGraphicOffset = 0;
			}
		}
		if (f->attackGraphicOffset) {
			int missileType;
			switch (m->enemyType) {
				case EnemyType_4_Goth:
				case EnemyType_5_Pergamum:
				case EnemyType_9_Egyptian:
				case EnemyType_10_Carthaginian:
					missileType = Figure_Arrow;
					break;
				default:
					missileType = Figure_Spear;
					break;
			}
			if (f->attackGraphicOffset == 1) {
				Figure_createMissile(walkerId, f->x, f->y, xTile, yTile, missileType);
				m->missileFired = 6;
			}
			if (missileType == Figure_Arrow) {
				Data_CityInfo.soundShootArrow--;
				if (Data_CityInfo.soundShootArrow <= 0) {
					Data_CityInfo.soundShootArrow = 10;
					Sound_Effects_playChannel(SoundChannel_Arrow);
				}
			}
			f->attackGraphicOffset++;
			if (f->attackGraphicOffset > 100) {
				f->attackGraphicOffset = 0;
			}
		}
	}
}

static void enemyMarching(int walkerId, struct Data_Walker *f, struct Data_Formation *m)
{
	f->waitTicks--;
	if (f->waitTicks <= 0) {
		f->waitTicks = 50;
		f->destinationX = m->destinationX + f->formationPositionX;
		f->destinationY = m->destinationY + f->formationPositionY;
		if (Routing_getGeneralDirection(f->x, f->y, f->destinationX, f->destinationY) == DirFigure_8_AtDestination) {
			f->actionState = FigureActionState_151_EnemyInitial;
			return;
		}
		f->destinationBuildingId = m->destinationBuildingId;
		FigureRoute_remove(walkerId);
	}
	FigureMovement_walkTicks(walkerId, f->speedMultiplier);
	if (f->direction == DirFigure_8_AtDestination ||
		f->direction == DirFigure_9_Reroute ||
		f->direction == DirFigure_10_Lost) {
		f->actionState = FigureActionState_151_EnemyInitial;
	}
}

static void enemyFighting(int walkerId, struct Data_Walker *f, struct Data_Formation *m)
{
	if (!m->recentFight) {
		f->actionState = FigureActionState_151_EnemyInitial;
	}
	if (f->type != Figure_Enemy46_Camel && f->type != Figure_Enemy47_Elephant) {
		if (f->type == Figure_Enemy48_Chariot || f->type == Figure_Enemy52_MountedArcher) {
			Data_CityInfo.soundMarchHorse--;
			if (Data_CityInfo.soundMarchHorse <= 0) {
				Data_CityInfo.soundMarchHorse = 200;
				Sound_Effects_playChannel(SoundChannel_HorseMoving);
			}
		} else {
			Data_CityInfo.soundMarchEnemy--;
			if (Data_CityInfo.soundMarchEnemy <= 0) {
				Data_CityInfo.soundMarchEnemy = 200;
				Sound_Effects_playChannel(SoundChannel_Marching);
			}
		}
	}
	int targetId = f->targetFigureId;
	if (FigureIsDead(targetId)) {
		f->targetFigureId = 0;
		targetId = 0;
	}
	if (targetId <= 0) {
		targetId = FigureAction_CombatEnemy_getTarget(f->x, f->y);
		if (targetId) {
			f->destinationX = Data_Walkers[targetId].x;
			f->destinationY = Data_Walkers[targetId].y;
			f->targetFigureId = targetId;
			f->targetFigureCreatedSequence = Data_Walkers[targetId].createdSequence;
			Data_Walkers[targetId].targetedByFigureId = walkerId;
			FigureRoute_remove(walkerId);
		}
	}
	if (targetId > 0) {
		FigureMovement_walkTicks(walkerId, f->speedMultiplier);
		if (f->direction == DirFigure_8_AtDestination) {
			f->destinationX = Data_Walkers[f->targetFigureId].x;
			f->destinationY = Data_Walkers[f->targetFigureId].y;
			FigureRoute_remove(walkerId);
		} else if (f->direction == DirFigure_9_Reroute || f->direction == DirFigure_10_Lost) {
			f->actionState = FigureActionState_151_EnemyInitial;
			f->targetFigureId = 0;
		}
	} else {
		f->actionState = FigureActionState_151_EnemyInitial;
		f->waitTicks = 50;
	}
}

static void FigureAction_enemyCommon(int walkerId, struct Data_Walker *f)
{
	struct Data_Formation *m = &Data_Formations[f->formationId];
	Data_CityInfo.numEnemiesInCity++;
	f->terrainUsage = FigureTerrainUsage_Enemy;
	f->formationPositionX = FigureActionFormationLayoutPositionX(m->layout, f->indexInFormation);
	f->formationPositionY = FigureActionFormationLayoutPositionY(m->layout, f->indexInFormation);

	switch (f->actionState) {
		case FigureActionState_150_Attack:
			FigureAction_Common_handleAttack(walkerId);
			break;
		case FigureActionState_149_Corpse:
			FigureAction_Common_handleCorpse(walkerId);
			break;
		case FigureActionState_148_Fleeing:
			f->destinationX = f->sourceX;
			f->destinationY = f->sourceY;
			FigureMovement_walkTicks(walkerId, f->speedMultiplier);
			if (f->direction == DirFigure_8_AtDestination ||
				f->direction == DirFigure_9_Reroute ||
				f->direction == DirFigure_10_Lost) {
				f->state = FigureState_Dead;
			}
			break;
		case FigureActionState_151_EnemyInitial:
			enemyInitial(walkerId, f, m);
			break;
		case FigureActionState_152_EnemyWaiting:
			Figure_updatePositionInTileList(walkerId);
			break;
		case FigureActionState_153_EnemyMarching:
			enemyMarching(walkerId, f, m);
			break;
		case FigureActionState_154_EnemyFighting:
			enemyFighting(walkerId, f, m);
			break;
	}
}

static int getDirection(struct Data_Walker *f)
{
	int dir;
	if (f->actionState == FigureActionState_150_Attack) {
		dir = f->attackDirection;
	} else if (f->direction < 8) {
		dir = f->direction;
	} else {
		dir = f->previousTileDirection;
	}
	FigureActionNormalizeDirection(dir);
	return dir;
}

static int getDirectionMissile(struct Data_Walker *f, struct Data_Formation *m)
{
	int dir;
	if (f->actionState == FigureActionState_150_Attack) {
		dir = f->attackDirection;
	} else if (m->missileFired || f->direction < 8) {
		dir = f->direction;
	} else {
		dir = f->previousTileDirection;
	}
	FigureActionNormalizeDirection(dir);
	return dir;
}

void FigureAction_enemy43_Spear(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirectionMissile(f, m);
	
	f->isEnemyGraphic = 1;
	
	switch (m->enemyType) {
		case EnemyType_5_Pergamum:
		case EnemyType_6_Seleucid:
		case EnemyType_7_Etruscan:
		case EnemyType_8_Greek:
			break;
		default:
			return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 745 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 745 + dir;
		}
	} else if (f->actionState == FigureActionState_151_EnemyInitial) {
		f->graphicId = 697 + dir + 8 * FigureActionMissileLauncherGraphicOffset(f);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 793 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 745 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy44_Sword(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	switch (m->enemyType) {
		case EnemyType_5_Pergamum:
		case EnemyType_6_Seleucid:
		case EnemyType_9_Egyptian:
			break;
		default:
			return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 545 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 545 + dir;
		}
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 593 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 545 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 449 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy45_Sword(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	switch (m->enemyType) {
		case EnemyType_7_Etruscan:
		case EnemyType_8_Greek:
		case EnemyType_10_Carthaginian:
			break;
		default:
			return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 545 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 545 + dir;
		}
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 593 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 545 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 449 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy46_Camel(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirectionMissile(f, m);
	
	f->isEnemyGraphic = 1;
	
	if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	} else if (f->actionState == FigureActionState_150_Attack) {
		f->graphicId = 601 + dir;
	} else if (f->actionState == FigureActionState_151_EnemyInitial) {
		f->graphicId = 697 + dir + 8 * FigureActionMissileLauncherGraphicOffset(f);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 745 + FigureActionCorpseGraphicOffset(f);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy47_Elephant(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	if (f->direction == DirFigure_11_Attack || f->actionState == FigureActionState_150_Attack) {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 705 + FigureActionCorpseGraphicOffset(f);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy48_Chariot(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 3;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	if (f->direction == DirFigure_11_Attack || f->actionState == FigureActionState_150_Attack) {
		f->graphicId = 697 + dir + 8 * (f->graphicOffset / 2);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 745 + FigureActionCorpseGraphicOffset(f);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy49_FastSword(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 2;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	int attackId, corpseId, normalId;
	if (m->enemyType == EnemyType_0_Barbarian) {
		attackId = 393;
		corpseId = 441;
		normalId = 297;
	} else if (m->enemyType == EnemyType_1_Numidian) {
		attackId = 593;
		corpseId = 641;
		normalId = 449;
	} else if (m->enemyType == EnemyType_4_Goth) {
		attackId = 545;
		corpseId = 593;
		normalId = 449;
	} else {
		return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = attackId + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = attackId + dir;
		}
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = corpseId + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = attackId + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = normalId + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy50_Sword(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	if (m->enemyType != EnemyType_2_Gaul && m->enemyType != EnemyType_3_Celt) {
		return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 545 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 545 + dir;
		}
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 593 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 545 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 449 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy51_Spear(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 2;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirectionMissile(f, m);
	
	f->isEnemyGraphic = 1;
	
	if (m->enemyType != EnemyType_1_Numidian) {
		return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 593 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 593 + dir;
		}
	} else if (f->actionState == FigureActionState_151_EnemyInitial) {
		f->graphicId = 545 + dir + 8 * FigureActionMissileLauncherGraphicOffset(f);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 641 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 593 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 449 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy52_MountedArcher(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 3;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirectionMissile(f, m);
	
	f->isEnemyGraphic = 1;
	
	if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	} else if (f->actionState == FigureActionState_150_Attack) {
		f->graphicId = 601 + dir;
	} else if (f->actionState == FigureActionState_151_EnemyInitial) {
		f->graphicId = 697 + dir + 8 * FigureActionMissileLauncherGraphicOffset(f);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 745 + FigureActionCorpseGraphicOffset(f);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy53_Axe(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	f->isEnemyGraphic = 1;
	
	if (m->enemyType != EnemyType_2_Gaul) {
		return;
	}
	if (f->actionState == FigureActionState_150_Attack) {
		if (f->attackGraphicOffset >= 12) {
			f->graphicId = 697 + dir + 8 * ((f->attackGraphicOffset - 12) / 2);
		} else {
			f->graphicId = 697 + dir;
		}
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = 745 + FigureActionCorpseGraphicOffset(f);
	} else if (f->direction == DirFigure_11_Attack) {
		f->graphicId = 697 + dir + 8 * (f->graphicOffset / 2);
	} else {
		f->graphicId = 601 + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemy54_Gladiator(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	f->terrainUsage = FigureTerrainUsage_Any;
	f->useCrossCountry = 0;
	FigureActionIncreaseGraphicOffset(f, 12);
	if (Data_Event.gladiatorRevolt.state == SpecialEvent_Finished) {
		// end of gladiator revolt: kill gladiators
		if (f->actionState != FigureActionState_149_Corpse) {
			f->actionState = FigureActionState_149_Corpse;
			f->waitTicks = 0;
			f->direction = 0;
		}
	}
	switch (f->actionState) {
		case FigureActionState_150_Attack:
			FigureAction_Common_handleAttack(walkerId);
			FigureActionIncreaseGraphicOffset(f, 16);
			break;
		case FigureActionState_149_Corpse:
			FigureAction_Common_handleCorpse(walkerId);
			break;
		case FigureActionState_158_NativeCreated:
			f->graphicOffset = 0;
			f->waitTicks++;
			if (f->waitTicks > 10 + (walkerId & 3)) {
				f->waitTicks = 0;
				f->actionState = FigureActionState_159_NativeAttacking;
				int xTile, yTile;
				int buildingId = Formation_Rioter_getTargetBuilding(&xTile, &yTile);
				if (buildingId) {
					f->destinationX = xTile;
					f->destinationY = yTile;
					f->destinationBuildingId = buildingId;
					FigureRoute_remove(walkerId);
				} else {
					f->state = FigureState_Dead;
				}
			}
			break;
		case FigureActionState_159_NativeAttacking:
			Data_CityInfo.numAttackingNativesInCity = 10;
			f->terrainUsage = FigureTerrainUsage_Enemy;
			FigureMovement_walkTicks(walkerId, 1);
			if (f->direction == DirFigure_8_AtDestination ||
				f->direction == DirFigure_9_Reroute ||
				f->direction == DirFigure_10_Lost) {
				f->actionState = FigureActionState_158_NativeCreated;
			}
			break;
	}
	int dir;
	if (f->actionState == FigureActionState_150_Attack || f->direction == DirFigure_11_Attack) {
		dir = f->attackDirection;
	} else if (f->direction < 8) {
		dir = f->direction;
	} else {
		dir = f->previousTileDirection;
	}
	FigureActionNormalizeDirection(dir);

	if (f->actionState == FigureActionState_150_Attack || f->direction == DirFigure_11_Attack) {
		f->graphicId = GraphicId(ID_Graphic_Figure_Gladiator) + dir + 104 + 8 * (f->graphicOffset / 2);
	} else if (f->actionState == FigureActionState_149_Corpse) {
		f->graphicId = GraphicId(ID_Graphic_Figure_Gladiator) + 96 + FigureActionCorpseGraphicOffset(f);
	} else {
		f->graphicId = GraphicId(ID_Graphic_Figure_Gladiator) + dir + 8 * f->graphicOffset;
	}
}

void FigureAction_enemyCaesarLegionary(int walkerId)
{
	struct Data_Walker *f = &Data_Walkers[walkerId];
	struct Data_Formation *m = &Data_Formations[f->formationId];
	Data_CityInfo.numImperialSoldiersInCity++;
	FigureActionIncreaseGraphicOffset(f, 12);
	f->cartGraphicId = 0;
	f->speedMultiplier = 1;
	FigureAction_enemyCommon(walkerId, f);
	
	int dir = getDirection(f);
	
	if (f->direction == DirFigure_11_Attack) {
		f->graphicId = GraphicId(ID_Graphic_Figure_CaesarLegionary) + dir +
			8 * ((f->attackGraphicOffset - 12) / 2);
	}
	switch (f->actionState) {
		case FigureActionState_150_Attack:
			if (f->attackGraphicOffset >= 12) {
				f->graphicId = GraphicId(ID_Graphic_Figure_CaesarLegionary) + dir +
					8 * ((f->attackGraphicOffset - 12) / 2);
			} else {
				f->graphicId = GraphicId(ID_Graphic_Figure_CaesarLegionary) + dir;
			}
			break;
		case FigureActionState_149_Corpse:
			f->graphicId = GraphicId(ID_Graphic_Figure_CaesarLegionary) +
				FigureActionCorpseGraphicOffset(f) + 152;
			break;
		case FigureActionState_84_SoldierAtStandard:
			if (m->isHalted && m->layout == FormationLayout_Tortoise && m->missileAttackTimeout) {
				f->graphicId = GraphicId(ID_Graphic_Figure_FortLegionary) + dir + 144;
			} else {
				f->graphicId = GraphicId(ID_Graphic_Figure_FortLegionary) + dir;
			}
			break;
		default:
			f->graphicId = GraphicId(ID_Graphic_Figure_CaesarLegionary) + 48 + dir + 8 * f->graphicOffset;
			break;
	}
}

int FigureAction_HerdEnemy_moveFormationTo(int formationId, int x, int y, int *xTile, int *yTile)
{
	struct Data_Formation *m = &Data_Formations[formationId];
	int baseOffset = GridOffset(
		FigureActionFormationLayoutPositionX(m->layout, 0),
		FigureActionFormationLayoutPositionY(m->layout, 0));
	int walkerOffsets[50];
	walkerOffsets[0] = 0;
	for (int i = 1; i < m->numFigures; i++) {
		walkerOffsets[i] = GridOffset(
			FigureActionFormationLayoutPositionX(m->layout, i),
			FigureActionFormationLayoutPositionY(m->layout, i)) - baseOffset;
	}
	Routing_canTravelOverLandNonCitizen(x, y, -1, -1, 0, 600);
	for (int r = 0; r <= 10; r++) {
		int xMin = x - r;
		int yMin = y - r;
		int xMax = x + r;
		int yMax = y + r;
		Bound2ToMap(xMin, yMin, xMax, yMax);
		for (int yy = yMin; yy <= yMax; yy++) {
			for (int xx = xMin; xx <= xMax; xx++) {
				int canMove = 1;
				for (int fig = 0; fig < m->numFigures; fig++) {
					int gridOffset = GridOffset(xx, yy) + walkerOffsets[fig];
					if (Data_Grid_terrain[gridOffset] & Terrain_1237) {
						canMove = 0;
						break;
					}
					if (Data_Grid_routingDistance[gridOffset] <= 0) {
						canMove = 0;
						break;
					}
					if (Data_Grid_figureIds[gridOffset] &&
						Data_Walkers[Data_Grid_figureIds[gridOffset]].formationId != formationId) {
						canMove = 0;
						break;
					}
				}
				if (canMove) {
					*xTile = xx;
					*yTile = yy;
					return 1;
				}
			}
		}
	}
	return 0;
}