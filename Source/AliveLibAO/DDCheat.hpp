#pragma once

#include "BaseGameObject.hpp"
#include "FunctionFwd.hpp"
#include "FixedPoint.hpp"

namespace AO {

enum class DDCheatValueType : int
{
    eShort_1 = 1,
    eInt_2 = 2,
    eShort_4 = 4,
    eInt_6 = 6,
};

union DDCheatValue
{
    int* pInt;
    short* pShort;
    FP* pFixedPoint;

    DDCheatValue()
    {
        pInt = nullptr;
    }

    DDCheatValue(int* v)
    {
        pInt = v;
    }

    DDCheatValue(short* v)
    {
        pShort = v;
    }

    DDCheatValue(FP* v)
    {
        pFixedPoint = v;
    }
};

struct DDCheatProperty
{
    const char* Name;
    DDCheatValueType ValueType;
    DDCheatValue ValuePtr;
};
ALIVE_ASSERT_SIZEOF(DDCheatProperty, 12);

class DDCheat : public BaseGameObject
{
public:
    EXPORT DDCheat* ctor_4095D0();

    static EXPORT void ClearProperties_4095B0();

    EXPORT BaseGameObject* dtor_409710();

    virtual BaseGameObject* VDestructor(signed int flags) override;

    EXPORT DDCheat* Vdtor_40A380(signed int flags);

    void AddPropertyEntry(const char* text, DDCheatValueType valueType, DDCheatValue valuePtr);

    virtual void VScreenChanged() override;

    virtual void VUpdate() override;

    EXPORT void VUpdate_4098C0();

    EXPORT void ScreenShot_409720();

    EXPORT void Teleport_409CE0();

    EXPORT void Misc_409E90();

    EXPORT static int DebugFont_Printf_498B40(int idx, const char* formatStr, ...);

    EXPORT static int DebugStr_495990(const char* pStr, ...);

    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
    int field_24_input;
};
ALIVE_ASSERT_SIZEOF(DDCheat, 0x28);

ALIVE_VAR_EXTERN(int, gTweak_X_5076D8);
ALIVE_VAR_EXTERN(int, gTweak_Y_5076DC);

ALIVE_VAR_EXTERN(short, sRescuedMudokons_5076C0);
ALIVE_VAR_EXTERN(short, sKilledMudokons_5076BC);
ALIVE_VAR_EXTERN(short, showDebugCreatureInfo_5076E0);
ALIVE_VAR_EXTERN(short, sDDCheat_FlyingEnabled_50771C);

}
