#include "zSaveLoad.h"

#include <types.h>
#include <string.h>
#include <stdio.h>

#include "iSystem.h"

#include "zUI.h"
#include "zGlobals.h"
#include "zGameState.h"
#include "zHud.h"
#include "zCamera.h"
#include "zGame.h"
#include "xParMgr.h"
#include "xCutscene.h"
#include "xDebug.h"
#include "xString.h"
#include "xTRC.h"
#include "xutil.h"

U32 saveSuccess;
F32 time_last;
F32 time_current;
F32 time_elapsed = 0.01f;
iTime t0;
iTime t1;
S32 promptSel;
S32 badCard;
S32 sAvailable;
S32 sNeeded;
S32 sAccessType;
U8 preAutoSaving;

S32 currentCard = -1;
S32 currentGame = -1;
F32 dontPoll = 1.0f;
S32 autoSaveCard = -1;

char currSceneStr[32] = "TEMP";
char sceneRead[32] = "0000";
zSaveLoadUI zSaveLoadUITable[62] = { { 0, 0, "ld gameslot group" },
                                     { 1, 0, "ld memcards group" },
                                     { 2, 0, "ld format prompt group" },
                                     { 3, 0, "ld mc missing group" },
                                     { 4, 0, "mnu3 ld mc1" },
                                     { 5, 0, "mnu3 ld mc2" },
                                     { 6, 0, "ld gameslot 0" },
                                     { 7, 0, "ld gameslot 1" },
                                     { 8, 0, "ld gameslot 2" },
                                     { 9, 0, "ld gameslot 3" },
                                     { 10, 0, "ld gameslot 4" },
                                     { 11, 0, "ld gameslot 5" },
                                     { 12, 0, "ld gameslot 6" },
                                     { 13, 0, "ld gameslot 7" },
                                     { 14, 0, "ld format prompt" },
                                     { 15, 0, "ld format yes" },
                                     { 16, 0, "ld format no" },
                                     { 17, 0, "ld mc missing" },
                                     { 18, 0, "ld nogames" },
                                     { 19, 0, "ld nogames" },
                                     { 20, 0, "mnu3 start group" },
                                     { 21, 0, "sv gameslot group" },
                                     { 22, 0, "sv memcards group" },
                                     { 23, 0, "sv mc1" },
                                     { 24, 0, "sv mc2" },
                                     { 25, 0, "sv gameslot 0" },
                                     { 26, 0, "sv gameslot 1" },
                                     { 27, 0, "sv gameslot 2" },
                                     { 28, 0, "sv gameslot 3" },
                                     { 29, 0, "sv gameslot 4" },
                                     { 30, 0, "sv gameslot 5" },
                                     { 31, 0, "sv gameslot 6" },
                                     { 32, 0, "sv gameslot 7" },
                                     { 33, 0, "sv format prompt" },
                                     { 34, 0, "sv overwrite" },
                                     { 35, 0, "sv overwrite damaged" },
                                     { 36, 0, "sv mc missing" },
                                     { 37, 0, "sv nospace" },
                                     { 38, 0, "sv nospacegame" },
                                     { 39, 0, "ld mc dontremove" },
                                     { 40, 0, "sv mc dontremove" },
                                     { 41, 0, "mnu4 mc dontremove" },
                                     { 42, 0, "ld badload" },
                                     { 43, 0, "sv badsave" },
                                     { 44, 0, "mnu3 badformat" },
                                     { 45, 0, "mnu3 badformatnocard" },
                                     { 46, 0, "mnu4 badformat" },
                                     { 47, 0, "mnu4 badformatnocard" },
                                     { 48, 0, "mnu3 format confirm" },
                                     { 49, 0, "mnu4 format confirm" },
                                     { 50, 0, "sv format group" },
                                     { 51, 0, "mnu3 disk free" },
                                     { 52, 0, "mnu4 disk free" },
                                     { 53, 0, "sv card damaged" },
                                     { 54, 0, "sv badsavenocard" },
                                     { 55, 0, "ld damaged card" },
                                     { 56, 0, "sv damaged card" },
                                     { 57, 0, "ld wrong device" },
                                     { 58, 0, "sv wrong device" },
                                     { 59, 0, "ld damaged save" },
                                     { 60, 0, "ld damaged save game" },
                                     { 0, 0, NULL } };

char* thumbIconMap[15] = { "ThumbIconHB", "ThumbIconJF", "ThumbIconBB", "ThumbIconGL",
                           "ThumbIconB1", "ThumbIconRB", "ThumbIconBC", "ThumbIconSM",
                           "ThumbIconB2", "ThumbIconKF", "ThumbIconGY", "ThumbIconDB",
                           "ThumbIconB3", "ThumbIconHB", "ThumbIconHB" };

void zUpdateThumbIcon()
{
    S32 i;
    S32 start;
    S32 end;
    if (gGameMode == eGameMode_Load)
    {
        start = 6;
        end = 9;
    }
    else
    {
        start = 25;
        end = 28;
    }

    for (i = start; i <= end; i++)
    {
        _zUI* gameSlot = (_zUI*)zSceneFindObject(zSaveLoadUITable[i].nameID);
        if (gameSlot != NULL && (gameSlot->uiFlags & 2) != 0)
        {
            U32 id = xStrHash(gGameMode == eGameMode_Load ? "MNU3 THUMBICON" : "MNU4 THUMBICON");
            _zUI* thumbIcon = (_zUI*)zSceneFindObject(id);
            if (thumbIcon != NULL)
            {
                S32 index = zSaveLoadGameTable[i - start].thumbIconIndex;
                if (index >= 0 && index < 15)
                {
                    if (thumbIcon->sasset->textureID != xStrHash(thumbIconMap[index]))
                    {
                        zChangeThumbIcon(thumbIconMap[index]);
                    }
                }
                else
                {
                    zChangeThumbIcon("");
                }
            }
        }
    }
}

void zSaveLoad_Tick()
{
    time_current = (1.0f / (GET_BUS_FREQUENCY() / 4)) * (float)iTimeGet();

    time_elapsed = time_current - time_last;
    if (time_elapsed < 0.0f)
    {
        time_elapsed = 1.0f / 60.f;
    }
    else if (time_elapsed > 0.1f)
    {
        time_elapsed = 1.0f / 60.f;
    }

    dontPoll = dontPoll - time_elapsed;
    t0 = t1;
    time_last = time_current;
    t1 = iTimeGet();
    sTimeCurrent = iTimeGet();
    sTimeElapsed = iTimeDiffSec(sTimeLast, sTimeCurrent);
    sTimeLast = sTimeCurrent;
    xPadUpdate(globals.currentActivePad, time_elapsed);
    iTRCDisk::CheckDVDAndResetState();
    xDrawBegin();
    xParMgrUpdate(time_elapsed);
    zSceneUpdate(time_elapsed);

    xMat4x3 playerMat;
    xMat4x3* ma = xEntGetFrame(&(xEnt)globals.player.ent);
    // This feels like a normal assignment but that calls the assignment operator function.
    *(U32*)&playerMat.right.x = *(U32*)&ma->right.x;
    *(U32*)&playerMat.right.y = *(U32*)&ma->right.y;
    *(U32*)&playerMat.right.z = *(U32*)&ma->right.z;
    *(U32*)&playerMat.flags = *(U32*)&ma->flags;
    *(U32*)&playerMat.up.x = *(U32*)&ma->up.x;
    *(U32*)&playerMat.up.y = *(U32*)&ma->up.y;
    *(U32*)&playerMat.up.z = *(U32*)&ma->up.z;
    *(U32*)&playerMat.pad1 = *(U32*)&ma->pad1;
    *(U32*)&playerMat.at.x = *(U32*)&ma->at.x;
    *(U32*)&playerMat.at.y = *(U32*)&ma->at.y;
    *(U32*)&playerMat.at.z = *(U32*)&ma->at.z;
    *(U32*)&playerMat.pad2 = *(U32*)&ma->pad2;
    *(U32*)&playerMat.pos.x = *(U32*)&ma->pos.x;
    *(U32*)&playerMat.pos.z = *(U32*)&ma->pos.z;
    *(U32*)&playerMat.pad3 = *(U32*)&ma->pad3;
    *(U32*)&playerMat.pos.y = *(U32*)&ma->pos.y;
    playerMat.pos.y += 0.6f;

    xSndSetListenerData(SND_LISTENER_CAMERA, &globals.camera.mat);
    xSndSetListenerData(SND_LISTENER_PLAYER, &playerMat);
    xSndUpdate();
    if (gGameMode == 6 && gGameState != 0)
    {
        zhud::update(sTimeElapsed);
    }

    zCameraUpdate(&globals.camera, time_elapsed);
    xCameraBegin(&globals.camera, 1);
    zUpdateThumbIcon();
    zSceneRender();
    xDebugUpdate();
    xDrawEnd();
    xCameraEnd(&globals.camera, time_elapsed, 1);
    iEnvEndRenderFX(0);
    xCameraShowRaster(&globals.camera);
    gFrameCount = gFrameCount + 1;
}

