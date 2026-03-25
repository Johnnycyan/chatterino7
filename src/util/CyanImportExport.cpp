// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/CyanImportExport.hpp"

#include <QFileInfo>
#include <QMap>
#include <QStringList>
#include <QTextStream>

namespace chatterino::cyan {

namespace {

// ---------------------------------------------------------------------------
// YAML writer helpers
// ---------------------------------------------------------------------------

/**
 * Quote a YAML scalar value with double-quotes if it is empty, is a plain
 * YAML boolean/null keyword, or contains characters that could be
 * misinterpreted by a YAML parser.
 */
QString yamlQuote(const QString &value)
{
    // Always quote empty string to distinguish from null
    if (value.isEmpty())
    {
        return QStringLiteral("\"\"");
    }

    // Check whether quoting is needed
    bool needsQuoting = false;
    for (const QChar ch : value)
    {
        if (ch == ':' || ch == '#' || ch == '"' || ch == '\'' ||
            ch == '\\' || ch == '\n' || ch == '\r' || ch == '\t' ||
            ch == '{' || ch == '}' || ch == '[' || ch == ']' ||
            ch == '&' || ch == '*' || ch == '!' || ch == '|' ||
            ch == '>' || ch == '%' || ch == '@' || ch == '`')
        {
            needsQuoting = true;
            break;
        }
    }
    if (value == QStringLiteral("true") || value == QStringLiteral("false") ||
        value == QStringLiteral("null"))
    {
        needsQuoting = true;
    }

    if (!needsQuoting)
    {
        return value;
    }

    // Double-quote with internal quote/backslash escaping
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    escaped.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}

QString yamlBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

/** Extract the basename (filename only) from a local path or URL string. */
QString baseName(const QString &pathOrUrl)
{
    if (pathOrUrl.isEmpty())
    {
        return {};
    }
    // Works for both local paths and file:// URLs because QFileInfo can
    // handle both.
    return QFileInfo(pathOrUrl).fileName();
}

// ---------------------------------------------------------------------------
// Simple YAML reader helpers
// ---------------------------------------------------------------------------

/**
 * Unquote a YAML scalar previously produced by `yamlQuote`.
 * Handles double-quoted strings and plain (unquoted) strings.
 */
QString yamlUnquote(const QString &raw)
{
    QString trimmed = raw.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"') &&
        trimmed.size() >= 2)
    {
        // Remove surrounding quotes and unescape
        QString inner = trimmed.mid(1, trimmed.size() - 2);
        inner.replace(QStringLiteral("\\\\"), QStringLiteral("\x01"));  // temp
        inner.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        inner.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        inner.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
        inner.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
        inner.replace(QStringLiteral("\x01"), QStringLiteral("\\"));
        return inner;
    }
    // Single-quoted string (passthrough – we never write these but handle
    // them for partial compatibility)
    if (trimmed.startsWith('\'') && trimmed.endsWith('\'') &&
        trimmed.size() >= 2)
    {
        return trimmed.mid(1, trimmed.size() - 2);
    }
    return trimmed;
}

bool yamlToBool(const QString &raw)
{
    return raw.trimmed().compare(QStringLiteral("true"),
                                 Qt::CaseInsensitive) == 0;
}

// ---------------------------------------------------------------------------
// Parser
//
// Produces a two-level structure:
//   sections["commands"]         → list of item-field maps
//   sections["custom_badges"]    → list of item-field maps
//   sections["moderation_buttons"] → list of item-field maps
//
// The YAML format produced by exportToYaml is:
//
//   commands:
//     - trigger: /foo
//       command: bar
//       menu: false
//     - trigger: /baz
//       ...
//   custom_badges:
//     - mode: Addon
//       ...
//   moderation_buttons:
//     - action: /foo
//       icon: delete.png
//
// Rules:
//   - Top-level lines (no leading space): section header, e.g. `commands:`
//   - Lines starting with `  - ` (2 spaces + dash + space): new list item;
//     the rest of the line is parsed as `key: value` for the first field.
//   - Lines starting with `    ` (4 spaces, no dash): continuation field of
//     the current list item.
//   - Blank lines and `#` comment lines are ignored.
// ---------------------------------------------------------------------------

using FieldMap = QMap<QString, QString>;
using SectionItems = QList<FieldMap>;

QMap<QString, SectionItems> parseSections(const QString &yaml, QString *error)
{
    QMap<QString, SectionItems> sections;
    QString currentSection;
    FieldMap currentItem;
    bool hasItem = false;

    auto flushItem = [&] {
        if (hasItem && !currentSection.isEmpty())
        {
            sections[currentSection].append(currentItem);
            currentItem.clear();
            hasItem = false;
        }
    };

    const QStringList lines = yaml.split('\n');
    for (const QString &rawLine : lines)
    {
        const QString line = QString(rawLine).remove('\r');
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
        {
            continue;
        }

        // Top-level section header: no leading whitespace, ends with ':'
        if (!line[0].isSpace())
        {
            flushItem();
            currentSection =
                trimmed.endsWith(':') ? trimmed.chopped(1) : trimmed;
            continue;
        }

        // List item line: starts with optional spaces then "- "
        // We accept 1–4 leading spaces before the dash.
        const int dashPos = line.indexOf(QLatin1String("- "));
        if (dashPos >= 0 && dashPos <= 3 && line.left(dashPos).trimmed().isEmpty())
        {
            flushItem();
            hasItem = true;

            // Parse the inline field that follows the dash
            const QString afterDash = line.mid(dashPos + 2).trimmed();
            const int colonPos = afterDash.indexOf(':');
            if (colonPos > 0)
            {
                const QString key = afterDash.left(colonPos).trimmed();
                const QString val =
                    yamlUnquote(afterDash.mid(colonPos + 1).trimmed());
                if (!key.isEmpty())
                {
                    currentItem[key] = val;
                }
            }
            continue;
        }

        // Continuation field of the current list item
        if (!hasItem)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "YAML parse error: field line before any list item");
            }
            return {};
        }

        const int colonPos = trimmed.indexOf(':');
        if (colonPos <= 0)
        {
            continue;  // skip malformed lines
        }

        const QString key = trimmed.left(colonPos).trimmed();
        const QString val =
            yamlUnquote(trimmed.mid(colonPos + 1).trimmed());
        currentItem[key] = val;
    }

    flushItem();
    return sections;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/*
 * Exported format:
 *
 *   commands:
 *     - trigger: /foo
 *       command: bar
 *       menu: false
 *       channel: ""
 *
 *   custom_badges:
 *     - mode: Addon
 *       name: Youtube Chatter
 *       type-version: youtube/1
 *       restriction: ""
 *       image: youtube-512.png
 *       channel: ""
 *
 *   moderation_buttons:
 *     - action: /mg-delete
 *       icon: delete.png
 */
