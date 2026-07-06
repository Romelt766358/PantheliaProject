// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Pragma once es obligatorio. Sin él, si este header se incluye desde múltiples
// .cpp en la misma unidad de traducción, el compilador define LogPanthelia dos
// veces y lanza un error de "struct type redefinition".

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Declara la categoría de log LogPanthelia para todo el proyecto.
// Usar LogPanthelia en lugar de LogTemp hace que los mensajes aparezcan
// en el Output Log con la categoría "LogPanthelia" en lugar de "LogTemp",
// lo que facilita filtrarlos con el buscador del Output Log.
//
// Uso:   UE_LOG(LogPanthelia, Log,     TEXT("Mensaje informativo"));
//        UE_LOG(LogPanthelia, Warning, TEXT("Algo sospechoso"));
//        UE_LOG(LogPanthelia, Error,   TEXT("Algo roto"));
//
// Verbosidades más usadas:
//   Log     → info normal de flujo (subida de nivel, XP ganada, etc.)
//   Warning → algo inesperado pero no fatal (tag no encontrado, etc.)
//   Error   → algo que impide el funcionamiento correcto
DECLARE_LOG_CATEGORY_EXTERN(LogPanthelia, Log, All);
