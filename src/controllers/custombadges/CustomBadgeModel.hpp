// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/SignalVectorModel.hpp"

#include <QObject>

namespace chatterino {

class CustomBadge;

class CustomBadgeModel : public SignalVectorModel<CustomBadge>
{
public:
    explicit CustomBadgeModel(QObject *parent);

    enum Column {
        Mode = 0,
        Name = 1,
        TypeVersion = 2,
        Restriction = 3,
        Image = 4,
        Channel = 5
    };

protected:
    CustomBadge getItemFromRow(std::vector<QStandardItem *> &row,
                               const CustomBadge &original) override;

    void getRowFromItem(const CustomBadge &item,
                        std::vector<QStandardItem *> &row) override;
};

}  // namespace chatterino