// Reordings/Float scheduling
S32 zSaveLoad_poll(S32 i)
{
    if (dontPoll < 0.0f)
    {
        dontPoll = 1.0f;
        if (!zSaveLoad_CardCheckSingle(currentCard))
        {
            promptSel = i;
            currentGame = 0;
        }
        return 1;
    }
    return 1;
}

void zSendEventToThumbIcon(U32 toEvent)
{
    const char* iconString = gGameMode == eGameMode_Load ? "MNU3 THUMBICON" : "MNU4 THUMBICON";
    zEntEvent(zSceneFindObject(xStrHash(iconString)), toEvent);
}

void zChangeThumbIcon(const char* icon)
{
    S32 arr[4];

    memset(arr, 0, sizeof(arr));
    arr[0] = xStrHash(icon);

    zEntEvent(zSceneFindObject(
                  xStrHash(gGameMode == eGameMode_Load ? "MNU3 THUMBICON" : "MNU4 THUMBICON")),
              eEventUIChangeTexture, (F32*)arr);
}

void zSaveLoadInit()
{
    zSaveLoadUITableInit(zSaveLoadUITable);
    zSaveLoadGameTableInit(zSaveLoadGameTable);
}

void zSaveLoadGameTableInit(zSaveLoadGame* saveTable)
{
    for (S32 i = 0; i < 3; i++)
    {
        saveTable[i].label[0] = 0;
        saveTable[i].date[0] = 0;
        saveTable[i].progress = 0;
        saveTable[i].size = 0;
        saveTable[i].thumbIconIndex = 0;
    }
}

void zSaveLoadUITableInit(zSaveLoadUI* saveTable)
{
    //Doesn't match the zSaveLoadUITable size for some reason
    for (S32 i = 0; i < 61; i++)
    {
        saveTable[i].nameID = xStrHash(saveTable[i].name);
    }
}

void zSaveLoad_UIEvent(S32 i, U32 toEvent)
{
    zEntEvent(zSceneFindObject(zSaveLoadUITable[i].nameID), toEvent);
}

st_XSAVEGAME_DATA* zSaveLoadSGInit(en_SAVEGAME_MODE mode)
{
    if (mode == XSG_MODE_LOAD)
    {
        zSaveLoad_UIEvent(0x27, eEventUIFocusOn);
    }
    else
    {
        zSaveLoad_UIEvent(0x28, eEventUIFocusOn);
    }

    for (S32 i = 0; i < 801; i++)
    {
        if (i % 200 == 0)
        {
            zSaveLoad_Tick();
        }
    }

    return xSGInit(mode);
}

S32 zSaveLoadSGDone(st_XSAVEGAME_DATA* data)
{
    if (data->mode == XSG_MODE_LOAD)
    {
        zSaveLoad_UIEvent(0x27, eEventUIFocusOff);
    }
    else
    {
        zSaveLoad_UIEvent(0x28, eEventUIFocusOff);
    }
    return xSGDone(data);
}

S32 zSaveLoad_getgame()
{
    return currentGame;
}

S32 zSaveLoad_getcard()
{
    return currentCard;
}

S32 zSaveLoad_getMCavailable()
{
    return sAvailable;
}

S32 zSaveLoad_getMCneeded()
{
    return sNeeded;
}

S32 zSaveLoad_getMCAccessType()
{
    return sAccessType;
}

S32 zSaveLoadGetAutoSaveCard()
{
    return autoSaveCard;
}

S32 format(S32 num, S32 mode)
{
    st_XSAVEGAME_DATA* data = zSaveLoadSGInit(XSG_MODE_LOAD);
    S32 tgtmax;

    S32 rc = 0;
    switch (xSGTgtCount(data, &tgtmax))
    {
    case 2:
        xSGTgtSelect(data, num);
        rc = xSGTgtFormatTgt(data, num, 0);
        zSaveLoadSGDone(data);
        if (rc == -1)
        {
            zSaveLoad_ErrorFormatCardYankedPrompt(mode);
            rc = 5;
        }
        else
        {
            if (rc == 0)
            {
                zSaveLoad_ErrorFormatPrompt(mode);
            }
            if (rc != 0)
            {
                rc = 1;
            }
        }
        break;
    case 1:
        S32 idx = xSGTgtPhysSlotIdx(data, 0);
        if (idx != num)
        {
            zSaveLoadSGDone(data);
            rc = 5;
        }
        else
        {
            if (idx ^ num)
            {
                zSaveLoadSGDone(data);
            }
            else
            {
                xSGTgtSelect(data, 0);
                rc = xSGTgtFormatTgt(data, num, 0);
                zSaveLoadSGDone(data);
                if (rc == 0)
                {
                    zSaveLoad_ErrorFormatPrompt(mode);
                }
            }
        }
        break;
    case 0:
        rc = 5;
        zSaveLoadSGDone(data);
        break;
    }
    return rc;
}

S32 CardtoTgt(S32 card)
{
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_LOAD);
    S32 tgtmax;

    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        // case 3:
        xSGDone(ldinst);
        return card;
    case 1:
        xSGDone(ldinst);
        return 0;
    case 0:
        xSGDone(ldinst);
        return -1;
    }
    return -1;
}

S32 zSaveLoad_CardCount()
{
    return 1;
}

