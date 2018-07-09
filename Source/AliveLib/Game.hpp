#pragma once

#include "FunctionFwd.hpp"
#include "BaseGameObject.hpp"
#include "Animation.hpp"

void Game_ForceLink();

namespace Test
{
    void GameTests();
}

EXPORT void CC Game_Main_4949F0();
EXPORT void CC sub_494580();

ALIVE_VAR_EXTERN(unsigned int, sGnFrame_5C1B84);
ALIVE_VAR_EXTERN(bool, sCommandLine_DDCheatEnabled_5CA4B5);
ALIVE_VAR_EXTERN(DynamicArrayT<BaseGameObject>*, gObjList_drawables_5C1124);
ALIVE_VAR_EXTERN(DynamicArrayT<Animation>*, gObjList_animations_5C1A24);