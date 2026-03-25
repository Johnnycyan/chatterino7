// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/commands/Command.hpp"
#include "controllers/custombadges/CustomBadge.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"

#include <QString>
#include <QUrl>

#include <vector>

namespace chatterino::cyan {

/**
 * All data that can appear in a Cyan import/export YAML file.
 */
struct ImportExportData {
    std::vector<Command> commands;
    std::vector<CustomBadge> customBadges;
    std::vector<ModerationAction> moderationButtons;
};

/**
 * Serialize @a data to a YAML string in the Cyan export format.
 *
 * - Commands are written as `command:` blocks.
 * - Custom badges are written as `custom_badge:` blocks.
 * - Moderation buttons are written as `moderation_button:` blocks.
 *
 * Image paths are stored as basenames only (no directory component).
 */
[[nodiscard]] QString exportToYaml(const ImportExportData &data);

/**
 * Parse a YAML string produced by @ref exportToYaml back into
 * @ref ImportExportData.
 *
 * @param yaml   The YAML text to parse.
 * @param error  If non-null and parsing fails, receives a human-readable
 *               error message and the function returns an empty struct.
 */
[[nodiscard]] ImportExportData importFromYaml(const QString &yaml,
                                              QString *error = nullptr);

}  // namespace chatterino::cyan
