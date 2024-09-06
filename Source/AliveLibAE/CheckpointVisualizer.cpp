#include "CheckpointVisualizer.hpp"
#include "ScreenManager.hpp"
#include "PsxDisplay.hpp"
#include "Events.hpp"
#include "Game.hpp"
#include <stdlib.hpp>
#include "Abe.hpp"

const s16 kCheckpoint[36] = {
    8, 0, -6, 1,
    -6,
    12,
    0,
    13,
    0,
    0,
    6,
    1,
    6,
    -11,
    0,
    -12,
    0,
    2,
    -5,
    11,
    -1,
    11,
    1,
    2,
    5,
    -1,
    5,
    -10,
    1,
    -10,
    -1,
    -1,
    -5,
    0,
    0,
    0};

CheckpointVisualizer* CheckpointVisualizer::ctor(FP xpos, FP ypos, FP spriteScale)
{
    LOG_INFO("create checkpoint visualizer at xpos " << FP_GetExponent(xpos) << " ypos " << FP_GetExponent(ypos));
    BaseGameObject_ctor_4DBFA0(TRUE, 0);

    SetType(AETypes::eCheckpointVisualizer);

    field_6_flags.Set(BaseGameObject::eDrawable_Bit4);
    field_6_flags.Set(BaseGameObject::eSurviveDeathReset_Bit9);

    gObjList_drawables_5C1124->Push_Back(this);

    mInitialXPos = xpos;
    mInitialYPos = ypos;

    mCurrentXPos = xpos;
    mCurrentYPos = ypos;

    mSpriteScale = spriteScale;

    mLayer = sActiveHero_5C1B68->field_20_animation.field_C_render_layer;

    return this;
}

void CheckpointVisualizer::VScreenChanged()
{
    if (gMap_5C3030.field_0_current_level != gMap_5C3030.field_A_level ||
        gMap_5C3030.field_2_current_path != gMap_5C3030.field_C_path)
    {
        field_6_flags.Set(BaseGameObject::eDead_Bit3);
        field_6_flags.Clear(BaseGameObject::eSurviveDeathReset_Bit9);
    }
}

void CheckpointVisualizer::VUpdate()
{
    if (sNum_CamSwappers_5C1B66 != 0)
    {
        return;
    }

    mRender = gMap_5C3030.Is_Point_In_Current_Camera_4810D0(gMap_5C3030.field_0_current_level, gMap_5C3030.field_2_current_path, mInitialXPos, mInitialYPos, 0);

    mCurrentYPos += FP_FromDouble(0.3) * Math_Sine_496DD0(static_cast<u8>(3 * sGnFrame_5C1B84));
}

void CheckpointVisualizer::VRender(PrimHeader** ppOt)
{
    if (!mRender)
    {
        return;
    }

    const FP camX = FP_NoFractional(pScreenManager_5BB5F4->field_20_pCamPos->field_0_x);
    const FP camY = FP_NoFractional(pScreenManager_5BB5F4->field_20_pCamPos->field_4_y);

    s16 xpos = 0;
    s16 ypos = 0;

    for (s16 counter = 0; counter < kCheckpoint[0]; counter++)
    {
        xpos = FP_GetExponent(((mCurrentXPos - camX) * FP_FromInteger(40) + FP_FromInteger(11)) / FP_FromInteger(23));
        ypos = FP_GetExponent(mCurrentYPos - camY);

        const FP x0 = FP_FromInteger(kCheckpoint[(4 * counter) + 1]) * mSpriteScale;
        const FP y0 = FP_FromInteger(kCheckpoint[(4 * counter) + 2]) * mSpriteScale;
        const FP x1 = FP_FromInteger(kCheckpoint[(4 * counter) + 3]) * mSpriteScale;
        const FP y1 = FP_FromInteger(kCheckpoint[(4 * counter) + 4]) * mSpriteScale;

        Line_G2* pLine = &mLines[gPsxDisplay_5C1130.field_C_buffer_index][counter];
        LineG2_Init(pLine);

        SetXY0(pLine, xpos + FP_GetExponent(x0), ypos + FP_GetExponent(y0));
        SetXY1(pLine, xpos + FP_GetExponent(x1), ypos + FP_GetExponent(y1));

        SetRGB0(pLine, mR & 0xFF, mG & 0xFF, mB & 0xFF);
        SetRGB1(pLine, mR & 0xFF, mG & 0xFF, mB & 0xFF);

        Poly_Set_SemiTrans_4F8A60(&pLine->mBase.header, TRUE);
        OrderingTable_Add_4F8AA0(OtLayer(ppOt, mLayer), &pLine->mBase.header);
    }

    Prim_SetTPage* pTPage = &mTPage[gPsxDisplay_5C1130.field_C_buffer_index];
    Init_SetTPage_4F5B60(pTPage, 1, 0, PSX_getTPage_4F60E0(TPageMode::e4Bit_0, TPageAbr::eBlend_1, 0, 0));

    OrderingTable_Add_4F8AA0(OtLayer(ppOt, mLayer), &pTPage->mBase);
     pScreenManager_5BB5F4->InvalidateRect_40EC90(
        0,
        0,
        320,
        480,
        pScreenManager_5BB5F4->field_3A_idx);
    //pScreenManager_5BB5F4->InvalidateRect_40EC90(
    //    xpos - 8,
    //    ypos - 8,
    //    xpos + 8,
    //    ypos + 8,
    //    pScreenManager_5BB5F4->field_3A_idx);
}

BaseGameObject* CheckpointVisualizer::VDestructor(s32 flags)
{
    if (field_6_flags.Get(BaseGameObject::eDrawable_Bit4))
    {
        gObjList_drawables_5C1124->Remove_Item(this);
    }

    BaseGameObject_dtor_4DBEC0();

    if (flags & 1)
    {
        ae_delete_free_495540(this);
    }
    return this;
}
