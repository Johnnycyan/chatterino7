// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/Command.hpp"

namespace chatterino {

// command
Command::Command(const QString &_text)
{
    // Parse format: [/]name[#channel] [func]
    int spaceIndex = _text.indexOf(' ');
    QString commandPart = (spaceIndex == -1) ? _text : _text.mid(0, spaceIndex);
    
    // Check for channel restriction in command name
    int channelIndex = commandPart.indexOf('#');
    if (channelIndex != -1)
    {
        this->name = commandPart.mid(0, channelIndex).trimmed();
        this->restrictedChannel = commandPart.mid(channelIndex + 1).trimmed();
    }
    else
    {
        this->name = commandPart.trimmed();
        this->restrictedChannel = QString();
    }

    if (spaceIndex == -1)
    {
        return;
    }

    this->func = _text.mid(spaceIndex + 1).trimmed();
    this->showInMsgContextMenu = false;
}

Command::Command(const QString &_name, const QString &_func,
                 bool _showInMsgContextMenu,
                 const QString &_restrictedChannel)
    : name(_name.trimmed())
    , func(_func.trimmed())
    , showInMsgContextMenu(_showInMsgContextMenu)
    , restrictedChannel(_restrictedChannel.trimmed())
{
}

QString Command::toString() const
{
    QString result = this->name;
    // Include channel restriction in output for display/debugging
    if (!this->restrictedChannel.isEmpty())
    {
        result += "#" + this->restrictedChannel;
    }
    result += " " + this->func;
    return result;
}

}  // namespace chatterino
