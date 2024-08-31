#pragma once
#include "BaseAliveGameObject.hpp"

class CheckpointVisualizer final : public BaseGameObject
{
public:
    CheckpointVisualizer* ctor(FP xpos, FP ypos, FP spriteScale);
    BaseGameObject* VDestructor(s32 flags);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VRender(PrimHeader** ppOt) override;

private:
    FP mInitialXPos;
    FP mInitialYPos;
    FP mCurrentXPos;
    FP mCurrentYPos;
    FP mSpriteScale;
    Line_G2 mLines[2][6];
    Prim_SetTPage mTPage[2];
    Layer mLayer;
    u8 mR = 127;
    u8 mG = 127;
    u8 mB = 127;

    enum class State
    {
        eGoingUp,
        eGoingDown
    };
    State mState = State::eGoingUp;
};
