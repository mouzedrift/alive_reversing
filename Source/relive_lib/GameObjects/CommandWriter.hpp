#pragma once

#include "BaseGameObject.hpp"
#include <string>

class CommandWriter final : public BaseGameObject
{
public:
    CommandWriter();
    ~CommandWriter();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    void ResetCurrentCommand();
private:
    bool WriteToFile() const;
    bool WriteCommands(bool isPressed, u32 commands);
    std::string MakeWaitTimer(u32 timer);

    std::string mCurrentCommand;
    u32 mWaitTimer = 0;
};

extern CommandWriter* gCommandWriter;
