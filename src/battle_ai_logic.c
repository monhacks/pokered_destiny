#include "battle_ai_logic.h"
#include "battle.h"
#include "battle_anim.h"
#include "random.h"
#include "constants/moves.h"

void LoadTemporaryBattleMon(struct Pokemon *pokemon, struct BattlePokemon *battlePokemon);


s32 ComputeMaxDamage(u8 battlerAttacker, u8 battlerTarget)
{
    u32 dmg, maxMoveDamage = 0;
    u16 currMove;
    u8 curMoveIdx;
    for(curMoveIdx = 0; curMoveIdx < MAX_MON_MOVES; curMoveIdx++)
    {
        gCurrentMove = gBattleMons[battlerAttacker].moves[curMoveIdx];
        currMove = gBattleMons[battlerAttacker].moves[curMoveIdx];
        gBattleMoveDamage = 0;

        if (currMove != 0 && gBattleMoves[currMove].power > 0)
        {
            gDynamicBasePower = 0;
            gBattleStruct->dynamicMoveType = 0;
            gBattleScripting.dmgMultiplier = 1;
            gMoveResultFlags = 0;
            gCritMultiplier = 1;

            AI_CalcDmg(battlerAttacker, battlerTarget);
            AI_TypeCalc(gCurrentMove, gBattleMons[battlerTarget].species, gBattleMons[battlerTarget].ability);
        }

        if(gBattleMoveDamage > maxMoveDamage) maxMoveDamage = gBattleMoveDamage;
    }
     
    return maxMoveDamage;
}

/**
 * 1 - The current POKEMON is slow 
 *    1.1 AND can be defeated by one of the oponents atacks
 */
bool8 Battle_ai_switchToAvoidDefeat()
{
    bool8 willBeDefeated;
    bool8 willGoFirst;
    u8 oponent;
    u8 ai;
    s32 damage;
    ai = gActiveBattler; //gBattlerAttacker;
    oponent = gActiveBattler ^ BIT_SIDE;// gActiveBattler ^ BIT_SIDE; gBattlerTarget
    damage = ComputeMaxDamage(oponent, ai);
    willBeDefeated = damage >= gBattleMons[ai].hp;
    
    if (willBeDefeated)
    {
        willGoFirst = GetWhoStrikesFirst(ai, oponent, TRUE) == 0;
        if (willGoFirst)
        {
            // will win no need to change
            if (ComputeMaxDamage(ai, oponent) >= gBattleMons[oponent].hp) return FALSE;
            // chance proportional to the remaining hp. If the hp is low the chance of staying is high.
            if ((Random() % gBattleMons[ai].maxHP) > gBattleMons[ai].hp) return FALSE;
        }
        *(gBattleStruct->AI_monToSwitchIntoId + (GetBattlerPosition(gActiveBattler) >> 1)) = PARTY_SIZE;
        return TRUE;
    }
    return FALSE;
}

/** 2 - The current POKEMON do not have means to ko
 *    2.1 AND AI has a POKEMON that can survive to one atack of current mon and is faster and have an attack able to ko current mon
 *    2.2 AND AI has a POKEMON that can survive to two atacks of current mon and have an attack able to ko current mon
 */