QString exportToYaml(const ImportExportData &data)
{
    QString out;
    QTextStream ts(&out);

    // ---- commands ----
    if (!data.commands.empty())
    {
        ts << "commands:\n";
        for (const Command &cmd : data.commands)
        {
            ts << "  - trigger: " << yamlQuote(cmd.name) << "\n";
            ts << "    command: " << yamlQuote(cmd.func) << "\n";
            ts << "    menu: " << yamlBool(cmd.showInMsgContextMenu) << "\n";
            ts << "    channel: " << yamlQuote(cmd.restrictedChannel) << "\n";
        }
        ts << "\n";
    }

    // ---- custom_badges ----
    if (!data.customBadges.empty())
    {
        ts << "custom_badges:\n";
        for (const CustomBadge &badge : data.customBadges)
        {
            ts << "  - mode: "
               << (badge.isAddon() ? QStringLiteral("Addon")
                                   : QStringLiteral("Replace"))
               << "\n";
            ts << "    name: " << yamlQuote(badge.name()) << "\n";
            QString typeVersion = badge.badgeType();
            if (!badge.badgeVersion().isEmpty())
            {
                typeVersion += QLatin1Char('/') + badge.badgeVersion();
            }
            ts << "    type-version: " << yamlQuote(typeVersion) << "\n";
            ts << "    restriction: " << yamlQuote(badge.restriction()) << "\n";
            ts << "    image: " << yamlQuote(baseName(badge.imageFilePath()))
               << "\n";
            ts << "    channel: " << yamlQuote(badge.channelName()) << "\n";
        }
        ts << "\n";
    }

    // ---- moderation_buttons ----
    if (!data.moderationButtons.empty())
    {
        ts << "moderation_buttons:\n";
        for (const ModerationAction &btn : data.moderationButtons)
        {
            ts << "  - action: " << yamlQuote(btn.getAction()) << "\n";
            ts << "    icon: "
               << yamlQuote(baseName(btn.iconPath().toLocalFile())) << "\n";
        }
        ts << "\n";
    }

    return out;
}

ImportExportData importFromYaml(const QString &yaml, QString *error)
{
    const auto sections = parseSections(yaml, error);
    if (sections.isEmpty() && error && !error->isEmpty())
    {
        return {};
    }

    ImportExportData result;

    // ---- commands ----
    for (const FieldMap &fields : sections.value(QStringLiteral("commands")))
    {
        Command cmd;
        cmd.name = fields.value(QStringLiteral("trigger"));
        cmd.func = fields.value(QStringLiteral("command"));
        cmd.showInMsgContextMenu =
            yamlToBool(fields.value(QStringLiteral("menu")));
        cmd.restrictedChannel = fields.value(QStringLiteral("channel"));
        result.commands.push_back(std::move(cmd));
    }

    // ---- custom_badges ----
    for (const FieldMap &fields :
         sections.value(QStringLiteral("custom_badges")))
    {
        const bool isAddon =
            fields.value(QStringLiteral("mode"))
                .compare(QStringLiteral("Addon"), Qt::CaseInsensitive) == 0;
        const QString typeVersion =
            fields.value(QStringLiteral("type-version"));

        QString badgeType;
        QString badgeVersion;
        const int slashPos = typeVersion.indexOf('/');
        if (slashPos >= 0)
        {
            badgeType = typeVersion.left(slashPos);
            badgeVersion = typeVersion.mid(slashPos + 1);
        }
        else
        {
            badgeType = typeVersion;
        }

        result.customBadges.emplace_back(
            badgeType, badgeVersion,
            fields.value(QStringLiteral("channel")),
            fields.value(QStringLiteral("restriction")),
            fields.value(QStringLiteral("image")), isAddon,
            fields.value(QStringLiteral("name")));
    }

    // ---- moderation_buttons ----
    for (const FieldMap &fields :
         sections.value(QStringLiteral("moderation_buttons")))
    {
        const QString action = fields.value(QStringLiteral("action"));
        const QString icon = fields.value(QStringLiteral("icon"));
        result.moderationButtons.emplace_back(action, QUrl(icon));
    }

    return result;
}

}  // namespace chatterino::cyan
