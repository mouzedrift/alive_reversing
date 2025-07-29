#include "CommandWriter.hpp"
#include <fstream>
#include <map>
#include "../../AliveLibAE/Input.hpp"

CommandWriter* gCommandWriter;

static constexpr u32 kTwitchChatCharLimit = 500;

struct CommandInfo
{
    const char_type* shortCommandName;
    const char_type* longCommandName;
    bool isHoldable;
};

static std::map<InputCommands, CommandInfo> sCommandsMap = {
    {InputCommands::eLeft, {"l", "left", true}},
    {InputCommands::eRight, {"r", "right", true}},
    {InputCommands::eUp, {"u", "up", true}},
    {InputCommands::eDown, {"d", "down", true}},
    {InputCommands::eDoAction, {"a", "action", false}},
    {InputCommands::eThrowItem, {"t", "throw", true}},
    {InputCommands::eRun, {"run", "run", true}},
    {InputCommands::eSneak, {"sneak", "sneak", true}},
    {InputCommands::eFartOrRoll, {"c", "crouch", true}},
    {InputCommands::eHop, {"j", "jump", true}},
    {InputCommands::eChant, {"0", "chant", true}},
    {InputCommands::eGameSpeak1, {"1", "hello", false}},
    {InputCommands::eGameSpeak2, {"2", "followme", false}},
    {InputCommands::eGameSpeak3, {"3", "wait", false}},
    {InputCommands::eGameSpeak4, {"4", "work", false}},
    {InputCommands::eGameSpeak5, {"5", "anger", false}},
    {InputCommands::eGameSpeak6, {"6", "allya", false}},
    {InputCommands::eGameSpeak7, {"7", "sympathy", false}},
    {InputCommands::eGameSpeak8, {"8", "stopit", false}},
};


CommandWriter::CommandWriter()
    : BaseGameObject(true, 0)
{
    gCommandWriter = this;
    SetSurviveDeathReset(true);
}

CommandWriter::~CommandWriter()
{
    gCommandWriter = nullptr;
}

bool CommandWriter::WriteToFile() const
{
    std::ofstream commandFile("command.txt");
    if (commandFile.is_open())
    {
        commandFile << mCurrentCommand;
        commandFile.close();
        return true;
    }

    return false;
}


bool CommandWriter::WriteCommands(bool isPressed, u32 commands)
{
    bool wroteCommand = false;
    std::string newCommands;

    // Iterate over the first 19 InputCommands bits
    for (u32 i = 0; i < 19; i++)
    {
        InputCommands command = static_cast<InputCommands>(1 << i);

        if ((commands & static_cast<u32>(command)) != 0)
        {
            auto it = sCommandsMap.find(command);
            if (it != sCommandsMap.end())
            {
                auto commandInfo = it->second;
                if (!commandInfo.isHoldable && !isPressed)
                {
                    // don't write a command when we're releasing a non holdable key
                    break;
                }

                wroteCommand = true;
                newCommands += commandInfo.shortCommandName;
                if (commandInfo.isHoldable)
                {
                    newCommands += (isPressed ? '+' : '-');
                }
                newCommands += " ";
            }
        }
    }

    if (wroteCommand)
    {
        if (mWaitTimer > 0)
        {
            mCurrentCommand += MakeWaitTimer(mWaitTimer);
        }

        mCurrentCommand += newCommands;
        mWaitTimer = 0;
    }

    return wroteCommand;
}

void CommandWriter::ResetCurrentCommand()
{
    mCurrentCommand = "";
    mWaitTimer = 0;
}

std::string CommandWriter::MakeWaitTimer(u32 timer)
{
    const u32 maxWaitTimer = 120;
    std::string result;

    while (timer > 0)
    {
        u32 chunk = (timer > maxWaitTimer) ? maxWaitTimer : timer;
        if (chunk == 15)
        {
            result += "w ";
        }
        else if (chunk == 30)
        {
            result += "lw ";
        }
        else
        {
            result += "w" + std::to_string(chunk) + " ";
        }
        timer -= chunk;
    }

    return result;
}

void CommandWriter::VUpdate()
{
    auto pressed = Input().GetPressed();
    auto released = Input().GetReleased();

    bool wroteCommand = false;
    if (pressed != 0)
    {
        wroteCommand |= WriteCommands(true, pressed);
    }

    if (released != 0)
    {
        wroteCommand |= WriteCommands(false, released);
    }

    if (wroteCommand)
    {
        LOG_INFO("command is: %s", mCurrentCommand.c_str());
    }

    mWaitTimer++;
    if (mCurrentCommand.size() > kTwitchChatCharLimit)
    {
        LOG_WARNING("command is longer than twitch chat's char limit!");
    }
}

void CommandWriter::VScreenChanged()
{
    // keep alive
}
