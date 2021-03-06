/****************************************************************************

    AstroMenace
    Hardcore 3D space scroll-shooter with spaceship upgrade possibilities.
    Copyright (c) 2006-2019 Mikhail Kurinnoi, Viewizard


    AstroMenace is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    AstroMenace is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with AstroMenace. If not, see <https://www.gnu.org/licenses/>.


    Website: https://viewizard.com/
    Project: https://github.com/viewizard/astromenace
    E-mail: viewizard@viewizard.com

*****************************************************************************/

// TODO translate comments

#include "../core/core.h"
#include "../config/config.h"
#include "../platform/platform.h"
#include "../ui/font.h"
#include "../ui/game/text.h"
#include "../assets/audio.h"
#include "../assets/texture.h"
#include "../game/camera.h"
#include "../object3d/explosion/explosion.h"
#include "../object3d/space_object/space_object.h"
#include "../object3d/ground_object/ground_object.h"
#include "../object3d/space_ship/space_ship.h"
#include "../object3d/projectile/projectile.h"
#include "../game.h" // FIXME "game.h" should be replaced by individual headers
#include "SDL2/SDL.h"

// NOTE switch to nested namespace definition (namespace A::B::C { ... }) (since C++17)
namespace viewizard {
namespace astromenace {

// режим неубиваемости... отладка
bool UndeadDebugMode = false;

// состояние управления, нужно, чтобы определять что менялось
int LastMouseX = -1;
int LastMouseY = -1;
// для восстановления положения курсора в меню, точнее при выходе из него
int LastMouseXR = 0;
int LastMouseYR = 0;

// в показателях от -1.0f до 1.0f
// назад-вперед
float MoveFB = 0.0f;
// лево-право
float MoveLR = 0.0f;

// текущая энергия корабля
float CurrentPlayerShipEnergy;

// для управления в аркадном режиме маневровыми двигателями
bool PlayerFighterLeftEng = false;
bool PlayerFighterRightEng = false;

std::weak_ptr<cParticleSystem> Shild1{};
std::weak_ptr<cParticleSystem> Shild2{};
float ShildRadius;
float ShildEnergyStatus;
float ShildStartHitStatus;

// голос с ворнингом, если столкнулись с несбиваемой частью
unsigned int VoiceWarningCollisionDetected{0};

// Номер, для проигрывания голосового сообщения об обнаружении ракеты
unsigned int VoiceMissileDetected{0};
bool VoiceMissileDetectedStatus{false};
// номер, для проигрывания голосового сообщения о проблемах в орудии
// Номер, для проигрывания голосового сообщения об отсутствии снарядов в боекомплекте
unsigned int VoiceWeaponMalfunction{0};
// для звука - мало жизни
unsigned int SoundLowLife{0};


// тут храним какая часть взорвалась на корабле игрока
int PlayerDeadObjectPieceNum;


// симулятивный режим
sVECTOR3D CurrentMovementVel(0.0f, 0.0f, 0.0f);

// работа с морганием вывода
extern float CurrentAlert2;
extern float CurrentAlert3;

// для переменного типа стрельбы
int PrimaryGroupCurrentFireWeaponNum = 1;
float PrimaryGroupCurrentFireWeaponDelay = 0.0f;
int SecondaryGroupCurrentFireWeaponNum = 1;
float SecondaryGroupCurrentFireWeaponDelay = 0.0f;




//------------------------------------------------------------------------------------
// Получаем максимально возможное значение энергии для устройства реактора-батареи
//------------------------------------------------------------------------------------
float GetShipMaxEnergy(int Num)
{
    switch (Num) {
    case 0:
        return 0.0f;
    // аккамулятор
    case 1:
        return 100.0f;
    // ядерный
    case 2:
        return 200.0f;
    // плазменный
    case 3:
        return 400.0f;
    // антиматерия
    case 4:
        return 800.0f;

    default:
        std::cerr << __func__ << "(): " << "wrong Num.\n";
        break;
    }

    return -1.0f;
}
//------------------------------------------------------------------------------------
// Получаем перезарядку энергии, в секунду
//------------------------------------------------------------------------------------
float GetShipRechargeEnergy(int Num)
{
    switch (Num) {
    case 0:
        return 0.0f;
    // аккамулятор
    case 1:
        return 20.0f;
    // ядерный
    case 2:
        return 50.0f;
    // плазменный
    case 3:
        return 130.0f;
    // антиматерия
    case 4:
        return 250.0f;

    default:
        std::cerr << __func__ << "(): " << "wrong Num.\n";
        break;
    }

    return -1.0f;
}
//------------------------------------------------------------------------------------
// Получаем расход энергии в секунду для доп систем (GameAdvancedProtectionSystem)
//------------------------------------------------------------------------------------
float GetShipProtectionSystemEnergyUse(int Num)
{
    switch (Num) {
    // нано роботы
    case 1:
        return 10.0f;
    // спец защитный слой
    case 2:
        return 0.0f;
    // щит
    case 3:
        return 50.0f;
    // отражатель
    case 4:
        return 100.0f;

    default:
        std::cerr << __func__ << "(): " << "wrong Num.\n";
        break;
    }

    return 0.0f;
}
//------------------------------------------------------------------------------------
// Получаем расход энергии в секунду для двигателей GameEngineSystem
//------------------------------------------------------------------------------------
float GetShipEngineSystemEnergyUse(int Num)
{
    switch (Num) {
    // обычные двигатели
    case 1:
        return 5.0f;
    // фатоновые
    case 2:
        return 10.0f;
    // плазменные
    case 3:
        return 30.0f;
    // на антиматерии
    case 4:
        return 60.0f;

    default:
        std::cerr << __func__ << "(): " << "wrong Num.\n";
        break;
    }

    return 0.0f;
}






//------------------------------------------------------------------------------------
// Инициализация корабля игрока
//------------------------------------------------------------------------------------
void InitGamePlayerShip()
{
    // создаем корабль игрока по настройкам в профайле
    VoiceMissileDetected = 0;
    VoiceMissileDetectedStatus = false;
    VoiceWeaponMalfunction = 0;
    SoundLowLife = 0;

    int TMPGameEnemyArmorPenalty = GameEnemyArmorPenalty;
    GameEnemyArmorPenalty = 1;

    // если не создано, здесь будет ноль скорее всего
    if (GameConfig().Profile[CurrentProfile].ShipHull == 0) {
        std::cerr << __func__ << "(): " << "Error, Pilot Profile not created.\n";
    }

    PlayerFighter = CreateEarthSpaceFighter(GameConfig().Profile[CurrentProfile].ShipHull);
    auto sharedPlayerFighter = PlayerFighter.lock();
    if (!sharedPlayerFighter) {
        return;
    }

    sharedPlayerFighter->ShipShake.emplace_back(sVECTOR3D{0.0f, 0.0f, 1.0f},
                                                0,
                                                0.035f,
                                                [] () {return vw_fRand0() * 0.1f;});

    sharedPlayerFighter->ObjectStatus = eObjectStatus::Player;
    sharedPlayerFighter->ArmorInitialStatus *= GameConfig().Profile[CurrentProfile].ShipHullUpgrade;
    sharedPlayerFighter->ArmorCurrentStatus = GameConfig().Profile[CurrentProfile].ArmorStatus;
    sharedPlayerFighter->ShowStatus = false;

    // создаем оружие
    for (unsigned i=0; i<sharedPlayerFighter->WeaponSlots.size(); i++) {
        if (GameConfig().Profile[CurrentProfile].Weapon[i] != 0) {
            if (SetEarthSpaceFighterWeapon(PlayerFighter, i+1, GameConfig().Profile[CurrentProfile].Weapon[i])) {
                if (auto sharedWeapon = sharedPlayerFighter->WeaponSlots[i].Weapon.lock()) {
                    sharedWeapon->Ammo = GameConfig().Profile[CurrentProfile].WeaponAmmo[i];
                }
                sharedPlayerFighter->WeaponSlots[i].YAngle = -GameConfig().Profile[CurrentProfile].WeaponSlotYAngle[i];
            }
        }
    }

    // создаем системы (визуальные)
    SetEarthSpaceFighterEngine(PlayerFighter, GameEngineSystem);
    SetEarthSpaceFighterArmor(PlayerFighter, GameConfig().Profile[CurrentProfile].ShipHullUpgrade - 1);

    GameEnemyArmorPenalty = TMPGameEnemyArmorPenalty;



    float Width2 = sharedPlayerFighter->Width/2.0f;
    float Length2 = sharedPlayerFighter->Length/2.0f;
    ShildRadius = vw_sqrtf(Width2*Width2+Length2*Length2);
    ShildEnergyStatus = 0.0f;
    ShildStartHitStatus = 0.0f;



    if (GameConfig().Profile[CurrentProfile].AdvancedProtectionSystem == 3) {
        Shild1 = vw_CreateParticleSystem();
        if (auto sharedShild1 = Shild1.lock()) {
            sharedShild1->ColorStart.r = 0.20f;
            sharedShild1->ColorStart.g = 0.50f;
            sharedShild1->ColorStart.b = 0.10f;
            sharedShild1->ColorEnd.r = 0.20f;
            sharedShild1->ColorEnd.g = 0.50f;
            sharedShild1->ColorEnd.b = 0.10f;
            sharedShild1->AlphaStart = 1.00f;
            sharedShild1->AlphaEnd = 0.00f;
            sharedShild1->SizeStart = 0.60f;
            sharedShild1->SizeVar = 0.10f;
            sharedShild1->SizeEnd = 0.10f;
            sharedShild1->Speed = 0.00f;
            sharedShild1->SpeedOnCreation = -1.00f;
            sharedShild1->Theta = 360.00f;
            sharedShild1->Life = 1.00f;
            sharedShild1->ParticlesPerSec = (int)(40 * ShildRadius);
            sharedShild1->CreationType = eParticleCreationType::Sphere;
            sharedShild1->CreationSize = sVECTOR3D{ShildRadius, 0.05f * ShildRadius, ShildRadius};
            sharedShild1->DeadZone = ShildRadius - 0.05f;
            sharedShild1->AlphaShowHide = true;
            sharedShild1->IsMagnet = true;
            sharedShild1->MagnetFactor = -3.0f;
            sharedShild1->Texture = GetPreloadedTextureAsset("gfx/flare1.tga");
            sharedShild1->Direction = sVECTOR3D{0.0f, 0.0f, -1.0f};
            sharedShild1->SetStartLocation(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        }

        ShildStartHitStatus = 100.0f;
        ShildEnergyStatus = 1.0f;
    }
    if (GameConfig().Profile[CurrentProfile].AdvancedProtectionSystem == 4) {
        Shild1 = vw_CreateParticleSystem();
        if (auto sharedShild1 = Shild1.lock()) {
            sharedShild1->ColorStart.r = 0.50f;
            sharedShild1->ColorStart.g = 0.50f;
            sharedShild1->ColorStart.b = 1.00f;
            sharedShild1->ColorEnd.r = 0.50f;
            sharedShild1->ColorEnd.g = 0.50f;
            sharedShild1->ColorEnd.b = 1.00f;
            sharedShild1->AlphaStart = 0.50f;
            sharedShild1->AlphaEnd = 0.00f;
            sharedShild1->SizeStart = 0.40f;
            sharedShild1->SizeVar = 0.10f;
            sharedShild1->SizeEnd = 0.20f;
            sharedShild1->Speed = 0.00f;
            sharedShild1->SpeedOnCreation = -1.00f;
            sharedShild1->Theta = 360.00f;
            sharedShild1->Life = 1.00f;
            sharedShild1->ParticlesPerSec = (int)(40 * ShildRadius);
            sharedShild1->CreationType = eParticleCreationType::Sphere;
            sharedShild1->CreationSize = sVECTOR3D{ShildRadius, 0.05f * ShildRadius, ShildRadius};
            sharedShild1->DeadZone = ShildRadius - 0.05f;
            sharedShild1->IsMagnet = true;
            sharedShild1->AlphaShowHide = true;
            sharedShild1->MagnetFactor = 2.5f;
            sharedShild1->Texture = GetPreloadedTextureAsset("gfx/flare1.tga");
            sharedShild1->Direction = sVECTOR3D{0.0f, 0.0f, -1.0f};
            sharedShild1->SetStartLocation(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        }

        Shild2 = vw_CreateParticleSystem();
        if (auto sharedShild2 = Shild2.lock()) {
            sharedShild2->ColorStart.r = 0.50f;
            sharedShild2->ColorStart.g = 0.50f;
            sharedShild2->ColorStart.b = 1.00f;
            sharedShild2->ColorEnd.r = 0.50f;
            sharedShild2->ColorEnd.g = 0.50f;
            sharedShild2->ColorEnd.b = 1.00f;
            sharedShild2->AlphaStart = 0.70f;
            sharedShild2->AlphaEnd = 0.10f;
            sharedShild2->SizeStart = 0.50f;
            sharedShild2->SizeVar = 0.10f;
            sharedShild2->SizeEnd = 0.30f;
            sharedShild2->Speed = 0.00f;
            sharedShild2->SpeedOnCreation = -1.00f;
            sharedShild2->Theta = 360.00f;
            sharedShild2->Life = 1.00f;
            sharedShild2->ParticlesPerSec = (int)(5 * ShildRadius);
            sharedShild2->CreationType = eParticleCreationType::Sphere;
            sharedShild2->CreationSize = sVECTOR3D{ShildRadius, 0.05f * ShildRadius, ShildRadius};
            sharedShild2->DeadZone = ShildRadius - 0.05f;
            sharedShild2->IsMagnet = true;
            sharedShild2->MagnetFactor = 20.0f;
            sharedShild2->Texture = GetPreloadedTextureAsset("gfx/flare1.tga");
            sharedShild2->Direction = sVECTOR3D{0.0f, 0.0f, -1.0f};
        }

        ShildStartHitStatus = 150.0f;
        ShildEnergyStatus = 1.0f;
    }



    // предварительная установка, полностью заряженное устройство
    CurrentPlayerShipEnergy = GetShipMaxEnergy(GamePowerSystem);

    // предварительно иним состояния управления
    LastMouseX = -1;
    LastMouseY = -1;
    MoveFB = 0.0f;
    MoveLR = 0.0f;
    CurrentMovementVel = sVECTOR3D{0.0f, 0.0f, 0.0f};

    // сброс стрельбы...
    PrimaryGroupCurrentFireWeaponNum = 1;
    PrimaryGroupCurrentFireWeaponDelay = 0.0f;
    SecondaryGroupCurrentFireWeaponNum = 1;
    SecondaryGroupCurrentFireWeaponDelay = 0.0f;
}








//------------------------------------------------------------------------------------
// Основная процедура обработки состояния корабля игрока
//------------------------------------------------------------------------------------
void GamePlayerShip()
{
    auto sharedPlayerFighter = PlayerFighter.lock();
    if (!sharedPlayerFighter) {
        return;
    }

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // проверяем, корабль живой еще, или сбили и нужно его удалить...
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (sharedPlayerFighter->ArmorCurrentStatus <= 0.0f) {
        // редкий случай
        if (UndeadDebugMode) {
            sharedPlayerFighter->ArmorCurrentStatus = sharedPlayerFighter->ArmorInitialStatus;
        } else {
            // делаем взрыв
            // + 10.0f движение камеры
            CreateSpaceExplosion(*sharedPlayerFighter, 31, sharedPlayerFighter->Location, sharedPlayerFighter->Speed+10.0f, PlayerDeadObjectPieceNum);

            // включаем музыку и отображение "миссия провалена"
            PlayMusicTheme(eMusicTheme::FAILED, 2000, 2000);

            // удаляем и уходим отсюда
            ReleaseSpaceShip(PlayerFighter);

            SetupMissionFailedText(20.0f);

            return;
        }
    }



    // голос выводим только в игре! в меню включается пауза
    // и если не закончился уровень
    if (GameContentTransp < 0.99f && !GameMissionCompleteStatus) {
        int WarningMessagesCount = 0;

        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // Вывод голосового предупреждения, если навелась ракета
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        bool CheckStatus{false};
        ForEachProjectile([&] (const cProjectile &Projectile) {
            if (auto sharedTarget = Projectile.Target.lock()) {
                if (sharedTarget.get() == sharedPlayerFighter.get() &&
                    // homing missile targeted on this ship, but not homing mine
                    (Projectile.Num < 26 || Projectile.Num > 29)) {
                    CheckStatus = true;
                }
            }
        });

        if (CheckStatus) {
            // проверяем, действительно еще играем (играем только 1 раз!)
            if (!vw_IsSoundAvailable(VoiceMissileDetected) && !VoiceMissileDetectedStatus) {
                VoiceMissileDetected = PlayVoicePhrase(eVoicePhrase::MissileDetected, 1.0f);
                VoiceMissileDetectedStatus = true;
            }

            // визуальный вывод - выводим постоянно
            vw_SetFontSize(24);
            int TmpFontSize = (GameConfig().InternalWidth - vw_TextWidthUTF32(vw_GetTextUTF32("Missile Detected"))) / 2;
            vw_DrawTextUTF32(TmpFontSize, 720 - 40*WarningMessagesCount, 0, 0, 1.0f, sRGBCOLOR{eRGBCOLOR::orange}, CurrentAlert3, vw_GetTextUTF32("Missile Detected"));
            ResetFontSize();
            WarningMessagesCount++;
        } else {
            if (CurrentAlert3 == 1.0f) { // сделали полный цикл , предыдущее значение счетчика было минимальное
                VoiceMissileDetectedStatus = false;
            } else if (VoiceMissileDetectedStatus) {
                // визуальный вывод - выводим постоянно
                vw_SetFontSize(24);
                int TmpFontSize = (GameConfig().InternalWidth - vw_TextWidthUTF32(vw_GetTextUTF32("Missile Detected"))) / 2;
                vw_DrawTextUTF32(TmpFontSize, 720 - 40*WarningMessagesCount, 0, 0, 1.0f, sRGBCOLOR{eRGBCOLOR::orange}, CurrentAlert3, vw_GetTextUTF32("Missile Detected"));
                ResetFontSize();
                WarningMessagesCount++;
            }
        }



        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // Вывод голосового предупреждения если возможно столкновение
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        bool CollisionDetected = false;
        ForEachSpaceObject([&CollisionDetected, &sharedPlayerFighter] (const cSpaceObject &tmpSpace, eSpaceCycle &Command) {
            // test with "immortal" big asteroids
            if (tmpSpace.ObjectType == eObjectType::BigAsteroid
                && vw_SphereSphereCollision(sharedPlayerFighter->Radius, sharedPlayerFighter->Location,
                                            tmpSpace.Radius, tmpSpace.Location, tmpSpace.PrevLocation)
                && vw_SphereAABBCollision(tmpSpace.AABB, tmpSpace.Location, sharedPlayerFighter->Radius,
                                          sharedPlayerFighter->Location, sharedPlayerFighter->PrevLocation)) {
                CollisionDetected = true;
                Command = eSpaceCycle::Break;
            }
        });
        ForEachGroundObject([&CollisionDetected, &sharedPlayerFighter] (const cGroundObject &tmpGround, eGroundCycle &Command) {
            // test with "immortal" civilian buildings
            if (tmpGround.ObjectType == eObjectType::CivilianBuilding
                && vw_SphereSphereCollision(sharedPlayerFighter->Radius, sharedPlayerFighter->Location,
                                            tmpGround.Radius, tmpGround.Location, tmpGround.PrevLocation)
                && vw_SphereAABBCollision(tmpGround.AABB, tmpGround.Location, sharedPlayerFighter->Radius,
                                          sharedPlayerFighter->Location, sharedPlayerFighter->PrevLocation)
                && vw_SphereOBBCollision(tmpGround.OBB.Box, tmpGround.OBB.Location, tmpGround.Location,
                                         tmpGround.CurrentRotationMat, sharedPlayerFighter->Radius,
                                         sharedPlayerFighter->Location, sharedPlayerFighter->PrevLocation)) {
                CollisionDetected = true;
                Command = eGroundCycle::Break;
            }
        });
        if (CollisionDetected) {
            // голос, ворнинг, можем столкнуться с объектом
            // проверяем, действительно еще играем
            if (!vw_IsSoundAvailable(VoiceWarningCollisionDetected)) {
                VoiceWarningCollisionDetected = PlayVoicePhrase(eVoicePhrase::Warning, 1.0f);
            }

            // визуальный вывод - выводим постоянно
            vw_SetFontSize(24);
            int TmpFontSize = (GameConfig().InternalWidth - vw_TextWidthUTF32(vw_GetTextUTF32("Collision Course Detected"))) / 2;
            vw_DrawTextUTF32(TmpFontSize, 720 - 40*WarningMessagesCount, 0, 0, 1.0f, sRGBCOLOR{eRGBCOLOR::red}, CurrentAlert3, vw_GetTextUTF32("Collision Course Detected"));
            ResetFontSize();
            WarningMessagesCount++;
        }





        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // Вывод голосового предупреждения, если в оружие нет пуль
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        if (!sharedPlayerFighter->WeaponSlots.empty()) { // если вообще есть оружие
            for (const auto &tmpWeaponSlot : sharedPlayerFighter->WeaponSlots) {
                // если нажали стрелять, а патронов нет в одном из орудий
                if (auto sharedWeapon = tmpWeaponSlot.Weapon.lock()) {
                    if (tmpWeaponSlot.SetFire
                        && sharedWeapon->Ammo <= 0
                        && !vw_IsSoundAvailable(VoiceWeaponMalfunction)) { // проверяем, действительно еще играем
                        VoiceWeaponMalfunction = PlayVoicePhrase(eVoicePhrase::WeaponMalfunction, 1.0f);
                    }
                }
            }
        }


        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // Звуковое оповещение, если жизни менее 10%
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        // если меньше 10% нужно бить тревогу
        if (sharedPlayerFighter->ArmorCurrentStatus < sharedPlayerFighter->ArmorInitialStatus / 10.0f
            && !vw_IsSoundAvailable(SoundLowLife)) {
            // если не играем, запускаем звук сирены
            SoundLowLife = PlayMenuSFX(eMenuSFX::WarningLowLife, 1.0f);
        }
    }





    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // управление кораблем - движение
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (GameContentTransp < 1.0f) { // если не в меню нажимают
        // получаем данные, для обоих типов управления
        // уже получили данные, нужно игнорировать остальные источникик
        bool NeedSkip = false;

        // mouse + joystick (since we emulate mouse movements)
        if (GameConfig().MouseControl) {
            SDL_GetMouseState(&LastMouseXR, &LastMouseYR);

            int X, Y;
            vw_GetMousePos(X, Y);
            if (LastMouseX == -1 && LastMouseY == -1) {
                LastMouseX = X;
                LastMouseY = Y;
            } else {
                if (X != LastMouseX || Y != LastMouseY) {
                    // 0.9+0.1 = 1.0 - минимум, всегда 1.0 должен быть!
                    float Koef = 0.9f + GameConfig().ControlSensivity / 10.0f;

                    // при любом реальном разрешении у нас x и y меняются с учетом AspectRatio
                    float AWw2 = GameConfig().InternalWidth / 2.0f;
                    float AHw2 = GameConfig().InternalHeight / 2.0f;

                    MoveFB += (-(Y-LastMouseY)/AHw2)*Koef;
                    MoveLR += ( (X-LastMouseX)/AWw2)*Koef;

                    NeedSkip = true;
                }

                LastMouseX = X;
                LastMouseY = Y;
            }

        }

        // клавиатура
        if (!NeedSkip) {
            if (vw_GetKeyStatus(GameConfig().KeyBoardDown)) {
                MoveFB -= 2.0f * (GameConfig().ControlSensivity / 10.0f) * sharedPlayerFighter->TimeDelta;
            }
            if (vw_GetKeyStatus(GameConfig().KeyBoardUp)) {
                MoveFB += 2.0f * (GameConfig().ControlSensivity / 10.0f) * sharedPlayerFighter->TimeDelta;
            }
            if (vw_GetKeyStatus(GameConfig().KeyBoardLeft)) {
                MoveLR -= 2.0f * (GameConfig().ControlSensivity / 10.0f) * sharedPlayerFighter->TimeDelta;
            }
            if (vw_GetKeyStatus(GameConfig().KeyBoardRight)) {
                MoveLR += 2.0f * (GameConfig().ControlSensivity / 10.0f) * sharedPlayerFighter->TimeDelta;
            }
        }



        // дополнительная проверка, т.к. можем выйти выйти за пределы
        if (MoveFB < -1.0f) {
            MoveFB = -1.0f;
        }
        if (MoveFB > 1.0f) {
            MoveFB = 1.0f;
        }
        if (MoveLR < -1.0f) {
            MoveLR = -1.0f;
        }
        if (MoveLR > 1.0f) {
            MoveLR = 1.0f;
        }






        // находим конечную точку перемещения
        sVECTOR3D PlayerFighterEndLocation;
        if (GameConfig().InternalWidth == 1024.0f) {
            PlayerFighterEndLocation = sVECTOR3D{-(73.15f-sharedPlayerFighter->Width/2.0f+MoveFB*(20.05f-sharedPlayerFighter->Length/6.0f))*MoveLR,
                                                 0.0f,
                                                 (46.0f-sharedPlayerFighter->Length/2.0f)*MoveFB};
        } else {
            PlayerFighterEndLocation = sVECTOR3D{-(70.0f-sharedPlayerFighter->Width/2.0f+MoveFB*(23.2f-sharedPlayerFighter->Length/6.0f))*MoveLR,
                                                 0.0f,
                                                 (46.0f-sharedPlayerFighter->Length/2.0f)*MoveFB};
        }

        PlayerFighterEndLocation += GetCameraCoveredDistance();

        // если есть двигатель
        if (GameEngineSystem != 0) {
            // в зависимости от типа управления выполняем действия
            if (GameConfig().Profile[CurrentProfile].SpaceShipControlMode == 1) {
                // аркадный режим

                // запускаем маневровые двигатели, если тянем корабль в сторону
                if ((int)sharedPlayerFighter->Location.x > (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = true;
                    PlayerFighterRightEng = false;
                }
                if ((int)sharedPlayerFighter->Location.x < (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = false;
                    PlayerFighterRightEng = true;
                }
                // если не двигаем, останавливаем маневровые двигатели
                if ((int)sharedPlayerFighter->Location.x == (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = false;
                    PlayerFighterRightEng = false;
                }


                // находим расстояние
                sVECTOR3D PlayerFighterNewDirection = PlayerFighterEndLocation - sharedPlayerFighter->Location;
                float EndLocationDistance = PlayerFighterNewDirection.Length();

                // находим направление движения
                PlayerFighterNewDirection.Normalize();

                float SimMoveSpeed = EndLocationDistance;

                if (SimMoveSpeed > 30.0f) {
                    SimMoveSpeed = 30.0f;
                }

                SimMoveSpeed = SimMoveSpeed*4.0f*sharedPlayerFighter->TimeDelta;


                // получаем текущее движение
                CurrentMovementVel = PlayerFighterNewDirection^SimMoveSpeed;

                // проверка
                float MaxSpeed = CurrentMovementVel.Length();
                CurrentMovementVel.Normalize();
                if (MaxSpeed > 30.0f) {
                    MaxSpeed = 30.0f;
                }

                CurrentMovementVel = CurrentMovementVel^MaxSpeed;

            } else {
                // симулятивный режим


                // запускаем маневровые двигатели, если тянем корабль в сторону
                if ((int)sharedPlayerFighter->Location.x > (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = true;
                    PlayerFighterRightEng = false;
                }
                if ((int)sharedPlayerFighter->Location.x < (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = false;
                    PlayerFighterRightEng = true;
                }
                // если не двигаем, останавливаем маневровые двигатели
                if ((int)sharedPlayerFighter->Location.x == (int)PlayerFighterEndLocation.x) {
                    PlayerFighterLeftEng = false;
                    PlayerFighterRightEng = false;
                }


                // находим расстояние
                sVECTOR3D PlayerFighterNewDirection = PlayerFighterEndLocation - sharedPlayerFighter->Location;
                float EndLocationDistance = PlayerFighterNewDirection.Length();

                // находим направление движения
                PlayerFighterNewDirection.Normalize();

                float SimMoveSpeed = EndLocationDistance;

                if (SimMoveSpeed > sharedPlayerFighter->MaxSpeed) {
                    SimMoveSpeed = sharedPlayerFighter->MaxSpeed;
                }

                SimMoveSpeed = SimMoveSpeed*(sharedPlayerFighter->MaxAcceler/14.0f)*sharedPlayerFighter->TimeDelta;


                // получаем текущее движение
                CurrentMovementVel = PlayerFighterNewDirection^SimMoveSpeed;

                // проверка
                float MaxSpeed = CurrentMovementVel.Length();
                CurrentMovementVel.Normalize();
                if (MaxSpeed > sharedPlayerFighter->MaxSpeed) {
                    MaxSpeed = sharedPlayerFighter->MaxSpeed;
                }

                CurrentMovementVel = CurrentMovementVel^MaxSpeed;

            }
        }

        // переносим корабль
        sVECTOR3D CurrentVel = sharedPlayerFighter->Location + CurrentMovementVel;
        CurrentVel.y = 0.0f;
        sharedPlayerFighter->SetLocationArcadePlayer(CurrentVel);



        // если стандартный аспект рейшен, надо перемещать камеру в дополнении к перемещению корабля
        if (GameConfig().InternalWidth == 1024.0f) {
            float DeviationSize = 14.55f;

            if (sharedPlayerFighter->Location.x < 0.0f) {
                float Diff = sharedPlayerFighter->Location.x / 3.5f;
                if (Diff < -DeviationSize) {
                    Diff = -DeviationSize;
                }

                sVECTOR3D TMPCameraLocation;
                vw_GetCameraLocation(&TMPCameraLocation);
                TMPCameraLocation.x = Diff;
                vw_SetCameraLocation(TMPCameraLocation);
            } else {
                float Diff = sharedPlayerFighter->Location.x / 3.5f;
                if (Diff > DeviationSize) {
                    Diff = DeviationSize;
                }

                sVECTOR3D TMPCameraLocation;
                vw_GetCameraLocation(&TMPCameraLocation);
                TMPCameraLocation.x = Diff;
                vw_SetCameraLocation(TMPCameraLocation);
            }
        }

    }





    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // управление кораблем - стрельба
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (GameContentTransp < 0.5f // если не в меню нажимают
        && !sharedPlayerFighter->WeaponSlots.empty()) { // если вообще есть оружие

        int PrimCount = 0;
        float PrimTime = 0.0f;
        int SecCount = 0;
        float SecTime = 0.0f;

        PrimaryGroupCurrentFireWeaponDelay -= sharedPlayerFighter->TimeDelta;
        SecondaryGroupCurrentFireWeaponDelay -= sharedPlayerFighter->TimeDelta;

        // находим кол-во оружия в группах
        for (unsigned i = 0; i < sharedPlayerFighter->WeaponSlots.size(); i++) {
            if (GameConfig().Profile[CurrentProfile].Weapon[i] != 0) { // если это оружие установлено

                if (GameConfig().Profile[CurrentProfile].WeaponControl[i] == 1
                    || GameConfig().Profile[CurrentProfile].WeaponControl[i] == 3) {
                    if (auto sharedWeapon = sharedPlayerFighter->WeaponSlots[i].Weapon.lock()) {
                        PrimCount++;
                        PrimTime += sharedWeapon->NextFireTime;
                    }
                }

                if (GameConfig().Profile[CurrentProfile].WeaponControl[i] == 2
                    || GameConfig().Profile[CurrentProfile].WeaponControl[i] == 3) {
                    if (auto sharedWeapon = sharedPlayerFighter->WeaponSlots[i].Weapon.lock()) {
                        SecCount++;
                        SecTime += sharedWeapon->NextFireTime;
                    }
                }
            }
        }

        int PrimNum = 0;
        int SecNum = 0;

        for (unsigned i = 0; i < sharedPlayerFighter->WeaponSlots.size(); i++) {
            if (GameConfig().Profile[CurrentProfile].Weapon[i] != 0) { // если это оружие установлено

                sharedPlayerFighter->WeaponSlots[i].SetFire = false;

                // получаем данные, в какую группу относится
                bool primary_fire = false;
                bool secondary_fire = false;
                if (GameConfig().Profile[CurrentProfile].WeaponControl[i] == 1
                    || GameConfig().Profile[CurrentProfile].WeaponControl[i] ==3) {
                    primary_fire = true;
                }
                if (GameConfig().Profile[CurrentProfile].WeaponControl[i] == 2
                    || GameConfig().Profile[CurrentProfile].WeaponControl[i] ==3) {
                    secondary_fire = true;
                }

                // мышка
                if (GameConfig().MouseControl) {
                    // primary fire
                    if (primary_fire) {
                        if (vw_GetMouseButtonStatus(GameConfig().MousePrimary)) {
                            if (GameConfig().Profile[CurrentProfile].PrimaryWeaponFireMode == 1) {
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                            } else {
                                PrimNum++;
                                if (PrimaryGroupCurrentFireWeaponNum == PrimNum
                                    && PrimaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                    PrimaryGroupCurrentFireWeaponDelay = PrimTime / (PrimCount * PrimCount);
                                    sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                    PrimaryGroupCurrentFireWeaponNum++;
                                    if (PrimaryGroupCurrentFireWeaponNum > PrimCount) {
                                        PrimaryGroupCurrentFireWeaponNum = 1;
                                    }
                                }
                            }
                        }
                    }

                    // secondary fire
                    if (secondary_fire) {
                        if (vw_GetMouseButtonStatus(GameConfig().MouseSecondary)) {
                            if (GameConfig().Profile[CurrentProfile].SecondaryWeaponFireMode == 1) {
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                            } else {
                                SecNum++;
                                if (SecondaryGroupCurrentFireWeaponNum == SecNum
                                    && SecondaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                    SecondaryGroupCurrentFireWeaponDelay = SecTime / (SecCount * SecCount);
                                    sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                    SecondaryGroupCurrentFireWeaponNum++;
                                    if (SecondaryGroupCurrentFireWeaponNum > SecCount) {
                                        SecondaryGroupCurrentFireWeaponNum = 1;
                                    }
                                }
                            }
                        }
                    }

                    // альтернативное управление
                    if (GameConfig().Profile[CurrentProfile].WeaponAltControl[i] == 2
                        && vw_GetMouseButtonStatus(GameConfig().Profile[CurrentProfile].WeaponAltControlData[i])) {
                        sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                    }
                }


                // джойстик
                if (isJoystickAvailable()) {
                    // primary fire
                    if (primary_fire) {
                        if (GetJoystickButton(GameConfig().JoystickPrimary)) {
                            if (GameConfig().Profile[CurrentProfile].PrimaryWeaponFireMode == 1) {
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                            } else {
                                PrimNum++;
                                if (PrimaryGroupCurrentFireWeaponNum == PrimNum
                                    && PrimaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                    PrimaryGroupCurrentFireWeaponDelay = PrimTime / (PrimCount * PrimCount);
                                    sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                    PrimaryGroupCurrentFireWeaponNum++;
                                    if (PrimaryGroupCurrentFireWeaponNum > PrimCount) {
                                        PrimaryGroupCurrentFireWeaponNum = 1;
                                    }
                                }
                            }
                        }
                    }

                    // secondary fire
                    if (secondary_fire) {
                        if (GetJoystickButton(GameConfig().JoystickSecondary)) {
                            if (GameConfig().Profile[CurrentProfile].SecondaryWeaponFireMode == 1) {
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                            } else {
                                SecNum++;
                                if (SecondaryGroupCurrentFireWeaponNum == SecNum
                                    && SecondaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                    SecondaryGroupCurrentFireWeaponDelay = SecTime / (SecCount * SecCount);
                                    sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                    SecondaryGroupCurrentFireWeaponNum++;
                                    if (SecondaryGroupCurrentFireWeaponNum > SecCount) {
                                        SecondaryGroupCurrentFireWeaponNum = 1;
                                    }
                                }
                            }
                        }
                    }

                    // альтернативное управление
                    if (GameConfig().Profile[CurrentProfile].WeaponAltControl[i] == 3
                        && GetJoystickButton(GameConfig().Profile[CurrentProfile].WeaponAltControlData[i])) {
                        sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                    }
                }

                // клавиатура

                // primary fire
                if (primary_fire) {
                    if (vw_GetKeyStatus(GameConfig().KeyBoardPrimary)) {
                        if (GameConfig().Profile[CurrentProfile].PrimaryWeaponFireMode == 1) {
                            sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                        } else {
                            PrimNum++;
                            if (PrimaryGroupCurrentFireWeaponNum == PrimNum
                                && PrimaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                PrimaryGroupCurrentFireWeaponDelay = PrimTime / (PrimCount * PrimCount);
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                PrimaryGroupCurrentFireWeaponNum++;
                                if (PrimaryGroupCurrentFireWeaponNum > PrimCount) {
                                    PrimaryGroupCurrentFireWeaponNum = 1;
                                }
                            }
                        }
                    }
                }

                // secondary fire
                if (secondary_fire) {
                    if (vw_GetKeyStatus(GameConfig().KeyBoardSecondary)) {
                        if (GameConfig().Profile[CurrentProfile].SecondaryWeaponFireMode == 1) {
                            sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                        } else {
                            SecNum++;
                            if (SecondaryGroupCurrentFireWeaponNum == SecNum
                                && SecondaryGroupCurrentFireWeaponDelay <= 0.0f) {
                                SecondaryGroupCurrentFireWeaponDelay = SecTime / (SecCount * SecCount);
                                sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                                SecondaryGroupCurrentFireWeaponNum++;
                                if (SecondaryGroupCurrentFireWeaponNum > SecCount) {
                                    SecondaryGroupCurrentFireWeaponNum = 1;
                                }
                            }
                        }
                    }
                }

                // альтернативное управление
                if (GameConfig().Profile[CurrentProfile].WeaponAltControl[i] == 1
                    && vw_GetKeyStatus(GameConfig().Profile[CurrentProfile].WeaponAltControlData[i])) {
                        sharedPlayerFighter->WeaponSlots[i].SetFire = true;
                }
            }
        }
    }





    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // управление и работа внутренних систем корабля
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // сделать:
    // учитывать, как работает двигатель... стоим или летим...
    // если не аркадный режим...
    if (GameSpaceShipControlMode != 1) {
        if (CurrentPlayerShipEnergy < GetShipEngineSystemEnergyUse(GameEngineSystem)*sharedPlayerFighter->TimeDelta) {
            sharedPlayerFighter->MaxSpeed = 0.0f;
            sharedPlayerFighter->MaxAcceler = 0.0f;
            sharedPlayerFighter->MaxSpeedRotate = 0.0f;
            // глушим двигатели
            for (auto &tmpEngine : sharedPlayerFighter->Engines) {
                if (auto sharedEngine = tmpEngine.lock()) {
                    sharedEngine->IsSuppressed = true;
                }
            }
            if (!sharedPlayerFighter->EnginesLeft.empty()) {
                for (auto &tmpEngineLeft : sharedPlayerFighter->EnginesLeft) {
                    if (auto sharedEngineLeft = tmpEngineLeft.lock()) {
                        sharedEngineLeft->IsSuppressed = true;
                    }
                }
            }
            if (!sharedPlayerFighter->EnginesRight.empty()) {
                for (auto &tmpEngineRight : sharedPlayerFighter->EnginesRight) {
                    if (auto sharedEngineRight = tmpEngineRight.lock()) {
                        sharedEngineRight->IsSuppressed = true;
                    }
                }
            }
        } else {
            sharedPlayerFighter->MaxSpeed = GetEnginePower(GameEngineSystem);
            sharedPlayerFighter->MaxAcceler = GetEngineAcceleration(GameEngineSystem);
            sharedPlayerFighter->MaxSpeedRotate = GetEngineRotatePower(GameEngineSystem);
            // запускаем прорисовку
            for (auto &tmpEngine : sharedPlayerFighter->Engines) {
                if (auto sharedEngine = tmpEngine.lock()) {
                    sharedEngine->IsSuppressed = false;
                }
            }
            if (!sharedPlayerFighter->EnginesLeft.empty()) {
                for (auto &tmpEngineLeft : sharedPlayerFighter->EnginesLeft) {
                    if (auto sharedEngineLeft = tmpEngineLeft.lock()) {
                        sharedEngineLeft->IsSuppressed = false;
                    }
                }
            }
            if (!sharedPlayerFighter->EnginesRight.empty()) {
                for (auto &tmpEngineRight : sharedPlayerFighter->EnginesRight) {
                    if (auto sharedEngineRight = tmpEngineRight.lock()) {
                        sharedEngineRight->IsSuppressed = false;
                    }
                }
            }
            CurrentPlayerShipEnergy -= GetShipEngineSystemEnergyUse(GameEngineSystem)*sharedPlayerFighter->TimeDelta;
        }
    }



    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // энергия для перезарядки и выстрела...
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // сейчас получаем всю энергию для перезарядки и выстрела
    // потом лучше будет переделать на постепенный отбор энергии
    for (unsigned i = 0; i < sharedPlayerFighter->WeaponSlots.size(); i++) {
        if (GameConfig().Profile[CurrentProfile].Weapon[i] != 0) {
            if (auto sharedWeapon = sharedPlayerFighter->WeaponSlots[i].Weapon.lock()) {
                if (sharedWeapon->CurrentEnergyAccumulated < sharedWeapon->EnergyUse) {
                    // если энергии не достаточно для зарядки орудия
                    if (CurrentPlayerShipEnergy < sharedWeapon->EnergyUse) {
                        // останавливаем перезарядку оружия
                        sharedWeapon->LastFireTime += sharedPlayerFighter->TimeDelta;
                        if (auto sharedFire = sharedWeapon->Fire.lock()) {
                            sharedFire->IsSuppressed = true;
                        }
                    } else {
                        // если энергии достаточно, все нормально берем ее и перезаряжаем оружие
                        sharedWeapon->CurrentEnergyAccumulated = sharedWeapon->EnergyUse;
                        CurrentPlayerShipEnergy -= sharedWeapon->EnergyUse;
                    }
                }
            }
        }
    }


    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // питание других (защитных) систем
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (CurrentPlayerShipEnergy >= GetShipEngineSystemEnergyUse(GameEngineSystem)*sharedPlayerFighter->TimeDelta) {

        switch (GameAdvancedProtectionSystem) {
        // нано роботы
        case 1:
            // восстанавливаем на 0.5% в секунду
            if (sharedPlayerFighter->ArmorCurrentStatus < sharedPlayerFighter->ArmorInitialStatus) {
                CurrentPlayerShipEnergy -= GetShipProtectionSystemEnergyUse(GameAdvancedProtectionSystem) * sharedPlayerFighter->TimeDelta;
                sharedPlayerFighter->ArmorCurrentStatus += (sharedPlayerFighter->ArmorInitialStatus / 200.0f) * sharedPlayerFighter->TimeDelta;
                if (sharedPlayerFighter->ArmorCurrentStatus > sharedPlayerFighter->ArmorInitialStatus) {
                    sharedPlayerFighter->ArmorCurrentStatus = sharedPlayerFighter->ArmorInitialStatus;
                }
            }
            break;
        // спец защитный слой
        case 2:
            break; // ничего не делаем
        // щит
        case 3:
            // восстанавливаем полностью за 4 секунды
            if (ShildEnergyStatus < 1.0f) {
                CurrentPlayerShipEnergy -= GetShipProtectionSystemEnergyUse(GameAdvancedProtectionSystem) * sharedPlayerFighter->TimeDelta;
                ShildEnergyStatus += 0.02f * sharedPlayerFighter->TimeDelta;
                if (ShildEnergyStatus > 1.0f) {
                    ShildEnergyStatus = 1.0f;
                }
            }
            break;
        // отражатель
        case 4:
            // восстанавливаем полностью за 2 секунды
            if (ShildEnergyStatus < 1.0f) {
                CurrentPlayerShipEnergy -= GetShipProtectionSystemEnergyUse(GameAdvancedProtectionSystem) * sharedPlayerFighter->TimeDelta;
                ShildEnergyStatus += 0.03f * sharedPlayerFighter->TimeDelta;
                if (ShildEnergyStatus > 1.0f) {
                    ShildEnergyStatus = 1.0f;
                }
            }
            break;
        }
    }


    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // управление визуализацией щитов-дефлекторов
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (auto sharedShild1 = Shild1.lock()) {
        sharedShild1->MoveSystem(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        sharedShild1->SetStartLocation(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        sharedShild1->RotateSystemAndParticlesByAngle(sharedPlayerFighter->Rotation);
        sharedShild1->ParticlesPerSec = (int)(40 * ShildEnergyStatus * ShildRadius);
    }
    if (auto sharedShild2 = Shild2.lock()) {
        sharedShild2->MoveSystem(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        sharedShild2->SetStartLocation(sharedPlayerFighter->Location + sharedPlayerFighter->OBB.Location);
        sharedShild2->RotateSystemAndParticlesByAngle(sharedPlayerFighter->Rotation);
        sharedShild2->ParticlesPerSec = (int)(5 * ShildEnergyStatus * ShildRadius);
    }


    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // если реактор - можем генерировать энергию, если баттарея - нет
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    CurrentPlayerShipEnergy += GetShipRechargeEnergy(GamePowerSystem)*sharedPlayerFighter->TimeDelta;
    if (CurrentPlayerShipEnergy > GetShipMaxEnergy(GamePowerSystem)) {
        CurrentPlayerShipEnergy = GetShipMaxEnergy(GamePowerSystem);
    }

}

} // astromenace namespace
} // viewizard namespace
