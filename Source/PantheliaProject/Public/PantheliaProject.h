// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// Canales de colisión custom del proyecto.
// Definidos en Project Settings → Collision. El orden en que se crearon determina
// el número de GameTraceChannel. Según Config/DefaultEngine.ini:
//   - GameTraceChannel1 = "Fighter"    (Trace Channel,  Default: Ignore)
//   - GameTraceChannel2 = "Projectile" (Object Channel, Default: Ignore)
//
// IMPORTANTE: verificar el orden en Project Settings → Collision si se añaden o
// reordenan canales, ya que el número de GameTraceChannel cambiaría.
//
// Uso de cada canal:
//   - ECC_Fighter:    detección de golpes de armas melee. Lo usan el TraceComponent
//                     legacy (jugador) y el WeaponTraceComponent (enemigos) para sus
//                     sweeps. Los actores golpeables responden a este canal con Overlap.
//   - ECC_Projectile: tipo de objeto de los proyectiles (hechizos, flechas). Los
//                     actores que pueden ser impactados por proyectiles responden a
//                     este canal con Overlap.
#define ECC_Fighter    ECC_GameTraceChannel1
#define ECC_Projectile ECC_GameTraceChannel2