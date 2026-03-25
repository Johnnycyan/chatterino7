// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/SignalVector.hpp"
#include "controllers/custombadges/CustomBadge.hpp"
#include "CustomBadge.hpp"
#include "messages/Emote.hpp"
#include "providers/twitch/TwitchBadge.hpp"

#include <QString>

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace chatterino {

class CustomBadgesController final
{
public:
    CustomBadgesController();
    ~CustomBadgesController();

    CustomBadgesController(const CustomBadgesController &) = delete;
    CustomBadgesController &operator=(const CustomBadgesController &) = delete;
    CustomBadgesController(CustomBadgesController &&) = delete;
    CustomBadgesController &operator=(CustomBadgesController &&) = delete;

    void initialize();

    SignalVector<CustomBadge> customBadges;

    bool isEnabled() const;

    std::optional<EmotePtr> getCustomBadgeEmote(const TwitchBadge &badge,
                                                const QString &channelName);

    std::vector<EmotePtr> getAddonBadges(const std::vector<TwitchBadge> &badges,
                                         const QString &channelName);

private:
    std::map<QString, EmotePtr> emoteCache_;

    EmotePtr getOrCreateEmote(const QString &imageFilePath,
                              const QString &badgeName = QString());
};

}  // namespace chatterino
