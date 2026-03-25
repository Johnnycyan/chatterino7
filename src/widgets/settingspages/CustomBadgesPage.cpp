// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/CustomBadgesPage.hpp"

#include "Application.hpp"
#include "controllers/custombadges/CustomBadge.hpp"
#include "controllers/custombadges/CustomBadgeModel.hpp"
#include "controllers/custombadges/CustomBadgesController.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/helper/EditableModelView.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QUrl>

namespace chatterino {

CustomBadgesPage::CustomBadgesPage()
{
    LayoutCreator<CustomBadgesPage> layoutCreator(this);
    auto layout = layoutCreator.setLayoutType<QVBoxLayout>();

    auto *checkbox =
        layout.emplace<QCheckBox>("Enable Custom Badges").getElement();
    checkbox->setChecked(getSettings()->customBadgesEnabled);
    QObject::connect(checkbox, &QCheckBox::stateChanged, [](int state) {
        getSettings()->customBadgesEnabled = (state == Qt::Checked);
    });

    layout.emplace<QLabel>(
        "Manage your custom badges. 'Replacement' badges override existing "
        "Twitch badges "
        "(e.g. subscriber, moderator).\n'Addon' badges appear alongside "
        "existing badges "
        "when the channel and matching criteria are met.\nBadge images should "
        "be at least 18x18 pixels for best results.");

    auto *model = new CustomBadgeModel(this);
    model->initialized(&getSettings()->customBadges);

    EditableModelView *view =
        layout.emplace<EditableModelView>(model).getElement();
    this->view_ = view;

    view->setTitles(
        {"Mode", "Name", "Type/Version", "Restriction", "Image", "Channel"});

    // Custom Badge List is not inline-editable since it's complex
    // So we use doubleClick to pop up the dialog
    view->getTableView()->setEditTriggers(QAbstractItemView::NoEditTriggers);

    std::ignore = view->addButtonPressed.connect([this] {
        this->showEditDialog(nullptr, -1);
    });

    QObject::connect(
        view->getTableView(), &QTableView::doubleClicked,
        [this](const QModelIndex &index) {
            auto snapshot = getSettings()->customBadges.readOnly();
            if (index.row() >= 0 && index.row() < (int)snapshot->size())
            {
                this->showEditDialog(&snapshot->at(index.row()), index.row());
            }
        });

    // Properly resize the table columns and rows after the model is fully initialized
    // Use a longer delay to ensure the view has been laid out by Qt
    QTimer::singleShot(100, [view] {
        // Resize columns and rows to fit their content
        view->getTableView()->resizeColumnsToContents();
        view->getTableView()->resizeRowsToContents();

        // After initial sizing, make columns stretch to fill available space
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch);
    });
}

bool CustomBadgesPage::filterElements(const QString &query)
{
    // Bypass search filtering for now so we can compile
    return false;
}

