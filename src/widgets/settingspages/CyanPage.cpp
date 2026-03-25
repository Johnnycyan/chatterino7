// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/CyanPage.hpp"

#include "Application.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/custombadges/CustomBadge.hpp"
#include "controllers/custombadges/CustomBadgesController.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"
#include "singletons/Settings.hpp"
#include "util/CyanImportExport.hpp"
#include "util/LayoutCreator.hpp"

#include <QCheckBox>
#include <QDialog>
#include <algorithm>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace chatterino {

// ---------------------------------------------------------------------------
// Image Resolver Dialog
//
// Shown when an import file references image files that need to be located.
// Groups entries by "Custom Badges" and "Moderation Buttons".
// ---------------------------------------------------------------------------

namespace {

struct ImageEntry {
    QString originalBasename;   ///< filename as stored in the YAML
    QString category;           ///< "Custom Badges" or "Moderation Buttons"
    QString label;              ///< "Image" or "Icon"
    QLineEdit *pathEdit{};      ///< filled in by the dialog
};

/**
 * Show the image resolver dialog.
 *
 * @param parent        Parent widget.
 * @param badgeImages   Basenames of images referenced by custom badges.
 * @param buttonIcons   Basenames of icons referenced by moderation buttons.
 * @param outBadgeMap   Receives original basename → resolved path for badges.
 * @param outButtonMap  Receives original basename → resolved path for buttons.
 * @return true if the user accepted, false if cancelled.
 */
bool runImageResolverDialog(
    QWidget *parent, const QStringList &badgeImages,
    const QStringList &buttonIcons,
    QMap<QString, QString> &outBadgeMap,
    QMap<QString, QString> &outButtonMap)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Resolve Image Files");
    dialog.setMinimumWidth(500);

    auto *outerLayout = new QVBoxLayout(&dialog);

    auto *introLabel = new QLabel(
        "Please specify what image files to use for these imports:", &dialog);
    introLabel->setWordWrap(true);
    outerLayout->addWidget(introLabel);

    QList<ImageEntry> entries;

    // --- Custom Badges section ---
    if (!badgeImages.isEmpty())
    {
        auto *badgesGroup = new QGroupBox("Custom Badges", &dialog);
        auto *badgesLayout = new QFormLayout(badgesGroup);

        for (const QString &basename : badgeImages)
        {
            auto *rowWidget = new QWidget(badgesGroup);
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto *edit = new QLineEdit(rowWidget);
            edit->setPlaceholderText(basename);
            edit->setMinimumWidth(200);
            auto *browseBtn = new QPushButton("Browse...", rowWidget);

            rowLayout->addWidget(edit);
            rowLayout->addWidget(browseBtn);

            QObject::connect(browseBtn, &QPushButton::clicked,
                             [&dialog, edit, basename] {
                                 const QString path = QFileDialog::getOpenFileName(
                                     &dialog, "Select image for " + basename,
                                     QString(),
                                     "Images (*.png *.jpg *.jpeg *.gif *.webp "
                                     "*.svg *.bmp);;All files (*)");
                                 if (!path.isEmpty())
                                 {
                                     edit->setText(path);
                                 }
                             });

            badgesLayout->addRow("Image: " + basename, rowWidget);

            entries.push_back(ImageEntry{basename, "badge", "Image", edit});
        }

        outerLayout->addWidget(badgesGroup);
    }

    // --- Moderation Buttons section ---
    if (!buttonIcons.isEmpty())
    {
        auto *btnsGroup = new QGroupBox("Moderation Buttons", &dialog);
        auto *btnsLayout = new QFormLayout(btnsGroup);

        for (const QString &basename : buttonIcons)
        {
            auto *rowWidget = new QWidget(btnsGroup);
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto *edit = new QLineEdit(rowWidget);
            edit->setPlaceholderText(basename);
            edit->setMinimumWidth(200);
            auto *browseBtn = new QPushButton("Browse...", rowWidget);

            rowLayout->addWidget(edit);
            rowLayout->addWidget(browseBtn);

            QObject::connect(browseBtn, &QPushButton::clicked,
                             [&dialog, edit, basename] {
                                 const QString path = QFileDialog::getOpenFileName(
                                     &dialog, "Select icon for " + basename,
                                     QString(),
                                     "Images (*.png *.jpg *.jpeg *.gif *.webp "
                                     "*.svg *.bmp *.ico);;All files (*)");
                                 if (!path.isEmpty())
                                 {
                                     edit->setText(path);
                                 }
                             });

            btnsLayout->addRow("Icon: " + basename, rowWidget);

            entries.push_back(ImageEntry{basename, "button", "Icon", edit});
        }

        outerLayout->addWidget(btnsGroup);
    }