S32 zSaveLoad_CardPrompt(S32 cardNumber)
{
    S32 i = 0x15;
    if (cardNumber == 1)
    {
        i = 0;
    }
    zSaveLoad_UIEvent(i, eEventEnable);

    i = 0x15;
    if (cardNumber == 1)
    {
        i = 0;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);

    i = 0x24;
    if (cardNumber == 1)
    {
        i = 0x11;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    i = 0x16;
    if (cardNumber == 1)
    {
        i = 1;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x24;
    if (cardNumber == 1)
    {
        i = 0x11;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);

    if (cardNumber == 1)
    {
        zSaveLoad_UIEvent(0, eEventDisable);
    }
    return promptSel;
}

S32 zSaveLoad_CardPromptFormat(S32 mode)
{
    S32 i = 0x15;
    if (mode == 1)
    {
        i = 0;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);

    i = 0x16;
    if (mode == 1)
    {
        i = 1;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x32, eEventUIFocusOn);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
        zSaveLoad_poll(3);
    }
    zSaveLoad_UIEvent(0x32, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardPromptSpace(S32 mode)
{
    S32 i = 0x15;
    if (mode == 1)
    {
        i = 0;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x16, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x25, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
        zSaveLoad_poll(3);
    }
    zSaveLoad_UIEvent(0x25, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardPromptGames(S32 mode)
{
    S32 i = 0x15;
    if (mode == 1)
    {
        i = 0;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(1, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x12, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
        zSaveLoad_poll(3);
    }
    zSaveLoad_UIEvent(0x12, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardPromptGameSlotEmpty()
{
    zSaveLoad_UIEvent(0, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x13, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }
    zSaveLoad_UIEvent(0x13, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardPromptOverwrite()
{
    zSaveLoad_UIEvent(0x15, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x22, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
        zSaveLoad_poll(5);
    }
    zSaveLoad_UIEvent(0x22, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardPromptOverwriteDamaged()
{
    zSaveLoad_UIEvent(0x15, eEventUIFocusOff_Unselect);
    zSaveLoad_UIEvent(0x23, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
        zSaveLoad_poll(5);
    }
    zSaveLoad_UIEvent(0x23, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_ErrorPrompt(S32 cardNumber)
{
    S32 i = 0x2b;
    if (cardNumber == 1)
    {
        i = 0x2a;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x2b;
    if (cardNumber == 1)
    {
        i = 0x2a;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_DamagedSaveGameErrorPrompt(S32 cardNumber)
{
    zSaveLoad_UIEvent(0x3c, eEventUIFocusOn_Select);
    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }
    zSaveLoad_UIEvent(0x3c, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardWrongDeviceErrorPrompt(S32 mode)
{
    int i = 0x3a;
    if (mode == 1)
    {
        i = 0x39;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x3a;
    if (mode == 1)
    {
        i = 0x39;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardDamagedErrorPrompt(S32 mode)
{
    int i = 0x38;
    if (mode == 1)
    {
        i = 0x37;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x38;
    if (mode == 1)
    {
        i = 0x37;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_SaveDamagedErrorPrompt(S32 cardNumber)
{
    zSaveLoad_UIEvent(0x35, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }
    zSaveLoad_UIEvent(0x35, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_CardYankedErrorPrompt(S32 cardNumber)
{
    zSaveLoad_UIEvent(0x36, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }
    zSaveLoad_UIEvent(0x36, eEventUIFocusOff_Unselect);
    return promptSel;
}

S32 zSaveLoad_ErrorFormatPrompt(S32 cardNumber)
{
    int i = 0x2e;
    if (cardNumber == 1)
    {
        i = 0x2c;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x2e;
    if (cardNumber == 1)
    {
        i = 0x2c;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    return 6;
}

S32 zSaveLoad_ErrorFormatCardYankedPrompt(S32 cardNumber)
{
    int i = 0x2f;
    if (cardNumber == 1)
    {
        i = 0x2d;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOn_Select);

    promptSel = -1;
    while (promptSel == -1)
    {
        zSaveLoad_Tick();
    }

    i = 0x2f;
    if (cardNumber == 1)
    {
        i = 0x2d;
    }
    zSaveLoad_UIEvent(i, eEventUIFocusOff_Unselect);
    return 6;
}

S32 zSaveLoad_CardCheckSingle(S32 cardNumber)
{
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_LOAD);
    S32 wrongDevice;
    S32 tgtmax;

    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        xSGDone(ldinst);
        return 1;
    case 1:
        int idx = xSGTgtPhysSlotIdx(ldinst, 0);
        xSGDone(ldinst);
        wrongDevice = iSGCheckForWrongDevice();
        if (wrongDevice >= 0 && wrongDevice == cardNumber)
        {
            return 9;
        }
        else
        {
            if (idx == cardNumber)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    case 0:
        wrongDevice = iSGCheckForWrongDevice();
        if (wrongDevice >= 0)
        {
            xSGDone(ldinst);
            return wrongDevice == cardNumber ? 9 : 0;
        }
        xSGDone(ldinst);
        return 0;
    }
    return -1;
}

S32 zSaveLoad_CardCheckFormattedSingle(S32 cardNumber)
{
    S32 rc;
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_LOAD);
    S32 tgtmax;

    rc = 0;
    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        switch (xSGTgtIsFormat(ldinst, cardNumber, NULL))
        {
        case -1:
            rc = 7;
            break;
        case 1:
            rc = 1;
            break;
        case 0:
        default:
            rc = 0;
            break;
        }
        break;
    case 1:
        S32 idx = xSGTgtPhysSlotIdx(ldinst, 0);
        if (idx != cardNumber)
        {
            rc = -1;
        }
        else if (!(idx ^ cardNumber))
        {
            switch (xSGTgtIsFormat(ldinst, 0, NULL))
            {
            case -1:
                rc = 7;
                break;
            case 1:
                rc = 1;
                break;
            case 0:
            default:
                rc = 0;
                break;
            }
        }
        break;
    case 0:
        rc = -1;
        break;
    }

    xSGDone(ldinst);
    return rc;
}

S32 zSaveLoad_CardCheckSpaceSingle_doCheck(st_XSAVEGAME_DATA* xsgdata, S32 cardNumber)
{
    int rc;

    if (xSGTgtIsFormat(xsgdata, cardNumber, 0) <= 0)
    {
        rc = 6;
    }
    else
    {
        // This makes no sense ¯\_(ツ)_/¯
        xSGTgtSelect(xsgdata, cardNumber);
        if (xSGTgtHasGameDir(xsgdata, cardNumber) == 1)
        {
            rc = xSGTgtHaveRoom(xsgdata, cardNumber, 0xcc00, -1, &sNeeded, &sAvailable, 0);
        }
        else
        {
            rc = xSGTgtHaveRoom(xsgdata, cardNumber, 0xcc00, -1, &sNeeded, &sAvailable, 0);
        }
        if (rc == 0)
        {
            rc = 0;
        }
    }
    return rc;
}

S32 zSaveLoad_CardCheckSpaceSingle(S32 cardNumber)
{
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_SAVE);
    S32 tgtmax;
    S32 rc;
    rc = 0;

    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        rc = zSaveLoad_CardCheckSpaceSingle_doCheck(ldinst, cardNumber);
        if (rc != 0)
        {
            rc = 1;
        }
        break;
    case 1:
        S32 idx = xSGTgtPhysSlotIdx(ldinst, 0);
        if (idx != cardNumber)
        {
            rc = 5;
        }
        if (!(idx ^ cardNumber))
        {
            rc = zSaveLoad_CardCheckSpaceSingle_doCheck(ldinst, 0);
        }
        break;
    case 0:
        rc = 5;
        break;
    }

    xSGDone(ldinst);
    return rc;
}

S32 zSaveLoad_CardCheckGamesSingle_doCheck(st_XSAVEGAME_DATA* xsgdata, S32 cardNumber)
{
    int rc;

    if (xSGTgtIsFormat(xsgdata, cardNumber, 0) <= 0)
    {
        rc = 6;
    }
    else
    {
        xSGTgtSelect(xsgdata, cardNumber);
        if (!xSGTgtHasGameDir(xsgdata, cardNumber))
        {
            rc = 0;
        }
        else
        {
            rc = xSGTgtHaveRoom(xsgdata, cardNumber, 0xcc00, -1, &sNeeded, &sAvailable, 0);
            if (rc == 0)
            {
                rc = 0;
            }
        }
    }
    return rc;
}

S32 zSaveLoad_CardCheckGamesSingle(S32 cardNumber)
{
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_LOAD);
    S32 tgtmax;
    int rc = 0;

    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        rc = zSaveLoad_CardCheckGamesSingle_doCheck(ldinst, cardNumber);
        if (rc != 0)
        {
            rc = 1;
        }
        break;
    case 1:
        S32 idx = xSGTgtPhysSlotIdx(ldinst, 0);
        if (idx != cardNumber)
        {
            rc = 5;
        }
        if (!(idx ^ cardNumber))
        {
            rc = zSaveLoad_CardCheckGamesSingle_doCheck(ldinst, 0);
        }
        break;
    case 0:
        rc = 5;
        break;
    }

    xSGDone(ldinst);
    return rc;
}

S32 zSaveLoad_CardCheckSlotEmpty_hasGame_doCheck(st_XSAVEGAME_DATA* xsgdata, S32 cardNumber,
                                                 S32 gameNumber)
{
    S32 rc;

    if (xSGTgtIsFormat(xsgdata, cardNumber, 0) <= 0)
    {
        rc = -1;
    }
    else
    {
        xSGTgtSelect(xsgdata, cardNumber);
        rc = xSGGameIsEmpty(xsgdata, gameNumber);
        if (rc != 0)
        {
            rc = 0;
        }
        else if (rc == 0)
        {
            rc = 1;
        }
    }
    return rc;
}

S32 zSaveLoad_CardCheckSlotEmpty_hasGame(S32 cardNumber, S32 gameNumber)
{
    st_XSAVEGAME_DATA* ldinst = xSGInit(XSG_MODE_LOAD);
    S32 tgtmax;
    int rc = 0;

    switch (xSGTgtCount(ldinst, &tgtmax))
    {
    case 2:
        rc = zSaveLoad_CardCheckSlotEmpty_hasGame_doCheck(ldinst, cardNumber, gameNumber);
        if (rc != 10 && rc != 0)
        {
            rc = 1;
        }
        break;
    case 1:
        S32 idx = xSGTgtPhysSlotIdx(ldinst, 0);
        if (idx != cardNumber)
        {
            rc = -1;
        }
        if (!(idx ^ cardNumber))
        {
            rc = zSaveLoad_CardCheckSlotEmpty_hasGame_doCheck(ldinst, 0, gameNumber);
        }
        break;
    case 0:
        rc = -1;
        break;
    }
    xSGDone(ldinst);
    return rc;
}

S32 zSaveLoad_CardCheckSlotOverwrite_Free(S32 cardNumber, S32 gameNumber)
{
    S32 i = zSaveLoad_CardCheckSlotEmpty_hasGame(cardNumber, gameNumber);
    switch (i)
    {
    case -1:
        // Won't match by simply returning -1
        return i;
    case 0:
        return 1;
    case 1:
        return 0;
    case 10:
        return 10;
    default:
        return 0;
    }
}

S32 zSaveLoad_CardCheck(S32 cardNumber, S32 mode)
{
    S32 cardResult = zSaveLoad_CardCheckSingle(cardNumber);
    while (cardResult == 0)
    {
        badCard = 1;

        cardResult = zSaveLoad_CardPrompt(mode);
        if (cardResult == 2 || cardResult == 4)
        {
            return 2;
        }
        cardResult = zSaveLoad_CardCheckSingle(cardNumber);
    }
    return cardResult == 9 ? 9 : 1;
}

S32 zSaveLoad_CardCheckFormatted(S32 cardNumber, S32 mode)
{
    S32 result;
    while (result = zSaveLoad_CardCheckFormattedSingle(cardNumber), result != 1)
    {
        badCard = 1;

        result = zSaveLoad_CardCheckFormattedSingle(cardNumber);
        if (result == -1)
        {
            return -1;
        }

        if (result == 7)
        {
            return 7;
        }

        result = zSaveLoad_CardPromptFormat(mode);
        if (result == 2 || result == 4)
        {
            return 2;
        }

        sAccessType = 3;
        S32 tmp = format(cardNumber, mode);
        sAccessType = 2;
        return tmp != 1 ? tmp : 11;
    }
    return 1;
}

S32 zSaveLoad_CardCheckValid(S32 cardNumber, S32 mode)
{
    if (mode == 1)
    {
        return zSaveLoad_CardCheckGames(cardNumber, mode);
    }
    else
    {
        return zSaveLoad_CardCheckSpace(cardNumber, mode);
    }
}

S32 zSaveLoad_CardCheckSpace(S32 cardNumber, S32 mode)
{
    S32 result = zSaveLoad_CardCheckSpaceSingle(cardNumber);
    while (result != 1)
    {
        badCard = 1;

        result = zSaveLoad_CardCheckSpaceSingle(cardNumber);
        if (result == -1 || result == 6 || result == 5)
        {
            return result;
        }

        result = zSaveLoad_CardPromptSpace(mode);
        if (result == 2 || result == 4)
        {
            return 2;
        }
    }
    return 1;
}

S32 zSaveLoad_CardCheckGames(S32 cardNumber, S32 mode)
{
    S32 result = zSaveLoad_CardCheckGamesSingle(cardNumber);
    while (result != 1)
    {
        badCard = 1;

        result = zSaveLoad_CardCheckGamesSingle(cardNumber);
        if (result == -1 || result == 6 || result == 5)
        {
            return result;
        }

        result = zSaveLoad_CardPromptGames(mode);
        if (result == 2 || result == 4)
        {
            return 2;
        }
    }
    return 1;
}

S32 zSaveLoad_CardCheckGameSlot(S32 cardNumber, S32 gameNumber, S32 mode)
{
    if (mode == 1)
    {
        return zSaveLoad_CardCheckSlotEmpty(cardNumber, gameNumber);
    }
    else
    {
        return zSaveLoad_CardCheckSlotOverwrite(cardNumber, gameNumber);
    }
}

S32 zSaveLoad_CardCheckSlotEmpty(S32 cardNumber, S32 gameNumber)
{
    S32 i = zSaveLoad_CardCheckSlotEmpty_hasGame(cardNumber, gameNumber);
    while (i != 1)
    {
        i = zSaveLoad_CardCheckSlotEmpty_hasGame(cardNumber, gameNumber);
        if (i == -1 || i == 6)
        {
            return i;
        }

        i = zSaveLoad_CardPromptGameSlotEmpty();
        if (i == 2 || i == 4)
        {
            return 2;
        }
    }
    return 1;
}

S32 zSaveLoad_CardCheckSlotOverwrite(S32 cardNumber, S32 gameNumber)
{
    // TODO: Figure out what this number means.
    S32 iVar1 = zSaveLoad_CardCheckSlotOverwrite_Free(cardNumber, gameNumber);
    // NOTE (Square): I'm not sure that this is supposed to be a loop. It doesn't make
    // sense to just break at the end and the condition feels like it should be an early return
    // but this matches and I don't know how else to generate the `b` instruction at 0x18
    while (iVar1 != 1 || iVar1 == 10)
    {
        if (iVar1 == -1 || iVar1 == 10)
        {
            return iVar1;
        }

        if (IsValidName(zSaveLoadGameTable[gameNumber].label))
        {
            iVar1 = zSaveLoad_CardPromptOverwrite();
        }
        else
        {
            iVar1 = zSaveLoad_CardPromptOverwriteDamaged();
        }

        if (iVar1 == 5)
        {
            return iVar1;
        }

        if (iVar1 == 2 || iVar1 == 4)
        {
            return 2;
        }
        break;
    }

    return 1;
}

S32 zSaveLoad_CardPick(S32 mode)
{
    S32 done = 0;
    S32 formatDone = 0x16;

    currentCard = -1;
    promptSel = -1;

    if (mode == 1)
    {
        formatDone = 1;
    }

    zSaveLoad_UIEvent(formatDone, eEventUIFocusOn);
    formatDone = 0x17;
    if (mode == 1)
    {
        formatDone = 4;
    }

    zSaveLoad_UIEvent(formatDone, eEventUISelect);

    while (!done && promptSel == -1)
    {
        if (currentCard != -1)
        {
            done = zSaveLoad_CardCheck(currentCard, mode);
            switch (done)
            {
            case 2:
                currentCard = -1;
                promptSel = -1;
                continue;
            case -1:
                done = 0;
                promptSel = -1;
                continue;
            case 9:
                zSaveLoad_CardWrongDeviceErrorPrompt(mode);
                done = -1;
                promptSel = -1;
                continue;
            }

            done = zSaveLoad_CardCheckFormatted(currentCard, mode);
            switch (done)
            {
            case 5:
                done = 0;
                if (promptSel == 3)
                {
                    promptSel = -1;
                    continue;
                }
                if (promptSel == 4)
                {
                    currentCard = -1;
                    return -1;
                }
                break;
            case 6:
                done = 0;
                promptSel = -1;
                continue;
            case 7:
                zSaveLoad_CardDamagedErrorPrompt(mode);
                done = -1;
                promptSel = -1;
                continue;
            case 2:
            case 4:
                currentCard = -1;
                promptSel = -1;
                continue;
            case -1:
                done = 0;
                promptSel = -1;
                continue;
            case 8:
            case 9:
            case 10:
            case 11:

            default:
                break;
            }

            done = zSaveLoad_CardCheckValid(currentCard, mode);
            switch (done)
            {
            case 5:
            case 6:
                done = 0;
                promptSel = -1;
                continue;
            case 2:
            case 4:
                promptSel = -1;
                currentGame = -1;
                break;
            case 0:
            case 1:
                break;
            case -1:
                done = 0;
                promptSel = -1;
                continue;
            case 3:
                break;
            }
        }

        zSaveLoad_Tick();
    }

    formatDone = 0x16;
    if (mode == 1)
    {
        formatDone = 1;
    }

    zSaveLoad_UIEvent(formatDone, eEventUIFocusOff_Unselect);

    if (done == 1)
    {
        return done;
    }
    if (promptSel != -1)
    {
        return promptSel;
    }
    return done;
}

bool IsValidName(char* name)
{
    if (strcmp((char*)name, "") == 0)
    {
        return 0;
    }

    for (char* p = name; *p != NULL; p++)
    {
        if ((*p < 'A' || *p > 'z') && (*p < '0' || *p > '9') && (*p != ' ' && *p != '\''))
        {
            return 0;
        }
    }
    return 1;
}

void BuildIt(char* build_txt, S32 i)
{
    char date1[32] = {};
    char date2[32];
    char biggerbuf[256] = {};
    char displaySizeUnit[32];

    if (IsValidName(zSaveLoadGameTable[i].label) == 0)
    {
        strcpy(build_txt, "Corrupt Save File\n\n");
    }
    else
    {
        strncpy(date1, zSaveLoadGameTable[i].date, 5);
        date1[2] = '/';
        sprintf(biggerbuf, "%s/%c%c", date1, zSaveLoadGameTable[i].date[8],
                zSaveLoadGameTable[i].date[9]);

        strncpy(date2, biggerbuf, sizeof(date2));
        date2[31] = NULL;

        memset(displaySizeUnit, 0, sizeof(displaySizeUnit));
        if (zSaveLoadGameTable[i].size == 1)
        {
            strcpy(displaySizeUnit, "block");
        }
        else
        {
            strcpy(displaySizeUnit, "blocks");
        }

        sprintf(biggerbuf, "%d%%  %s    (%d %s)\n%s", zSaveLoadGameTable[i].progress, date2,
                zSaveLoadGameTable[i].size, displaySizeUnit, zSaveLoadGameTable[i].label);
        strncpy(build_txt, biggerbuf, 0x80);
        date1[31] = NULL;
    }
}

void zSaveLoad_BuildName(char* name_txt, S32 idx)
{
    char desired[128];
    char current_name[128];

    BuildIt(desired, idx);

    S32 counter = 0;
    for (S32 i = 0; i < idx; i++)
    {
        BuildIt(current_name, i);
        if (strcmp(zSaveLoadGameTable[i].label, zSaveLoadGameTable[idx].label) == 0 &&
            strcmp(current_name, "Corrupt Save File\n\n") != 0)
        {
            counter++;
        }
    }

    if (counter > 0)
    {
        sprintf(name_txt, "%s (%d)", desired, counter);
        name_txt[0x3f] = NULL;
    }
    else
    {
        strcpy(name_txt, desired);
    }
}

S32 zSaveLoad_GameSelect(S32 mode)
{
    S32 done = 0;
    S32 i;
    st_XSAVEGAME_DATA* svinst;
    S32 use_tgt;

    badCard = 1;
    while (badCard != 0)
    {
        currentGame = -1;
        promptSel = -1;
        use_tgt = CardtoTgt(currentCard);

        if (mode == 1)
        {
            zSaveLoad_UIEvent(0, eEventEnable);
            zSaveLoad_UIEvent(0, eEventUIFocusOn);
            zSaveLoad_UIEvent(0x33, eEventUIFocusOn);
        }
        else
        {
            zSaveLoad_UIEvent(0x34, eEventUIFocusOn);
        }

        for (i = 0; i < 3; i++)
        {
            svinst = xSGInit(XSG_MODE_LOAD);
            xSGTgtSelect(svinst, use_tgt);
            if (xSGGameIsEmpty(svinst, i))
            {
                if (mode == 1)
                {
                    zSaveLoad_UIEvent(i + 6, eEventDisable);
                }
                strcpy(zSaveLoadGameTable[i].label, "Empty");
                strcpy(zSaveLoadGameTable[i].date, "Empty");
                zSaveLoadGameTable[i].progress = 0;
                zSaveLoadGameTable[i].size = 0;
                zSaveLoadGameTable[i].thumbIconIndex = -1;
            }
            else
            {
                strcpy(zSaveLoadGameTable[i].label, xSGGameLabel(svinst, i));
                if (strcmpi(zSaveLoadGameTable[i].label, zSceneGetLevelName('MNU3')) == 0)
                {
                    strcpy(zSaveLoadGameTable[i].label, zSceneGetLevelName('HB01'));
                }
                strcpy(zSaveLoadGameTable[i].date, xSGGameModDate(svinst, i));
                zSaveLoadGameTable[i].progress = xSGGameProgress(svinst, i);
                zSaveLoadGameTable[i].size = xSGGameSize(svinst, i);
                zSaveLoadGameTable[i].size = zSaveLoadGameTable[i].size + 0x1fff & 0xffffe000;
                zSaveLoadGameTable[i].size >>= 0xd;
                zSaveLoadGameTable[i].size += 3;

                zSendEventToThumbIcon(3);
                zSaveLoadGameTable[i].thumbIconIndex = xSGGameThumbIndex(svinst, i);
                if (strcmpi(zSaveLoadGameTable[i].label, "") == 0)
                {
                    zSaveLoadGameTable[i].thumbIconIndex = -1;
                }
            }
            xSGDone(svinst);
        }

        if (mode == 1)
        {
            for (S32 emptyCount = 6; emptyCount <= 8; emptyCount++)
            {
                if (zSaveLoad_slotIsEmpty(emptyCount - 6) == 0)
                {
                    zSaveLoad_UIEvent(emptyCount, eEventUISelect);
                    break;
                }
            }
        }
        else
        {
            zSaveLoad_UIEvent(0x15, eEventUIFocusOn);
            zSaveLoad_UIEvent(0x19, eEventUISelect);
        }
        badCard = 0;

        while (!done && promptSel == -1)
        {
            zSaveLoad_Tick();
            zSaveLoad_poll(3);
            if (promptSel == 4)
            {
                done = promptSel;
            }

            if (currentGame == -1)
            {
                continue;
            }

            done = zSaveLoad_CardCheck(currentCard, mode);
            if (done != 1)
            {
                currentGame = -1;
                badCard = 0;

                if (done == 9)
                {
                    zSaveLoad_CardWrongDeviceErrorPrompt(mode);
                    done = 9;
                }
                break;
            }

            done = zSaveLoad_CardCheckFormatted(currentCard, mode);
            if (done != 1)
            {
                switch (done)
                {
                case 5:
                    done = 0;
                    promptSel = -1;
                    continue;
                case 7:
                    zSaveLoad_CardDamagedErrorPrompt(mode);
                    promptSel = -1;
                    done = 7;
                    badCard = 0;
                    continue;
                case 2:
                    currentGame = -1;
                    badCard = 0;
                    continue;
                default:
                    continue;
                }
            }

            done = zSaveLoad_CardCheckValid(currentCard, mode);
            if (done != 1)
            {
                switch (done)
                {
                case 5:
                case 6:
                    done = 0;
                    promptSel = -1;
                    continue;
                case 2:
                    promptSel = -1;
                    currentGame = -1;
                    badCard = 0;
                    continue;
                default:
                    break;
                }
            }

            if (badCard == 0)
            {
                done = zSaveLoad_CardCheckGameSlot(currentCard, currentGame, mode);
                if (done != 1)
                {
                    switch (done)
                    {
                    case -1:
                    case 5:
                        done = 0;
                        promptSel = -1;
                        break;
                    case 2:
                        currentGame = -1;
                        break;
                    }
                }
            }
            else
            {
                done = 0;
                currentGame = -1;
            }
        }
    }

    if (mode == 1)
    {
        zSaveLoad_UIEvent(0, eEventEnable);
    }

    S32 index = 0x15;
    if (mode == 1)
    {
        index = 0;
    }
    zSaveLoad_UIEvent(index, eEventUIFocusOff_Unselect);
    return done;
}

U8 zSaveLoadGetPreAutoSave()
{
    return preAutoSaving;
}

void zSaveLoadPreAutoSave(bool onOff)
{
    preAutoSaving = onOff;
    if (onOff)
    {
        ResetButton::DisableReset();
    }
    else
    {
        ResetButton::EnableReset();
    }
}

void zSaveLoadAutoSaveUpdate()
{
    xBase* obj;
    s32 out1, out2;

    if (globals.autoSaveFeature == 0 || gGameMode == eGameMode_Pause)
    {
        return;
    }
    if (preAutoSaving == 0)
    {
        return;
    }

    S32 physicalSlot = xSGAutoSave_GetCache()->LastPhysicalSlot();
    if (physicalSlot >= 0)
    {
        autoSaveCard = physicalSlot;
        switch (CARDProbeEx(physicalSlot, &out1, &out2))
        {
        case 0:
        case -1:
            obj = zSceneFindObject(xStrHash("SAVING GAME ICON UI")); //"SAVING GAME ICON UI"
            if (obj != NULL)
            {
                zEntEvent(obj, eEventVisible);
            }
            break;
        default:
            obj = zSceneFindObject(xStrHash("SAVING GAME ICON UI")); //"SAVING GAME ICON UI"
            if (obj != NULL)
            {
                zEntEvent(obj, eEventInvisible);
            }

            obj = zSceneFindObject(xStrHash("MNU4 AUTO SAVE FAILED")); //"MNU4 AUTO SAVE FAILED"
            if (obj != NULL)
            {
                zEntEvent(obj, eEventVisible);
            }
            globals.autoSaveFeature = 0;
            zSaveLoadPreAutoSave(0);
            break;
        }
    }
}

S32 zSaveLoad_DoAutoSave()
{
    S32 success = 0;
    S32 teststat = 1;
    en_XSGASYNC_STATUS asstat = XSG_ASTAT_NOOP;
    st_XSAVEGAME_DATA* svinst;

    XSGAutoData* autodata = xSGAutoSave_GetCache();
    if (autodata == NULL)
    {
        return -1;
    }
    if (!autodata->IsValid())
    {
        return -1;
    }

    S32 use_tgt = CardtoTgt(autodata->LastTarget());
    S32 lastGame = autodata->LastGame();
    autodata->Discard();
    svinst = xSGInit(XSG_MODE_SAVE);
    xSGTgtSelect(svinst, use_tgt);
    xSGGameSet(svinst, lastGame);

    if (svinst == NULL)
    {
        teststat = 0;
    }

    zSceneSave(globals.sceneCur, 0);

    xSGAddSaveClient(svinst, 'ROOM', 0, xSGT_SaveInfoCB, xSGT_SaveProcCB);
    xSGAddSaveClient(svinst, 'PREF', 0, xSGT_SaveInfoPrefsCB, xSGT_SaveProcPrefsCB);

    xSerial_svgame_register(svinst, XSG_MODE_SAVE);
    U32 progress = zSceneCalcProgress();

    const char* area;
    if (globals.sceneCur->sceneID == 'PG12')
    {
        area = zSceneGetLevelName('HB01');
    }
    else
    {
        area = zSceneGetLevelName(globals.sceneCur->sceneID);
    }

    char label[64];
    strncpy(label, area, sizeof(label));
    if (!xSGSetup(svinst, lastGame, label, progress, 0, zSceneGetLevelIndex()))
    {
        teststat = 0;
    }

    if (teststat != 0)
    {
        S32 iprocess = xSGProcess(svinst);
        if (iprocess != 0)
        {
            asstat = xSGAsyncStatus(svinst, 1, 0, 0);
        }
        xSGGameIsEmpty(svinst, lastGame);
        xSGTgtHasGameDir(svinst, use_tgt);
        if (iprocess == 0)
        {
            teststat = false;
        }
        else
        {
            switch (asstat)
            {
            case XSG_ASTAT_INPROG:
                break;
            case XSG_ASTAT_SUCCESS:
                success = true;
                break;
            case XSG_ASTAT_FAILED:
                success = false;
                break;
            }
        }
    }

    if (teststat && xSGWrapup(svinst) == 0)
    {
        teststat = false;
    }

    if (xSGDone(svinst) == 0)
    {
        teststat = false;
    }

    if ((success) && (teststat))
    {
        if (autodata != NULL)
        {
            S32 idx = xSGTgtPhysSlotIdx(svinst, use_tgt);
            autodata->SetCache(use_tgt, lastGame, idx);
            globals.autoSaveFeature = 1;
        }
        return 1;
    }
    else
    {
        if (autodata != NULL)
        {
            autodata->Discard();
        }
        globals.autoSaveFeature = 0;
        return -1;
    }
}

// Reordering at beginning
S32 zSaveLoad_SaveGame()
{
    S32 success = false;
    S32 teststat = true;
    en_XSGASYNC_STATUS asstat = XSG_ASTAT_NOOP;
    S32 use_tgt = CardtoTgt(currentCard);
    S32 use_game = currentGame;
    autoSaveCard = currentCard;

    st_XSAVEGAME_DATA* xsgdata = zSaveLoadSGInit(XSG_MODE_SAVE);
    if (xSGCheckMemoryCard(xsgdata, currentCard) == 0)
    {
        zSaveLoadSGDone(xsgdata);
        return 8;
    }

    xSGTgtSelect(xsgdata, use_tgt);
    xSGGameSet(xsgdata, use_game);
    if (xsgdata == NULL)
    {
        teststat = false;
    }

    zSceneSave(globals.sceneCur, NULL);

    xSGAddSaveClient(xsgdata, 'ROOM', NULL, xSGT_SaveInfoCB, xSGT_SaveProcCB);
    xSGAddSaveClient(xsgdata, 'PREF', NULL, xSGT_SaveInfoPrefsCB, xSGT_SaveProcPrefsCB);

    xSerial_svgame_register(xsgdata, XSG_MODE_SAVE);

    U32 progress = zSceneCalcProgress();
    char label[64];
    const char* area;

    if (globals.sceneCur->sceneID == 'PG12')
    {
        area = zSceneGetLevelName('HB01');
    }
    else
    {
        area = zSceneGetLevelName(globals.sceneCur->sceneID);
    }

    strncpy(label, area, sizeof(label));

    if (!xSGSetup(xsgdata, use_game, label, progress, 0, zSceneGetLevelIndex()))
    {
        teststat = false;
    }

    en_XSG_WHYFAIL whyFail = XSG_WHYERR_NONE;
    if (teststat)
    {
        S32 rc = xSGProcess(xsgdata);
        if (rc)
        {
            asstat = xSGAsyncStatus(xsgdata, 1, &whyFail, NULL);
        }

        xSGGameIsEmpty(xsgdata, use_game);
        xSGTgtHasGameDir(xsgdata, use_tgt);
        if (!rc)
        {
            teststat = false;
        }
        else
        {
            switch (asstat)
            {
            case XSG_ASTAT_SUCCESS:
                success = true;
                break;
            case XSG_ASTAT_FAILED:
                success = false;
                break;
            case XSG_ASTAT_NOOP:
            case XSG_ASTAT_INPROG:
                break;
            }
        }
    }

    if (teststat && !xSGWrapup(xsgdata))
    {
        teststat = false;
    }

    if (!xSGCheckMemoryCard(xsgdata, currentCard))
    {
        zSaveLoadSGDone(xsgdata);
        return 8;
    }

    if (!zSaveLoadSGDone(xsgdata))
    {
        teststat = false;
    }

    XSGAutoData* asg = xSGAutoSave_GetCache();
    if (success && teststat)
    {
        S32 idx = xSGTgtPhysSlotIdx(xsgdata, use_tgt);
        asg->SetCache(use_tgt, use_game, idx);
        globals.autoSaveFeature = 1;
        return 1;
    }
    asg->Discard();

    switch (whyFail)
    {
    case XSG_WHYERR_DAMAGE:
    case XSG_WHYERR_OTHER:
        return 7;
    case XSG_WHYERR_NOCARD:
    case XSG_WHYERR_CARDYANKED:
        return 8;
    default:
        return -1;
    }
}

// Reordering, causing different register use at the end
S32 zSaveLoad_LoadGame()
{
    S32 success = false;
    S32 teststat = true;
    S32 rc;
    en_XSGASYNC_STATUS asstat = XSG_ASTAT_NOOP;
    S32 use_tgt = CardtoTgt(currentCard);
    autoSaveCard = currentCard;

    st_XSAVEGAME_DATA* xsgdata = zSaveLoadSGInit(XSG_MODE_LOAD);
    xSGTgtSelect(xsgdata, use_tgt);
    xSGGameSet(xsgdata, currentGame);

    if (xsgdata == NULL)
    {
        teststat = false;
    }

    xSGAddLoadClient(xsgdata, 'ROOM', NULL, xSGT_LoadLoadCB);
    xSGAddLoadClient(xsgdata, 'PREF', NULL, xSGT_LoadPrefsCB);
    xSerial_svgame_register(xsgdata, XSG_MODE_LOAD);

    if (!xSGSetup(xsgdata))
    {
        teststat = false;
    }

    en_XSG_WHYFAIL whyFail = XSG_WHYERR_NONE;
    if (teststat)
    {
        rc = xSGProcess(xsgdata);
        if (rc)
        {
            asstat = xSGAsyncStatus(xsgdata, 1, &whyFail, NULL);
        }
        if (!rc)
        {
            teststat = false;
        }
        else
        {
            switch (asstat)
            {
            case XSG_ASTAT_SUCCESS:
                success = true;
                break;
            case XSG_ASTAT_FAILED:
                success = false;
                break;
            case XSG_ASTAT_NOOP:
            case XSG_ASTAT_INPROG:
                break;
            }
        }
    }

    if (teststat)
    {
        rc = xSGWrapup(xsgdata);
        if (rc < 0)
        {
            teststat = false;
            whyFail = XSG_WHYERR_OTHER;
        }
        else if (rc == 0)
        {
            teststat = false;
        }
    }

    if (!zSaveLoadSGDone(xsgdata))
    {
        teststat = false;
    }

    XSGAutoData* asg = xSGAutoSave_GetCache();
    S32 use_game = currentCard;
    if (success && teststat)
    {
        S32 idx = xSGTgtPhysSlotIdx(xsgdata, use_tgt);
        asg->SetCache(use_tgt, use_game, idx);
        globals.autoSaveFeature = 1;
        return 1;
    }
    asg->Discard();

    switch (whyFail)
    {
    case XSG_WHYERR_OTHER:
        return 7;
    default:
        return -1;
    }
}

U32 zSaveLoad_LoadLoop()
{
    zSaveLoadInit();
    sAccessType = 1;
    S32 state = 0;
    while (state != 6)
    {
        switch (state)
        {
        case 0:
            while (zSaveLoad_CardCount() == 0)
            {
                zSaveLoad_CardPrompt(1);
            }
            zSaveLoad_UIEvent(0x14, eEventUIFocusOff_Unselect);
            state = 1;
            break;
        case 1:
            switch (zSaveLoad_CardPick(1))
            {
            case 1:
                state = 2;
                break;
            case 4:
                state = 8;
                break;
            case -1:
                state = 0;
                break;
            case 2:
                break;
            }
            break;
        case 2:
            switch (zSaveLoad_GameSelect(1))
            {
            case 2:
            case 4:
            case 5:
            case 6:
            case 7:
            case 9:
                state = 1;
                break;
            case 1:
                state = 9;
                break;
            case -1:
                zSaveLoad_ErrorPrompt(1);
                state = 1;
                break;
            }
            break;
        case 3:
            break;
        case 8:
            zGameModeSwitch(eGameMode_Title);
            zGameStateSwitch(0);
            state = 6;
            break;
        case 9:
            switch (zSaveLoad_LoadGame())
            {
            case 1:
                zGameModeSwitch(eGameMode_Game);
                zGameStateSwitch(0);
                state = 6;
                globals.autoSaveFeature = 1;
                break;
            case 7:
                zSaveLoad_DamagedSaveGameErrorPrompt(1);
                state = 1;
                break;
            default:
                zSaveLoad_ErrorPrompt(1);
                state = 1;
            }
        }
    }

    zSendEventToThumbIcon(eEventInvisible);
    return (U32)sceneRead[0] << 0x18 | (U32)sceneRead[1] << 0x10 | (U32)sceneRead[2] << 0x8 |
           (U32)sceneRead[3];
}

// Scheduling meme on the return
U32 zSaveLoad_SaveLoop()
{
    zSaveLoadInit();
    S32 state = 0;
    saveSuccess = 0;

    while (state != 6)
    {
        switch (state)
        {
        case 0:
            while (zSaveLoad_CardCount() == 0)
            {
                zSaveLoad_CardPrompt(0);
            }
            state = 1;
            break;
        case 1:
            switch (zSaveLoad_CardPick(0))
            {
            case 1:
                state = 2;
                break;
            case 4:
                if (gGameState != 0)
                {
                    gGameMode = eGameMode_Pause;
                    state = 6;

                    xBase* sendTo = zSceneFindObject(xStrHash("PAUSE OPTIONS BKG GROUP"));
                    zEntEvent(sendTo, eEventUIFocusOn_Select);

                    sendTo = zSceneFindObject(xStrHash("PAUSE OPTIONS GROUP"));
                    zEntEvent(sendTo, eEventUIFocusOn);

                    sendTo = zSceneFindObject(xStrHash("PAUSE OPTION MGR UIF"));
                    zEntEvent(sendTo, eEventUIFocusOn_Select);

                    sendTo = zSceneFindObject(xStrHash("PAUSE OPTION SAVE UIF"));
                    zEntEvent(sendTo, eEventUIFocusOn_Select);
                }
                else
                {
                    gGameMode = eGameMode_Title;
                    state = 6;

                    xBase* sendTo = zSceneFindObject(xStrHash("MNU3 START CREATE NEW GAME NO"));
                    zEntEvent(sendTo, eEventUIFocusOn);

                    sendTo = zSceneFindObject(xStrHash("MNU3 START CREATE NEW GAME NO"));
                    zEntEvent(sendTo, eEventUIUnselect);

                    sendTo = zSceneFindObject(xStrHash("MNU3 START CREATE NEW GAME YES"));
                    zEntEvent(sendTo, eEventUIFocusOn_Select);

                    sendTo = zSceneFindObject(xStrHash("MNU3 START CREATE NEW GAME"));
                    zEntEvent(sendTo, eEventUIFocusOn);

                    sendTo = zSceneFindObject(xStrHash("BLUE ALPHA 1 UI"));
                    zEntEvent(sendTo, eEventInvisible);
                }
                break;
            case 11:
                state = 10;
                currentGame = 0;
                break;
            default:
                state = 0;
            }
            break;
        case 2:
            sAccessType = 2;
            switch (zSaveLoad_GameSelect(0))
            {
            case 2:
            case 4:
            case 5:
            case 6:
            case 7:
            case 9:
                state = 1;
                break;
            case 1:
                state = 10;
                break;
            case -1:
                zSaveLoad_ErrorPrompt(0);
                state = 1;
                break;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            break;
        case 11:
            zGameModeSwitch(eGameMode_Game);
            zGameStateSwitch(1);
            state = 6;
            break;
        case 10:
            S32 tmp = zSaveLoad_SaveGame();
            if (tmp == 1)
            {
                zGameModeSwitch(eGameMode_Game);
                zGameStateSwitch(1);
                saveSuccess = 1;
                state = 6;

                xBase* sendTo = zSceneFindObject(xStrHash("MNU4 SAVE COMPLETED"));
                zEntEvent(sendTo, eEventVisible);
            }
            else
            {
                switch (tmp)
                {
                case 7:
                    zSaveLoad_SaveDamagedErrorPrompt(0);
                    break;
                case 8:
                    zSaveLoad_CardYankedErrorPrompt(0);
                    break;
                case 10:
                    zSaveLoad_CardPromptSpace(0);
                    break;
                default:
                    zSaveLoad_ErrorPrompt(0);
                    break;
                }
                state = 1;
            }
            break;
        }
    }

    sAccessType = 0;
    return saveSuccess;
}

void zSaveLoad_DispatchCB(U32 dispatchEvent, const F32* toParam)
{
    switch (dispatchEvent)
    {
    case 0xa8:
        promptSel = 4;
        break;
    case 0xaa:
        promptSel = 3;
        break;
    case 0xab:
        currentCard = (int)*toParam;
        en_SAVEGAME_MODE mode = XSG_MODE_LOAD;
        if (gGameMode == eGameMode_Save)
        {
            mode = XSG_MODE_SAVE;
        }
        st_XSAVEGAME_DATA* data = xSGInit(mode);
        zSaveLoad_CardCheckSpaceSingle_doCheck(data, currentCard);
        xSGDone(data);
        break;
    case 0xac:
        currentGame = (int)*toParam;
        break;
    case 0xad:
        promptSel = 3;
        break;
    }
}

S32 xSGT_SaveInfoCB(void* vp, st_XSAVEGAME_DATA* xsgdata, S32* need, S32* most)
{
    *need = xSGWriteStrLen(currSceneStr);
    *most = *need << 1;
    return 1;
}

S32 xSGT_SaveProcCB(void* vp, st_XSAVEGAME_DATA* xsgdata, st_XSAVEGAME_WRITECONTEXT* wctxt)
{
    if (globals.sceneCur->sceneID == 'PG12')
    {
        strcpy(currSceneStr, xUtil_idtag2string('HB01', 0));
    }
    else
    {
        strcpy(currSceneStr, xUtil_idtag2string(globals.sceneCur->sceneID, 0));
    }
    return xSGWriteData(xsgdata, wctxt, currSceneStr, 1, strlen(currSceneStr)) + 1;
}

S32 xSGT_SaveInfoPrefsCB(void* p1, st_XSAVEGAME_DATA* data, S32* i, S32* j)
{
    *i = 16;
    *j = *i * 2;
    return 1;
}

S32 xSGT_SaveProcPrefsCB(void* vp, st_XSAVEGAME_DATA* xsgdata, st_XSAVEGAME_WRITECONTEXT* wctxt)
{
    int sum = 0;
    sum += xSGWriteData(xsgdata, wctxt, &gSnd.stereo, 1);
    sum += xSGWriteData(xsgdata, wctxt, &gSnd.categoryVolFader[2], 1);
    sum += xSGWriteData(xsgdata, wctxt, &gSnd.categoryVolFader[0], 1);
    sum += xSGWriteData(xsgdata, wctxt, &globals.option_vibration, 1);
    return sum + 1;
}

S32 xSGT_LoadLoadCB(void* vp, st_XSAVEGAME_DATA* xsgdata, st_XSAVEGAME_READCONTEXT* rctxt, U32 ui,
                    S32 i)
{
    char bigbuf[32] = {};
    S32 compdiff = 0;

    xSGReadData(xsgdata, rctxt, bigbuf, 1, strlen(currSceneStr));
    if (strlen(currSceneStr) != strlen(bigbuf))
    {
        compdiff = 1;
    }
    if (compdiff == 0)
    {
        strcpy(sceneRead, bigbuf);
    }

    return compdiff == 0;
}

S32 xSGT_LoadPrefsCB(void* vp, st_XSAVEGAME_DATA* xsgdata, st_XSAVEGAME_READCONTEXT* rctxt, U32 ui,
                     S32 i)
{
    U32 stereo;

    xSGReadData(xsgdata, rctxt, &stereo, 1);
    xSGReadData(xsgdata, rctxt, &gSnd.categoryVolFader[2], 1);
    xSGReadData(xsgdata, rctxt, &gSnd.categoryVolFader[0], 1);
    xSGReadData(xsgdata, rctxt, &globals.option_vibration, 1);
    if (globals.option_vibration != 0)
    {
        xPadRumbleEnable(globals.currentActivePad, 1);
    }
    else
    {
        xPadRumbleEnable(globals.currentActivePad, 0);
    }
    return 1;
}

U32 zSaveLoad_slotIsEmpty(U32 slot)
{
    // TODO: Fix this hardcoded offset once string generation is correct
    char* label = zSaveLoadGameTable[slot].label;
    return strcmp(label, "ld gameslot group" + 0x49c) == 0 ? 1 : 0;
}

S32 XSGAutoData::LastPhysicalSlot()
{
    return this->lastPhysicalSlot;
}

S32 XSGAutoData::LastGame()
{
    return this->lastGame;
}

S32 XSGAutoData::LastTarget()
{
    return this->lastTarg;
}