bool8 Battle_ai_switchToSwipper()
{
    bool8 ableToKill;
    u8 ai, oponent, pokemon;
    s32 damage, maxDamage;
    u8 curMoveIdx;
    struct BattlePokemon battlePokemon;
    
    ai = gActiveBattler;
    oponent = gActiveBattler ^ BIT_SIDE;
    damage = ComputeMaxDamage(ai, oponent);
    ableToKill = damage >= gBattleMons[oponent].hp;

    if (!ableToKill) {
        for(pokemon = 0; pokemon < PARTY_SIZE; pokemon++) {
            // Invalid pokemon to switch
            if (GetMonData(&gEnemyParty[pokemon], MON_DATA_HP) == 0
             || GetMonData(&gEnemyParty[pokemon], MON_DATA_SPECIES2) == SPECIES_NONE
             || GetMonData(&gEnemyParty[pokemon], MON_DATA_SPECIES2) == SPECIES_EGG
             || pokemon == gBattlerPartyIndexes[ai])
                continue;

            LoadTemporaryBattleMon(&gEnemyParty[pokemon], &battlePokemon);
            // Compute the maximun damage that the current opponent pokemon is able to deal to pokemon i.
            maxDamage = 0;
            for(curMoveIdx = 0; curMoveIdx < MAX_MON_MOVES; curMoveIdx++)
            {
                gCurrentMove = gBattleMons[oponent].moves[curMoveIdx];
                if (gCurrentMove == MOVE_NONE)
                    continue;
                
                gDynamicBasePower = 0;
                gBattleStruct->dynamicMoveType = 0;
                gBattleScripting.dmgMultiplier = 1;
                gMoveResultFlags = 0;
                gCritMultiplier = 1;
                gBattleMoveDamage = CalculateBaseDamage(&gBattleMons[oponent], &battlePokemon, gCurrentMove, 0, 0, 0, oponent, ai);
                AI_TypeCalc(gCurrentMove, battlePokemon.species, battlePokemon.ability);
                if (gBattleMoveDamage > maxDamage) maxDamage = gBattleMoveDamage;
            }
            // if the pokemon is slower it will take 2 hits before deliver the first attack
            if (battlePokemon.speed <= gBattleMons[oponent].speed) maxDamage *= 2;

            if (maxDamage >= battlePokemon.hp)
                continue;
            
            // Search for a move able to kill opponent's pokemon
            for(curMoveIdx = 0; curMoveIdx < MAX_MON_MOVES; curMoveIdx++)
            {
                gCurrentMove = battlePokemon.moves[curMoveIdx];
                if (gCurrentMove == MOVE_NONE)
                    continue;
                
                gDynamicBasePower = 0;
                gBattleStruct->dynamicMoveType = 0;
                gBattleScripting.dmgMultiplier = 1;
                gMoveResultFlags = 0;
                gCritMultiplier = 1;
                gBattleMoveDamage = CalculateBaseDamage(&battlePokemon, &gBattleMons[oponent], gCurrentMove, 0, 0, 0, ai, oponent);
                AI_TypeCalc(gCurrentMove, gBattleMons[oponent].species, gBattleMons[oponent].ability);

                if (gBattleMoveDamage < gBattleMons[oponent].hp)
                    continue;
                
                gBattleStruct->AI_monToSwitchIntoId[GetBattlerPosition(ai) >> 1] = pokemon;
                return TRUE;
            }
        }
    }
    return FALSE;
}

void LoadTemporaryBattleMon(struct Pokemon *pokemon, struct BattlePokemon *battlePokemon)
{
    s32 i;
    u8 iv;
    battlePokemon->species = GetMonData(pokemon, MON_DATA_SPECIES);
    battlePokemon->type1 = gBaseStats[battlePokemon->species].type1;
    battlePokemon->type2 = gBaseStats[battlePokemon->species].type2;
    battlePokemon->item = GetMonData(pokemon, MON_DATA_HELD_ITEM);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        battlePokemon->moves[i] = GetMonData(pokemon, MON_DATA_MOVE1 + i);
        battlePokemon->pp[i] = GetMonData(pokemon, MON_DATA_PP1 + i);
    }
    battlePokemon->ppBonuses = GetMonData(pokemon, MON_DATA_PP_BONUSES);
    battlePokemon->friendship = GetMonData(pokemon, MON_DATA_FRIENDSHIP);
    battlePokemon->experience= GetMonData(pokemon, MON_DATA_EXP);
    battlePokemon->hpIV = GetMonData(pokemon, MON_DATA_HP_IV);
    battlePokemon->attackIV = GetMonData(pokemon, MON_DATA_ATK_IV);
    battlePokemon->defenseIV = GetMonData(pokemon, MON_DATA_DEF_IV);
    battlePokemon->speedIV = GetMonData(pokemon, MON_DATA_SPEED_IV);
    battlePokemon->spAttackIV = GetMonData(pokemon, MON_DATA_SPATK_IV);
    battlePokemon->spDefenseIV = GetMonData(pokemon, MON_DATA_SPDEF_IV);
    battlePokemon->personality = GetMonData(pokemon, MON_DATA_PERSONALITY);
    battlePokemon->status1 = GetMonData(pokemon, MON_DATA_STATUS);
    battlePokemon->level = GetMonData(pokemon, MON_DATA_LEVEL);
    battlePokemon->hp = GetMonData(pokemon, MON_DATA_HP);
    battlePokemon->maxHP = GetMonData(pokemon, MON_DATA_MAX_HP);
    battlePokemon->attack = GetMonData(pokemon, MON_DATA_ATK);
    battlePokemon->defense = GetMonData(pokemon, MON_DATA_DEF);
    battlePokemon->speed = GetMonData(pokemon, MON_DATA_SPEED);
    battlePokemon->spAttack = GetMonData(pokemon, MON_DATA_SPATK);
    battlePokemon->spDefense= GetMonData(pokemon, MON_DATA_SPDEF);
    for (i = 0; i < BATTLE_STATS_NO; i++) {
        battlePokemon->statStages[i] = DEFAULT_STAT_STAGE;
    }
}
