// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/**
 * Settings page for Cyan-specific import/export functionality.
 *
 * Allows exporting and importing Commands, Custom Badges, and Moderation
 * Buttons as a single combined YAML file.
 */
class CyanPage : public SettingsPage
{
    Q_OBJECT

public:
    CyanPage();

    bool filterElements(const QString &query) override;

private:
    void doExport(bool includeCommands, bool includeBadges, bool includeButtons);
    void doImport(bool replace);
};

}  // namespace chatterino
