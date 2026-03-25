// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/RapidjsonHelpers.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <QString>

namespace chatterino {

/**
 * A custom badge entry defined by the user.
 *
 * When isAddon is false: replaces the image of a matching Twitch badge.
 * When isAddon is true:  appears in addition to all badges for users
 *                        matching the channel/restriction filter.
 */
class CustomBadge
{
public:
    CustomBadge(const QString &badgeType, const QString &badgeVersion,
                const QString &channelName, const QString &restriction,
                const QString &imageFilePath, bool isAddon,
                const QString &name = QString());

    bool operator==(const CustomBadge &other) const;
    bool operator!=(const CustomBadge &other) const;

    /**
     * @brief The Twitch badge set id to match (e.g. "subscriber", "moderator").
     *        Empty string matches any badge type (only meaningful for addon mode).
     */
    [[nodiscard]] const QString &badgeType() const;

    /**
     * @brief The badge version to match (e.g. "1", "3000").
     *        Empty string matches any version.
     */
    [[nodiscard]] const QString &badgeVersion() const;

    /**
     * @brief Optional channel restriction. Empty = matches all channels.
     */
    [[nodiscard]] const QString &channelName() const;

    /**
     * @brief Optional restriction string (reserved for future use).
     */
    [[nodiscard]] const QString &restriction() const;

    /**
     * @brief Path to the local image file.
     *        If it does not start with a path separator or drive letter,
     *        it is treated as a filename relative to the chatterino image folder.
     */
    [[nodiscard]] const QString &imageFilePath() const;

    /**
     * @brief If true, this badge appears *in addition to* existing badges.
     *        If false, it *replaces* the image of a matching Twitch badge.
     */
    [[nodiscard]] bool isAddon() const;

    /**
     * @brief Returns a combined "badgeType/badgeVersion" string for display.
     */
    [[nodiscard]] QString typeAndVersion() const;

    /**
     * @brief Optional custom name for this badge. Shows in tooltip on hover.
     *        If empty, defaults to "Custom Badge".
     */
    [[nodiscard]] const QString &name() const;

private:
    QString badgeType_;
    QString badgeVersion_;
    QString channelName_;
    QString restriction_;
    QString imageFilePath_;
    bool isAddon_;
    QString name_;
};

}  // namespace chatterino

namespace pajlada {

template <>
struct Serialize<chatterino::CustomBadge> {
    static rapidjson::Value get(const chatterino::CustomBadge &value,
                                rapidjson::Document::AllocatorType &a)
    {
        rapidjson::Value ret(rapidjson::kObjectType);

        chatterino::rj::set(ret, "badgeType", value.badgeType(), a);
        chatterino::rj::set(ret, "badgeVersion", value.badgeVersion(), a);
        chatterino::rj::set(ret, "channelName", value.channelName(), a);
        chatterino::rj::set(ret, "restriction", value.restriction(), a);
        chatterino::rj::set(ret, "imageFilePath", value.imageFilePath(), a);
        chatterino::rj::set(ret, "isAddon", value.isAddon(), a);
        chatterino::rj::set(ret, "name", value.name(), a);

        return ret;
    }
};

template <>
struct Deserialize<chatterino::CustomBadge> {
    static chatterino::CustomBadge get(const rapidjson::Value &value,
                                       bool *error = nullptr)
    {
        if (!value.IsObject())
        {
            PAJLADA_REPORT_ERROR(error)
            return chatterino::CustomBadge(QString(), QString(), QString(),
                                           QString(), QString(), false,
                                           QString());
        }

        QString _badgeType;
        QString _badgeVersion;
        QString _channelName;
        QString _restriction;
        QString _imageFilePath;
        bool _isAddon = false;
        QString _name;

        chatterino::rj::getSafe(value, "badgeType", _badgeType);
        chatterino::rj::getSafe(value, "badgeVersion", _badgeVersion);
        chatterino::rj::getSafe(value, "channelName", _channelName);
        chatterino::rj::getSafe(value, "restriction", _restriction);
        chatterino::rj::getSafe(value, "imageFilePath", _imageFilePath);
        chatterino::rj::getSafe(value, "isAddon", _isAddon);
        chatterino::rj::getSafe(value, "name", _name);

        return chatterino::CustomBadge(_badgeType, _badgeVersion, _channelName,
                                       _restriction, _imageFilePath, _isAddon,
                                       _name);
    }
};

}  // namespace pajlada
