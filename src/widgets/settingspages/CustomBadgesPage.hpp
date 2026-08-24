// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

class EditableModelView;
class CustomBadge;

class CustomBadgesPage : public SettingsPage
{
public:
    CustomBadgesPage();

    bool filterElements(const QString &query) override;

private:
    EditableModelView *view_{};
    
    void showEditDialog(const CustomBadge *badge, int row);
};

}  // namespace chatterino
