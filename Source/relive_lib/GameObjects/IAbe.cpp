#include "IAbe.hpp"
#include "../GameType.hpp"

IAbe::IAbe()
    : BaseAliveGameObject(GetGameType() == GameType::eAe ? 28 : 0)
{

}
