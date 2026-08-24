// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/custombadges/CustomBadge.hpp"

namespace chatterino {

CustomBadge::CustomBadge(const QString &badgeType, const QString &badgeVersion,
                         const QString &channelName, const QString &restriction,
                         const QString &imageFilePath, bool isAddon,
                         const QString &name)
    : badgeType_(badgeType)
    , badgeVersion_(badgeVersion)
    , channelName_(channelName)
    , restriction_(restriction)
    , imageFilePath_(imageFilePath)
    , isAddon_(isAddon)
    , name_(name)
{
}

bool CustomBadge::operator==(const CustomBadge &other) const
{
    return this->badgeType_ == other.badgeType_ &&
           this->badgeVersion_ == other.badgeVersion_ &&
           this->channelName_ == other.channelName_ &&
           this->restriction_ == other.restriction_ &&
           this->imageFilePath_ == other.imageFilePath_ &&
           this->isAddon_ == other.isAddon_ &&
           this->name_ == other.name_;
}

bool CustomBadge::operator!=(const CustomBadge &other) const
{
    return !(*this == other);
}

const QString &CustomBadge::badgeType() const
{
    return this->badgeType_;
}

const QString &CustomBadge::badgeVersion() const
{
    return this->badgeVersion_;
}

const QString &CustomBadge::channelName() const
{
    return this->channelName_;
}

const QString &CustomBadge::restriction() const
{
    return this->restriction_;
}

const QString &CustomBadge::imageFilePath() const
{
    return this->imageFilePath_;
}

bool CustomBadge::isAddon() const
{
    return this->isAddon_;
}

QString CustomBadge::typeAndVersion() const
{
    if (this->badgeType_.isEmpty())
    {
        return QString();
    }
    if (this->badgeVersion_.isEmpty())
    {
        return this->badgeType_;
    }
    return this->badgeType_ + "/" + this->badgeVersion_;
}

const QString &CustomBadge::name() const
{
    return this->name_;
}

}  // namespace chatterino
