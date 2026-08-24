// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/custombadges/CustomBadgeModel.hpp"

#include "controllers/custombadges/CustomBadge.hpp"
#include "util/StandardItemHelper.hpp"

namespace chatterino {

CustomBadgeModel::CustomBadgeModel(QObject *parent)
    : SignalVectorModel<CustomBadge>(6, parent)
{
}

CustomBadge CustomBadgeModel::getItemFromRow(
    std::vector<QStandardItem *> &row, const CustomBadge &original)
{
    // Our editable model view mostly relies on a custom dialog for editing,
    // so we just return the original if it hasn't changed, or reconstruct
    // what we can. In this case, we prefer original data.
    return CustomBadge{
        original.badgeType(),
        original.badgeVersion(),
        original.channelName(),
        original.restriction(),
        original.imageFilePath(),
        original.isAddon(),
        original.name()
    };
}

void CustomBadgeModel::getRowFromItem(const CustomBadge &item,
                                      std::vector<QStandardItem *> &row)
{
    using Column = CustomBadgeModel::Column;

    setStringItem(row[Column::Mode], item.isAddon() ? "Addon" : "Replace", false, false);
    setStringItem(row[Column::Name], item.name(), false, false);
    setStringItem(row[Column::TypeVersion], item.typeAndVersion(), false, false);
    setStringItem(row[Column::Restriction], item.restriction(), false, false);
    setStringItem(row[Column::Image], item.imageFilePath(), false, false);
    setStringItem(row[Column::Channel], item.channelName(), false, false);
}

}  // namespace chatterino
