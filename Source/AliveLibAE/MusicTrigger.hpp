#pragma once

#include "FunctionFwd.hpp"
#include "BaseGameObject.hpp"
#include "Path.hpp"
#include "MusicController.hpp"

enum class MusicTriggerMusicType : __int16
{
    eUnknown_0 = 0,
    eDeathShort_1 = 1,
    eSecretAreaLong_2 = 2,
    eChase_3 = 3,
    eSlogChase_4 = 4,
    eChime_5 = 5,
    eSecretAreaShort_6 = 6
};

enum class TriggeredBy : __int16
{
    eTimer_0 = 0,
    eTouching_1 = 1
};

struct Path_MusicTrigger : public Path_TLV
{
    MusicTriggerMusicType field_10_music_type;
    TriggeredBy field_12_triggered_by;
    __int16 field_14_music_delay;
    __int16 field_16_pad;
};
ALIVE_ASSERT_SIZEOF_ALWAYS(Path_MusicTrigger, 0x18);

class MusicTrigger : public BaseGameObject
{
public:
    EXPORT BaseGameObject* ctor_47FE40(Path_MusicTrigger* pTlv, DWORD tlvInfo);
    EXPORT MusicTrigger* ctor_47FF10(MusicTriggerMusicType musicType, TriggeredBy triggeredBy, int /*not_used*/, __int16 delay);
    EXPORT void Init_47FFB0(MusicTriggerMusicType musicType, TriggeredBy triggeredBy, __int16 delay);
    EXPORT BaseGameObject* vdtor_47FEE0(signed int flags);
    EXPORT void dtor_4800C0();
    EXPORT void vScreenChange_4802A0();
    EXPORT void vUpdate_480140();

    virtual BaseGameObject* VDestructor(signed int flags) override;
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    int field_20_tlvInfo;
    enum Flags_24
    {
        e24_Bit1_TriggeredByTouching = 0x1,
        e24_Bit2_TriggeredByTimer = 0x2,
        e24_Bit3_SetMusicToNoneOnDtor = 0x4,
    };
    BitField16<Flags_24> field_24_flags;
    MusicController::MusicTypes field_26_music_type;
    int field_28_counter;
    PSX_Point field_2C_tl;
    PSX_Point field_30_br;
};
ALIVE_ASSERT_SIZEOF(MusicTrigger, 0x34);
