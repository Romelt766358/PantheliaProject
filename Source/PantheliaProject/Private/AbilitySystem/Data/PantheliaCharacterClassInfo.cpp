// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"

FCharacterClassDefaultInfo UPantheliaCharacterClassInfo::GetClassDefaultInfo(EPantheliaCharacterClass CharacterClass)
{
    // FindChecked lanza un assert si la clave no existe en el mapa.
    // Si ves este crash: falta añadir el arquetipo en DA_CharacterClassInfo en el editor.
    return CharacterClassInformation.FindChecked(CharacterClass);
}