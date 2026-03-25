// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/custombadges/CustomBadgesController.hpp"

#include "Application.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace chatterino {

CustomBadgesController::CustomBadgesController()
{
}

void CustomBadgesController::initialize()
{
    // Try to create the custom badges directory
    QString badgesDir = getApp()->getPaths().miscDirectory + "/CustomBadges";
    (void)QDir().mkpath(badgesDir);

    // Connect to the Settings' customBadges to monitor changes
    auto *settings = getSettings();
    settings->customBadges.delayedItemsChanged.connect([this] {
        this->emoteCache_.clear();
    });
}

CustomBadgesController::~CustomBadgesController()
{
}

bool CustomBadgesController::isEnabled() const
{
    return getSettings()->customBadgesEnabled;
}

EmotePtr CustomBadgesController::getOrCreateEmote(const QString &imageFilePath,
                                                  const QString &badgeName)
{
    // Create a cache key that includes the badge name if provided
    QString cacheKey = imageFilePath;
    if (!badgeName.isEmpty())
    {
        cacheKey = imageFilePath + "|" + badgeName;
    }

    if (this->emoteCache_.count(cacheKey))
    {
        return this->emoteCache_[cacheKey];
    }

    QString actualPath = imageFilePath;

    // If it's a relative path, resolve it against the misc/CustomBadges directory
    QFileInfo fileInfo(imageFilePath);
    if (fileInfo.isRelative())
    {
        actualPath = QDir(getApp()->getPaths().miscDirectory + "/CustomBadges")
                         .absoluteFilePath(imageFilePath);
    }

    QString urlStr = QUrl::fromLocalFile(actualPath).toString();

    // We make an emote that essentially renders an Image from local file
    // badge emotes expect base size to be roughly 18x18
    // Provide properly scaled versions for 1x, 2x, and 4x DPI
    auto image1x = Image::fromUrl(Url{urlStr}, 1, QSize(18, 18));
    auto image2x = Image::fromUrl(Url{urlStr}, 0.5, QSize(36, 36));
    auto image4x = Image::fromUrl(Url{urlStr}, 0.25, QSize(72, 72));

    // Use the provided badge name, or default to "Custom Badge"
    QString displayName =
        badgeName.isEmpty() ? QString("Custom Badge") : badgeName;

    Emote emote{
        .name = EmoteName{},
        .images = ImageSet{image1x, image2x, image4x},
        .tooltip = Tooltip{displayName},
        .homePage = Url{},
    };

    auto ptr = std::make_shared<Emote>(std::move(emote));
    this->emoteCache_[cacheKey] = ptr;
    return ptr;
}

std::optional<EmotePtr> CustomBadgesController::getCustomBadgeEmote(
    const TwitchBadge &badge, const QString &channelName)
{
    if (!this->isEnabled())
        return std::nullopt;

    auto snapshot = getSettings()->customBadges.readOnly();
    for (const auto &cb : *snapshot)
    {
        if (cb.isAddon())
            continue;  // We only want replacement badges here

        // Check mode: type matching
        if (!cb.badgeType().isEmpty() && cb.badgeType() != badge.key_)
            continue;

        // Check version matching
        if (!cb.badgeVersion().isEmpty() && cb.badgeVersion() != badge.value_)
            continue;

        // Check channel restriction
        if (!cb.channelName().isEmpty() &&
            cb.channelName().compare(channelName, Qt::CaseInsensitive) != 0)
            continue;

        // Todo: Add user restriction matching when implemented

        return this->getOrCreateEmote(cb.imageFilePath(), cb.name());
    }

    return std::nullopt;
}

std::vector<EmotePtr> CustomBadgesController::getAddonBadges(
    const std::vector<TwitchBadge> &badges, const QString &channelName)
{
    std::vector<EmotePtr> result;
    if (!this->isEnabled())
        return result;

    auto snapshot = getSettings()->customBadges.readOnly();
    for (const auto &cb : *snapshot)
    {
        if (!cb.isAddon())
            continue;  // We only want addon badges here

        // Check channel restriction
        if (!cb.channelName().isEmpty() &&
            cb.channelName().compare(channelName, Qt::CaseInsensitive) != 0)
            continue;

        // Check if there's any matching badge in the user's set
        bool matched = false;
        if (cb.badgeType().isEmpty() && cb.badgeVersion().isEmpty())
        {
            matched =
                true;  // matches any user (maybe apply to everyone in channel)
        }
        else
        {
            for (const auto &userBadge : badges)
            {
                bool typeMatches = cb.badgeType().isEmpty() ||
                                   cb.badgeType() == userBadge.key_;
                bool versionMatches = cb.badgeVersion().isEmpty() ||
                                      cb.badgeVersion() == userBadge.value_;
                if (typeMatches && versionMatches)
                {
                    matched = true;
                    break;
                }
            }
        }

        if (matched)
        {
            result.push_back(
                this->getOrCreateEmote(cb.imageFilePath(), cb.name()));
        }
    }

    return result;
}

}  // namespace chatterino