    auto *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog,
                     [&dialog, &entries] {
                         // Validate every entry has a non-empty, existing file path
                         for (const auto &entry : entries)
                         {
                             const QString path =
                                 entry.pathEdit->text().trimmed();
                             if (path.isEmpty())
                             {
                                 QMessageBox::warning(
                                     &dialog, "Missing Image",
                                     QString("Please browse and select a file "
                                             "for: %1")
                                         .arg(entry.originalBasename));
                                 entry.pathEdit->setFocus();
                                 return;
                             }
                             if (!QFileInfo::exists(path))
                             {
                                 QMessageBox::warning(
                                     &dialog, "File Not Found",
                                     QString("The selected file does not "
                                             "exist:\n%1")
                                         .arg(path));
                                 entry.pathEdit->setFocus();
                                 return;
                             }
                         }
                         dialog.accept();
                     });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog,
                     &QDialog::reject);
    outerLayout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    for (const auto &entry : entries)
    {
        const QString resolved = entry.pathEdit->text().trimmed();
        if (entry.category == "badge")
        {
            outBadgeMap[entry.originalBasename] = resolved;
        }
        else
        {
            outButtonMap[entry.originalBasename] = resolved;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Helper: collect unique non-empty image basenames
// ---------------------------------------------------------------------------

QStringList collectBadgeImages(const std::vector<CustomBadge> &badges)
{
    QStringList result;
    for (const auto &badge : badges)
    {
        const QString bn = QFileInfo(badge.imageFilePath()).fileName();
        if (!bn.isEmpty() && !result.contains(bn))
        {
            result.append(bn);
        }
    }
    return result;
}

QStringList collectButtonIcons(const std::vector<ModerationAction> &buttons)
{
    QStringList result;
    for (const auto &btn : buttons)
    {
        const QString path = btn.iconPath().toLocalFile().isEmpty()
                                 ? btn.iconPath().toString()
                                 : btn.iconPath().toLocalFile();
        const QString bn = QFileInfo(path).fileName();
        if (!bn.isEmpty() && !result.contains(bn))
        {
            result.append(bn);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Duplicate filtering
// ---------------------------------------------------------------------------

// Remove entries from `data` that already exist in the live settings.
// Returns the number of items removed (skipped).
int filterDuplicates(cyan::ImportExportData &data)
{
    int skipped = 0;

    {
        auto existing = getApp()->getCommands()->items.readOnly();
        auto it = std::remove_if(
            data.commands.begin(), data.commands.end(),
            [&existing](const Command &cmd) {
                return std::any_of(existing->begin(), existing->end(),
                                   [&cmd](const Command &e) {
                                       return e.name == cmd.name;
                                   });
            });
        skipped += static_cast<int>(
            std::distance(it, data.commands.end()));
        data.commands.erase(it, data.commands.end());
    }

    {
        auto existing = getSettings()->customBadges.readOnly();
        auto it = std::remove_if(
            data.customBadges.begin(), data.customBadges.end(),
            [&existing](const CustomBadge &badge) {
                return std::any_of(
                    existing->begin(), existing->end(),
                    [&badge](const CustomBadge &e) {
                        return e.badgeType() == badge.badgeType() &&
                               e.badgeVersion() == badge.badgeVersion() &&
                               e.channelName() == badge.channelName() &&
                               e.restriction() == badge.restriction();
                    });
            });
        skipped += static_cast<int>(
            std::distance(it, data.customBadges.end()));
        data.customBadges.erase(it, data.customBadges.end());
    }

    {
        auto existing = getSettings()->moderationActions.readOnly();
        auto it = std::remove_if(
            data.moderationButtons.begin(), data.moderationButtons.end(),
            [&existing](const ModerationAction &btn) {
                return std::any_of(
                    existing->begin(), existing->end(),
                    [&btn](const ModerationAction &e) {
                        return e.getAction() == btn.getAction();
                    });
            });
        skipped += static_cast<int>(
            std::distance(it, data.moderationButtons.end()));
        data.moderationButtons.erase(it, data.moderationButtons.end());
    }

    return skipped;
}

// ---------------------------------------------------------------------------
// Apply import data to live settings
// ---------------------------------------------------------------------------

// Returns the number of items actually appended (skips duplicates when !replace)
struct ApplyResult {
    int imported{};
    int skipped{};
};

// `data` must already have duplicates removed (via filterDuplicates) before
// calling this. Clears existing items first when replace == true.
ApplyResult applyImport(cyan::ImportExportData &data, bool replace)
{
    if (replace)
    {
        auto &cmds = getApp()->getCommands()->items;
        while (!cmds.raw().empty())
            cmds.removeAt(static_cast<int>(cmds.raw().size()) - 1);

        auto &badges = getSettings()->customBadges;
        while (!badges.raw().empty())
            badges.removeAt(static_cast<int>(badges.raw().size()) - 1);

        auto &actions = getSettings()->moderationActions;
        while (!actions.raw().empty())
            actions.removeAt(static_cast<int>(actions.raw().size()) - 1);
    }

    for (const auto &cmd : data.commands)
        getApp()->getCommands()->items.append(cmd);
    for (const auto &badge : data.customBadges)
        getSettings()->customBadges.append(badge);
    for (const auto &btn : data.moderationButtons)
        getSettings()->moderationActions.append(btn);

    ApplyResult result;
    result.imported = static_cast<int>(data.commands.size()) +
                      static_cast<int>(data.customBadges.size()) +
                      static_cast<int>(data.moderationButtons.size());
    return result;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// CyanPage
// ---------------------------------------------------------------------------

CyanPage::CyanPage()
{
    LayoutCreator<CyanPage> layoutCreator(this);
    auto layout = layoutCreator.setLayoutType<QVBoxLayout>();

    // ---- Title / description ----
    layout.emplace<QLabel>(
        "<b>Cyan Import / Export</b><br>"
        "Export or import your Commands, Custom Badges, and Moderation Buttons "
        "as a single YAML file.");

    layout->addSpacing(8);

    // ---- Export group ----
    {
        auto *exportGroup = new QGroupBox("Export", this);
        auto *exportLayout = new QVBoxLayout(exportGroup);

        auto *exportDesc = new QLabel(
            "Select which settings to include in the exported file:", exportGroup);
        exportDesc->setWordWrap(true);
        exportLayout->addWidget(exportDesc);

        auto *cbCommands = new QCheckBox("Commands", exportGroup);
        auto *cbBadges = new QCheckBox("Custom Badges", exportGroup);
        auto *cbButtons = new QCheckBox("Moderation Buttons", exportGroup);
        cbCommands->setChecked(true);
        cbBadges->setChecked(true);
        cbButtons->setChecked(true);
        exportLayout->addWidget(cbCommands);
        exportLayout->addWidget(cbBadges);
        exportLayout->addWidget(cbButtons);

        auto *exportBtn = new QPushButton("Export to YAML file...", exportGroup);
        exportLayout->addWidget(exportBtn);

        QObject::connect(exportBtn, &QPushButton::clicked, this,
                         [this, cbCommands, cbBadges, cbButtons] {
                             this->doExport(cbCommands->isChecked(),
                                            cbBadges->isChecked(),
                                            cbButtons->isChecked());
                         });

        layout->addWidget(exportGroup);
    }

    layout->addSpacing(8);

    // ---- Import group ----
    {
        auto *importGroup = new QGroupBox("Import", this);
        auto *importLayout = new QVBoxLayout(importGroup);

        auto *modeLabel =
            new QLabel("How to handle existing settings:", importGroup);
        importLayout->addWidget(modeLabel);

        auto *rbReplace =
            new QRadioButton("Replace — clear existing entries first", importGroup);
        auto *rbAppend =
            new QRadioButton("Append — add imported entries to existing ones",
                             importGroup);
        rbAppend->setChecked(true);
        importLayout->addWidget(rbReplace);
        importLayout->addWidget(rbAppend);

        layout->addSpacing(4);

        auto *importBtn = new QPushButton("Import from YAML file...", importGroup);
        importLayout->addWidget(importBtn);

        QObject::connect(importBtn, &QPushButton::clicked, this,
                         [this, rbReplace] {
                             this->doImport(rbReplace->isChecked());
                         });

        layout->addWidget(importGroup);
    }

    layout->addStretch(1);
}

bool CyanPage::filterElements(const QString &query)
{
    return query.isEmpty() ||
           QString("cyan import export commands custom badges moderation buttons yaml")
               .contains(query, Qt::CaseInsensitive);
}

void CyanPage::doExport(bool includeCommands, bool includeBadges,
                        bool includeButtons)
{
    if (!includeCommands && !includeBadges && !includeButtons)
    {
        QMessageBox::warning(this, "Nothing Selected",
                             "Please select at least one category to export.");
        return;
    }

    cyan::ImportExportData data;

    if (includeCommands)
    {
        data.commands = *getApp()->getCommands()->items.readOnly();
    }
    if (includeBadges)
    {
        data.customBadges = *getSettings()->customBadges.readOnly();
    }
    if (includeButtons)
    {
        data.moderationButtons = *getSettings()->moderationActions.readOnly();
    }

    const QString yamlText = cyan::exportToYaml(data);

    const QString path = QFileDialog::getSaveFileName(
        this, "Export Cyan Settings", QString(),
        "YAML files (*.yaml *.yml);;All files (*)");
    if (path.isEmpty())
    {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Export Failed",
                              "Could not open file for writing:\n" + path);
        return;
    }

    QTextStream ts(&file);
    ts << yamlText;
    file.close();

    QMessageBox::information(this, "Export Successful",
                             "Settings exported to:\n" + path);
}

void CyanPage::doImport(bool replace)
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Cyan Settings", QString(),
        "YAML files (*.yaml *.yml);;All files (*)");
    if (path.isEmpty())
    {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Import Failed",
                              "Could not open file for reading:\n" + path);
        return;
    }

    const QString yamlText = QTextStream(&file).readAll();
    file.close();

    QString parseError;
    cyan::ImportExportData data = cyan::importFromYaml(yamlText, &parseError);
    if (!parseError.isEmpty())
    {
        QMessageBox::critical(this, "Import Failed",
                              "Failed to parse YAML:\n" + parseError);
        return;
    }

    if (data.commands.empty() && data.customBadges.empty() &&
        data.moderationButtons.empty())
    {
        QMessageBox::warning(this, "Nothing to Import",
                             "The file contained no recognised entries.");
        return;
    }

    // In append mode, remove duplicates before asking about images so we
    // don't prompt the user to resolve images for items that won't be imported.
    int preSkipped = 0;
    if (!replace)
    {
        preSkipped = filterDuplicates(data);

        if (data.commands.empty() && data.customBadges.empty() &&
            data.moderationButtons.empty())
        {
            QMessageBox::information(
                this, "Nothing New to Import",
                QString("All %1 item(s) in the file already exist and were "
                        "skipped.")
                    .arg(preSkipped));
            return;
        }
    }

    // Collect image basenames that need to be resolved (only for items that
    // will actually be imported)
    const QStringList badgeImages = collectBadgeImages(data.customBadges);
    const QStringList buttonIcons = collectButtonIcons(data.moderationButtons);

    QMap<QString, QString> badgeMap;
    QMap<QString, QString> buttonMap;

    if (!badgeImages.isEmpty() || !buttonIcons.isEmpty())
    {
        const bool accepted = runImageResolverDialog(
            this, badgeImages, buttonIcons, badgeMap, buttonMap);
        if (!accepted)
        {
            return;
        }

        // Apply resolved paths into the data
        for (auto &badge : data.customBadges)
        {
            const QString bn =
                QFileInfo(badge.imageFilePath()).fileName();
            if (!bn.isEmpty() && badgeMap.contains(bn))
            {
                badge = CustomBadge(badge.badgeType(), badge.badgeVersion(),
                                    badge.channelName(), badge.restriction(),
                                    badgeMap.value(bn), badge.isAddon(),
                                    badge.name());
            }
        }

        for (auto &btn : data.moderationButtons)
        {
            const QString path2 = btn.iconPath().toLocalFile().isEmpty()
                                      ? btn.iconPath().toString()
                                      : btn.iconPath().toLocalFile();
            const QString bn = QFileInfo(path2).fileName();
            if (!bn.isEmpty() && buttonMap.contains(bn))
            {
                btn = ModerationAction(btn.getAction(),
                                       QUrl::fromLocalFile(buttonMap.value(bn)));
            }
        }
    }

    // Confirm replace
    if (replace)
    {
        const int totalExisting =
            static_cast<int>(getApp()->getCommands()->items.readOnly()->size()) +
            static_cast<int>(getSettings()->customBadges.readOnly()->size()) +
            static_cast<int>(getSettings()->moderationActions.readOnly()->size());

        if (totalExisting > 0)
        {
            int ret = QMessageBox::question(
                this, "Replace Settings",
                "This will replace your existing Commands, Custom Badges, and "
                "Moderation Buttons with the imported ones. Continue?",
                QMessageBox::Yes | QMessageBox::No);
            if (ret != QMessageBox::Yes)
            {
                return;
            }
        }
    }

    const auto applyResult = applyImport(data, replace);

    const int totalSkipped = preSkipped + applyResult.skipped;
    QString msg =
        QString("Imported %1 item(s).")
            .arg(applyResult.imported);
    if (totalSkipped > 0)
    {
        msg += QString("\n%1 duplicate item(s) were skipped.")
                   .arg(totalSkipped);
    }

    QMessageBox::information(this, "Import Successful", msg);
}

}  // namespace chatterino
