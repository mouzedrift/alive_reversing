#pragma once

#include "BaseAliveGameObject.hpp"

class IAbe : public BaseAliveGameObject
{
public:
    IAbe();


    // AO only
    bool mParamoniaDone = false;
    bool mScrabaniaDone = false;
    bool mElumMountBegin = false;
    bool mElumMountEnd = false;
    bool mElumUnmountBegin = false;

    // AE only
    bool mHaveHealing = false;
    bool mMudancheeDone = false;
    bool mMudomoDone = false;
    bool mDoQuicksave = false;
    bool mHasEvilFart = false;
    s16 mSaveFileId = 0;
    Guid mWorkWheelId;
    s32 mInvisibilityTimer = 0;
    Guid mInvisibleEffectId;
    s16 mHaveInvisibility = 0;
    s16 mInvisibilityDuration = 0;

    // shared

    bool mShrivel = false;
    bool mReturnToPreviousMotion = false;
    bool mLandSoftly = true;
    bool mLaughAtChantEnd = false;
    bool mPreventChanting = true;
    bool mbMotionChanged = false;
    EReliveLevelIds mDstWellLevel = EReliveLevelIds::eNone;
    s16 mDstWellPath = 0;
    s16 mDstWellCamera = 0;
    Guid mSlapableOrPickupId;
    Guid mPullRingRopeId;
    Guid mFadeId;
    Guid mCircularFadeId;
    Guid mOrbWhirlWindId;
    Guid mThrowableId;
    s32 mRegenHealthTimer = 0;
    s32 mAutoSayTimer = 0;
    s16 mHaveShrykull = 0;
    s32 mRingPulseTimer = 0;
    s16 mObjectIdInCam = 0; // ID to identify the correct object in a cam for example when we have more than 1 door in the same cam
    s8 mBaseThrowableCount = 0;
    s8 mThrowDirection = 0;
    FP mFallMotionVelX = {};
    s32 mPrevInput = 0;
    s32 mReleasedButtons = 0;
    ReliveTypes mHandStoneType = {};
    s16 mHandStoneCamIdx = 0;

    enum class PortalSubStates : s16
    {
        eJumpingInsidePortal = 0,
        eSetNewActiveCamera = 1,
        eHopOutOfPortal = 2,
        ePadding_3 = 3,
        eSetNewAbePosition = 4
    };
    PortalSubStates mBirdPortalSubState = PortalSubStates::eJumpingInsidePortal;
};