void CustomBadgesPage::showEditDialog(const CustomBadge *badge, int row)
{
    QDialog dialog(this);
    dialog.setWindowTitle(badge ? "Edit Custom Badge" : "Add Custom Badge");

    auto *layout = new QFormLayout(&dialog);

    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("e.g. VIP Badge, Special Badge (optional)");

    auto *modeCombo = new QComboBox(&dialog);
    modeCombo->addItems({"Replace", "Addon"});

    auto *typeVersionEdit = new QLineEdit(&dialog);
    typeVersionEdit->setPlaceholderText("e.g. subscriber/1 or moderator");
    auto *typeVersionHelp = new QLabel("Leave empty to match any.", &dialog);

    auto *channelEdit = new QLineEdit(&dialog);
    channelEdit->setPlaceholderText("Limit to specific channel (optional)");

    auto *restrictionEdit = new QLineEdit(&dialog);
    restrictionEdit->setPlaceholderText("User ID or name (optional)");

    auto *imageEdit = new QLineEdit(&dialog);
    imageEdit->setPlaceholderText("image.png or absolute path");
    auto *browseBtn = new QPushButton("Browse...", &dialog);
    auto *folderBtn = new QPushButton("Open Folder", &dialog);

    auto *imageLayout = new QHBoxLayout;
    imageLayout->addWidget(imageEdit);
    imageLayout->addWidget(browseBtn);
    imageLayout->addWidget(folderBtn);

    layout->addRow("Name:", nameEdit);
    layout->addRow("Mode:", modeCombo);
    layout->addRow("Type/Version:", typeVersionEdit);
    layout->addRow("", typeVersionHelp);
    layout->addRow("Channel:", channelEdit);
    layout->addRow("User Restriction:", restrictionEdit);
    layout->addRow("Image:", imageLayout);

    if (badge)
    {
        nameEdit->setText(badge->name());
        modeCombo->setCurrentIndex(badge->isAddon() ? 1 : 0);
        typeVersionEdit->setText(badge->typeAndVersion());
        channelEdit->setText(badge->channelName());
        restrictionEdit->setText(badge->restriction());
        imageEdit->setText(badge->imageFilePath());
    }

    // Connect "Open Folder"
    QString badgesDir = getApp()->getPaths().miscDirectory + "/CustomBadges";
    QObject::connect(folderBtn, &QPushButton::clicked, [badgesDir] {
        (void)QDir().mkpath(badgesDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(badgesDir));
    });

    // Connect "Browse"
    QObject::connect(
        browseBtn, &QPushButton::clicked, [&dialog, imageEdit, badgesDir] {
            (void)QDir().mkpath(badgesDir);
            QString filePath = QFileDialog::getOpenFileName(
                &dialog, "Select Custom Badge Image", badgesDir,
                "Images (*.png *.gif *.jpeg *.jpg)");
            if (!filePath.isEmpty())
            {
                // Check if the file is already in the CustomBadges folder
                QDir dir(badgesDir);
                QString relativePath = dir.relativeFilePath(filePath);

                if (!relativePath.startsWith("..") &&
                    !relativePath.contains('/') && !relativePath.contains('\\'))
                {
                    // File is already in the CustomBadges folder
                    imageEdit->setText(relativePath);
                }
                else
                {
                    // File is outside the CustomBadges folder, copy it to the folder
                    QFileInfo fileInfo(filePath);
                    QString destPath = badgesDir + "/" + fileInfo.fileName();

                    // Handle filename conflicts by appending a number
                    QString baseName = fileInfo.baseName();
                    QString suffix = fileInfo.suffix();
                    int counter = 1;
                    while (QFile::exists(destPath))
                    {
                        QString newFileName = baseName + "_" +
                                              QString::number(counter) + "." +
                                              suffix;
                        destPath = badgesDir + "/" + newFileName;
                        counter++;
                    }

                    // Copy the file
                    if (QFile::copy(filePath, destPath))
                    {
                        // Store just the filename since it's now in the CustomBadges folder
                        imageEdit->setText(QFileInfo(destPath).fileName());
                    }
                    else
                    {
                        // If copy fails, store the full path
                        imageEdit->setText(filePath);
                    }
                }
            }
        });

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog,
                     &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog,
                     &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        // Parse type/version
        QString tv = typeVersionEdit->text().trimmed();
        QString type, version;
        int slashIdx = tv.indexOf('/');
        if (slashIdx != -1)
        {
            type = tv.mid(0, slashIdx);
            version = tv.mid(slashIdx + 1);
        }
        else
        {
            type = tv;
        }

        CustomBadge newBadge(
            type, version, channelEdit->text().trimmed(),
            restrictionEdit->text().trimmed(), imageEdit->text().trimmed(),
            modeCombo->currentIndex() == 1, nameEdit->text().trimmed());

        if (row >= 0)
        {
            getSettings()->customBadges.removeAt(row);
            getSettings()->customBadges.insert(newBadge, row);
        }
        else
        {
            getSettings()->customBadges.append(newBadge);
        }

        std::ignore = getSettings()->requestSave();
    }
}

}  // namespace chatterino
