/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     zhangsong<zhangsong@uniontech.com>
*
* Maintainer: zhangsong<zhangsong@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "CentralDocPage.h"
#include "DocSheet.h"
#include "DocTabBar.h"
#include "MainWindow.h"
#include "SaveDialog.h"
#include "SlideWidget.h"
#include "Application.h"
#include "DBusObject.h"

#include <DDialog>
#include <DTitlebar>
#include <DFileDialog>

#include <QVBoxLayout>
#include <QStackedLayout>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QUrl>
#include <QDesktopServices>
#include <QStackedLayout>
#include <QMimeType>
#include <QMimeDatabase>
#include <QProcess>
#include <QUuid>
#include <QTimer>
#include <QDebug>

CentralDocPage::CentralDocPage(DWidget *parent)
    : BaseWidget(parent)
{
    m_tabBar = new DocTabBar(this);
    connect(m_tabBar, SIGNAL(sigTabChanged(DocSheet *)), this, SLOT(onTabChanged(DocSheet *)));
    connect(m_tabBar, SIGNAL(sigTabMoveIn(DocSheet *)), this, SLOT(onTabMoveIn(DocSheet *)));
    connect(m_tabBar, SIGNAL(sigTabClosed(DocSheet *)), this, SLOT(onTabClosed(DocSheet *)));
    connect(m_tabBar, SIGNAL(sigTabMoveOut(DocSheet *)), this, SLOT(onTabMoveOut(DocSheet *)));
    connect(m_tabBar, SIGNAL(sigTabNewWindow(DocSheet *)), this, SLOT(onTabNewWindow(DocSheet *)));
    connect(m_tabBar, SIGNAL(sigNeedOpenFilesExec()), this, SIGNAL(sigNeedOpenFilesExec()));
    connect(m_tabBar, SIGNAL(sigNeedActivateWindow()), this, SIGNAL(sigNeedActivateWindow()));

    m_stackedLayout = new QStackedLayout;
    m_mainLayout = new QVBoxLayout(this);

    m_mainLayout->addWidget(m_tabBar);
    m_mainLayout->addItem(m_stackedLayout);
    m_mainLayout->setMargin(0);
    m_mainLayout->setSpacing(0);

    this->setLayout(m_mainLayout);

    m_tabLabel = new DLabel(this);
    m_tabLabel->setElideMode(Qt::ElideMiddle);
    m_tabLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    m_tabLabel->setAlignment(Qt::AlignCenter);
    connect(this, SIGNAL(sigSheetCountChanged(int)), this, SLOT(onSheetCountChanged(int)));

    QWidget *mainwindow = parent->parentWidget();
    if (mainwindow) {
        DIconButton *leftButton = m_tabBar->findChild<DIconButton *>("leftButton");
        DIconButton *rightButton = m_tabBar->findChild<DIconButton *>("rightButton");
        DIconButton *addButton = m_tabBar->findChild<DIconButton *>("AddButton");

        QList<QWidget *> orderlst = mainwindow->property("orderlist").value<QList<QWidget *>>();
        orderlst << leftButton;
        orderlst << rightButton;
        orderlst << addButton;
        mainwindow->setProperty("orderlist", QVariant::fromValue(orderlst));
        mainwindow->setProperty("orderWidgets", QVariant::fromValue(orderlst));
    }
}

CentralDocPage::~CentralDocPage()
{
}

bool CentralDocPage::firstThumbnail(QString filePath, QString thumbnailPath)
{
    int fileType = Dr::fileType(filePath);
    if (Dr::DJVU == fileType) {
        QImage image = DocSheet::firstThumbnail(filePath);
        if (image.isNull())
            return false;
        return image.save(thumbnailPath, "PNG");
    }

    return false;
}

void CentralDocPage::openCurFileFolder()
{
    DocSheet *sheet = getCurSheet();
    if (nullptr == sheet)
        return;

    QString filePath = sheet->filePath();
    bool result = QProcess::startDetached(QString("dde-file-manager \"%1\" --show-item").arg(filePath));
    if (!result) {
        QDesktopServices::openUrl(QUrl(QFileInfo(filePath).dir().path()));
    }
}

void CentralDocPage::onSheetFileChanged(DocSheet *sheet)
{
    if (DocSheet::existFileChanged()) {
        DBusObject::instance()->blockShutdown();
    } else {
        DBusObject::instance()->unBlockShutdown();
    }

    if (nullptr == sheet && sheet != getCurSheet())
        return;

    emit sigCurSheetChanged(sheet);
}

void CentralDocPage::onSheetOperationChanged(DocSheet *sheet)
{
    if (nullptr == sheet && sheet != getCurSheet())
        return;

    emit sigCurSheetChanged(sheet);
}

void CentralDocPage::addSheet(DocSheet *sheet)
{
    m_tabBar->insertSheet(sheet);

    enterSheet(sheet);
}

void CentralDocPage::addFileAsync(const QString &filePath)
{
    //????????????????????????????????????filePath??????????????????????????????sheet???????????????????????????
    if (m_tabBar) {
        int index = m_tabBar->indexOfFilePath(filePath);
        if (index >= 0 && index < m_tabBar->count()) {
            m_tabBar->setCurrentIndex(index);
            return;
        }
    }

    Dr::FileType fileType = Dr::fileType(filePath);

    if (Dr::PDF != fileType && Dr::DJVU != fileType && Dr::DOCX != fileType) {
        showTips(m_stackedLayout->currentWidget(), tr("The format is not supported"), 1);
        return;
    }

    DocSheet *sheet = new DocSheet(fileType, filePath, this);

    connect(sheet, SIGNAL(sigFileChanged(DocSheet *)), this, SLOT(onSheetFileChanged(DocSheet *)));
    connect(sheet, SIGNAL(sigOperationChanged(DocSheet *)), this, SLOT(onSheetOperationChanged(DocSheet *)));
    connect(sheet, SIGNAL(sigFindOperation(const int &)), this, SIGNAL(sigFindOperation(const int &)));
    connect(sheet, &DocSheet::sigFileOpened, this, &CentralDocPage::onOpened);

    m_stackedLayout->addWidget(sheet);

    m_stackedLayout->setCurrentWidget(sheet);

    m_tabBar->insertSheet(sheet);

    this->activateWindow();

    sheet->defaultFocus();

    emit sigSheetCountChanged(m_stackedLayout->count());

    emit sigCurSheetChanged(static_cast<DocSheet *>(m_stackedLayout->currentWidget()));

    sheet->openFileAsync("");
}

void CentralDocPage::onOpened(DocSheet *sheet, deepin_reader::Document::Error error)
{
    if (deepin_reader::Document::FileError == error || deepin_reader::Document::FileDamaged == error || deepin_reader::Document::ConvertFailed == error) {
        m_stackedLayout->removeWidget(sheet);

        m_tabBar->removeSheet(sheet);

        emit sigSheetCountChanged(m_stackedLayout->count());

        emit sigCurSheetChanged(static_cast<DocSheet *>(m_stackedLayout->currentWidget()));

        sheet->deleteLater();

        if (deepin_reader::Document::FileError == error)
            showTips(nullptr, tr("Open failed"), 1);
        else if (deepin_reader::Document::FileDamaged == error)
            showTips(nullptr, tr("Please check if the file is damaged"), 1);
        else if (deepin_reader::Document::ConvertFailed == error)
            showTips(nullptr, tr("Conversion failed, please check if the file is damaged"), 1);

        return;
    }

    if (nullptr == sheet)
        return;

    this->activateWindow();

    sheet->defaultFocus();
}

void CentralDocPage::onTabChanged(DocSheet *sheet)
{
    if (nullptr != sheet) {
        m_stackedLayout->setCurrentWidget(sheet);

        sheet->defaultFocus();
    }

    emit sigCurSheetChanged(sheet);
}

void CentralDocPage::onTabMoveIn(DocSheet *sheet)
{
    if (nullptr == sheet)
        return;

    enterSheet(sheet);
}

void CentralDocPage::onTabClosed(DocSheet *sheet)
{
    closeSheet(sheet, true);
}

void CentralDocPage::onTabMoveOut(DocSheet *sheet)
{
    if (nullptr == sheet)
        return;

    leaveSheet(sheet);

    if (m_stackedLayout->count() <= 0) {
        emit sigNeedClose();
        return;
    }
}

void CentralDocPage::onTabNewWindow(DocSheet *sheet)
{
    leaveSheet(sheet);

    MainWindow *window = MainWindow::createWindow(sheet);

    window->move(QCursor::pos());

    window->show();
}

void CentralDocPage::onCentralMoveIn(DocSheet *sheet)
{
    addSheet(sheet);
}

void CentralDocPage::leaveSheet(DocSheet *sheet)
{
    if (nullptr == sheet)
        return;

    m_stackedLayout->removeWidget(sheet);

    disconnect(sheet, SIGNAL(sigFileChanged(DocSheet *)), this, SLOT(onSheetFileChanged(DocSheet *)));
    disconnect(sheet, SIGNAL(sigOperationChanged(DocSheet *)), this, SLOT(onSheetOperationChanged(DocSheet *)));
    disconnect(sheet, SIGNAL(sigFindOperation(const int &)), this, SIGNAL(sigFindOperation(const int &)));

    emit sigSheetCountChanged(m_stackedLayout->count());

    emit sigCurSheetChanged(static_cast<DocSheet *>(m_stackedLayout->currentWidget()));
}

bool CentralDocPage::closeSheet(DocSheet *sheet, bool needToBeSaved)
{
    if (nullptr == sheet)
        return false;

    if (!DocSheet::existSheet(sheet))
        return false;

    //????????????????????????????????????
    if (sheet->fileChanged() && needToBeSaved) {
        int result = SaveDialog::showExitDialog(QFileInfo(sheet->filePath()).fileName());

        if (result <= 0) {
            return false;
        }

        if (result == 2) {
            //docx????????????????????????????????????
            if (Dr::DOCX == sheet->fileType()) {
                if (!saveAsCurrent())
                    return false;
            } else {
                if (!sheet->saveData())
                    return false;
            }
        }
    }

    m_stackedLayout->removeWidget(sheet);

    if (m_tabBar) {
        m_tabBar->removeSheet(sheet);
    }

    emit sigSheetCountChanged(m_stackedLayout->count());

    emit sigCurSheetChanged(static_cast<DocSheet *>(m_stackedLayout->currentWidget()));

    delete sheet;

    return true;
}

bool CentralDocPage::closeAllSheets(bool needToBeSaved)
{
    QList<DocSheet *> sheets = m_tabBar->getSheets();

    if (sheets.count() > 0) {
        for (int i = 0; i < sheets.count(); ++i) {
            showSheet(sheets[i]);
            if (!closeSheet(sheets[i], needToBeSaved))
                return false;
        }
    }

    return true;
}

void CentralDocPage::enterSheet(DocSheet *sheet)
{
    if (nullptr == sheet)
        return;

    sheet->setParent(this);

    connect(sheet, SIGNAL(sigFileChanged(DocSheet *)), this, SLOT(onSheetFileChanged(DocSheet *)));
    connect(sheet, SIGNAL(sigOperationChanged(DocSheet *)), this, SLOT(onSheetOperationChanged(DocSheet *)));
    connect(sheet, SIGNAL(sigFindOperation(const int &)), this, SIGNAL(sigFindOperation(const int &)));

    m_stackedLayout->addWidget(sheet);

    m_stackedLayout->setCurrentWidget(sheet);

    sheet->defaultFocus();

    emit sigSheetCountChanged(m_stackedLayout->count());

    emit sigCurSheetChanged(static_cast<DocSheet *>(m_stackedLayout->currentWidget()));
}

bool CentralDocPage::hasSheet(DocSheet *sheet)
{
    if (nullptr == sheet)
        return false;

    auto sheets = this->findChildren<DocSheet *>();

    for (int i = 0; i < sheets.count(); ++i) {
        if (sheets[i] == sheet && DocSheet::existSheet(sheet)) {
            return true;
        }
    }

    return false;
}

void CentralDocPage::showSheet(DocSheet *sheet)
{
    if (nullptr == sheet)
        return;

    m_tabBar->showSheet(sheet);
}

QList<DocSheet *> CentralDocPage::getSheets()
{
    return m_tabBar->getSheets();
}

bool CentralDocPage::saveCurrent()
{
    DocSheet *sheet = static_cast<DocSheet *>(m_stackedLayout->currentWidget());

    if (nullptr == sheet)
        return false;

    if (!sheet->fileChanged()) {
        return false;
    }

    //docx????????????????????????????????????
    if (Dr::DOCX == sheet->fileType()) {
        return saveAsCurrent();
    }

    if (!sheet->saveData()) {
        showTips(m_stackedLayout->currentWidget(), tr("Save failed"), 1);
        return false;
    }

    sigCurSheetChanged(sheet);

    showTips(m_stackedLayout->currentWidget(), tr("Saved successfully"));

    return true;
}

bool CentralDocPage::saveAsCurrent()
{
    DocSheet *sheet = getCurSheet();

    if (nullptr == sheet)
        return false;

    //????????????????????????????????????docx??????????????????pdf?????????????????????
    QString showFilePath = sheet->filePath();

    if (Dr::DOCX == sheet->fileType())
        showFilePath.replace(".docx", ".pdf");

    QString saveFilePath = DFileDialog::getSaveFileName(this, tr("Save as"), showFilePath, sheet->filter());

    //???????????????
    if (saveFilePath.isEmpty())
        return false;

    //????????????????????????
    if (Dr::PDF == sheet->fileType() || Dr::DOCX == sheet->fileType()) {
        if (saveFilePath.endsWith("/.pdf")) {
            DDialog dlg("", tr("Invalid file name"));
            dlg.setIcon(QIcon::fromTheme(QString("dr_") + "exception-logo"));
            dlg.addButtons(QStringList() << tr("OK", "button"));
            QMargins mar(0, 0, 0, 30);
            dlg.setContentLayoutContentsMargins(mar);
            dlg.exec();
            return false;
        }
    } else if (Dr::DJVU == sheet->fileType()) {
        if (saveFilePath.endsWith("/.djvu")) {
            DDialog dlg("", tr("Invalid file name"));
            dlg.setIcon(QIcon::fromTheme(QString("dr_") + "exception-logo"));
            dlg.addButtons(QStringList() << tr("OK", "button"));
            QMargins mar(0, 0, 0, 30);
            dlg.setContentLayoutContentsMargins(mar);
            dlg.exec();
            return false;
        }
    }

    return sheet->saveAsData(saveFilePath);
}

DocSheet *CentralDocPage::getCurSheet()
{
    if (m_stackedLayout != nullptr) {
        return dynamic_cast<DocSheet *>(m_stackedLayout->currentWidget());
    }

    return nullptr;
}

DocSheet *CentralDocPage::getSheet(const QString &filePath)
{
    auto sheets = this->findChildren<DocSheet *>();
    foreach (auto sheet, sheets) {
        if (sheet->filePath() == filePath) {
            return sheet;
        }
    }

    return nullptr;
}

void CentralDocPage::handleShortcut(const QString &s)
{
    if (s == Dr::key_esc && m_slideWidget) {
        quitSlide();
        return;
    }

    if (s == Dr::key_f11 && m_slideWidget) { //???????????? f11??????????????????    ???
        return;
    }

    if (s == Dr::key_esc && !m_magniferSheet.isNull() && m_magniferSheet->magnifierOpened()) {
        quitMagnifer();
        return;
    }

    if ((s == Dr::key_esc || s == Dr::key_f11) && getCurSheet() && getCurSheet()->closeFullScreen())
        return;

    if (m_slideWidget) {
        m_slideWidget->handleKeyPressEvent(s);
        return;
    }

    if (s == Dr::key_ctrl_s) {
        saveCurrent();
        handleBlockShutdown();
    } else if (s == Dr::key_ctrl_shift_s) {
        saveAsCurrent();
    } else if (s == Dr::key_f5) {
        openSlide();
    } else if (s == Dr::key_alt_z) {
        openMagnifer();
    } else { //  ?????????????????? CurSheet ??????????????????
        auto sheet = getCurSheet();
        if (sheet) {
            if (s == Dr::key_ctrl_p) {
                QTimer::singleShot(1, sheet, SLOT(onPopPrintDialog()));
            } else if (s == Dr::key_alt_1) {
                sheet->setMouseShape(Dr::MouseShapeNormal);
            } else if (s == Dr::key_alt_2) {
                sheet->setMouseShape(Dr::MouseShapeHand);
            } else if (s == Dr::key_ctrl_1) {
                sheet->setScaleMode(Dr::FitToPageDefaultMode);
            } else if (s == Dr::key_ctrl_m) {
                sheet->setSidebarVisible(true);
            } else if (s == Dr::key_ctrl_2) {
                sheet->setScaleMode(Dr::FitToPageHeightMode);
            } else if (s == Dr::key_ctrl_3) {
                sheet->setScaleMode(Dr::FitToPageWidthMode);
            } else if (s == Dr::key_ctrl_r) {
                sheet->rotateLeft();
            } else if (s == Dr::key_ctrl_shift_r) {
                sheet->rotateRight();
            } else if (s == Dr::key_alt_harger || s == Dr::key_ctrl_equal) {
                sheet->zoomin();
            } else if (s == Dr::key_ctrl_smaller) {
                sheet->zoomout();
            } else if (s == Dr::key_ctrl_d) {
                sheet->setBookMark(sheet->currentIndex(), true);
            } else if (s == Dr::key_ctrl_f) {
                sheet->prepareSearch();
            } else if (s == Dr::key_ctrl_c) {
                sheet->copySelectedText();
            } else if (s == Dr::key_alt_h) {
                sheet->highlightSelectedText();
            } else if (s == Dr::key_alt_a) {
                sheet->addSelectedTextHightlightAnnotation();
            }
            // ??????????????????????????????????????????????????????????????????
            /*else if (s == Dr::key_left) {
                sheet->jumpToPrevPage();
            } else if (s == Dr::key_right) {
                sheet->jumpToNextPage();
            } */else if (s == Dr::key_f11) {
                sheet->openFullScreen();
            }
        }
    }
}

void CentralDocPage::showTips(QWidget *parent, const QString &tips, int iconIndex)
{
    emit sigNeedShowTips(parent, tips, iconIndex);
}

void CentralDocPage::openMagnifer()
{
    quitMagnifer();

    m_magniferSheet = getCurSheet();

    if (!m_magniferSheet.isNull())
        m_magniferSheet->openMagnifier();
}

//  ???????????????
void CentralDocPage::quitMagnifer()
{
    if (!m_magniferSheet.isNull() && m_magniferSheet->magnifierOpened()) {
        m_magniferSheet->closeMagnifier();
        m_magniferSheet = nullptr;
    }
}

void CentralDocPage::openSlide()
{
    DocSheet *docSheet = getCurSheet();
    if (docSheet && docSheet->opened() && m_slideWidget == nullptr) {
        m_slideWidget = new SlideWidget(getCurSheet());
    }
}

void CentralDocPage::quitSlide()
{
    if (m_slideWidget) {
        m_slideWidget->close();
        m_slideWidget = nullptr;
    }
}

bool CentralDocPage::isSlide()
{
    return m_slideWidget != nullptr;
}

void CentralDocPage::prepareSearch()
{
    DocSheet *docSheet = getCurSheet();

    if (docSheet)
        docSheet->prepareSearch();
}

bool CentralDocPage::isFullScreen()
{
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(parentWidget()->parentWidget()->parentWidget());

    if (nullptr == mainWindow)
        return false;

    return mainWindow->isFullScreen();
}

void CentralDocPage::openFullScreen()
{
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(parentWidget()->parentWidget()->parentWidget());

    if (nullptr == mainWindow)
        return;

    if (!mainWindow->isFullScreen()) {
        m_mainLayout->removeWidget(m_tabBar);

        m_isMaximizedBeforeFullScreen = mainWindow->isMaximized();

        mainWindow->setDocTabBarWidget(m_tabBar);

        mainWindow->showFullScreen();
    }
}

bool CentralDocPage::quitFullScreen(bool force)
{
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(parentWidget()->parentWidget()->parentWidget());

    if (nullptr == mainWindow)
        return false;

    if (mainWindow->isFullScreen() || force) {
        m_tabBar->setParent(this);

        m_mainLayout->insertWidget(0, m_tabBar);

        m_tabBar->setVisible(m_tabBar->count() > 1);

        mainWindow->setDocTabBarWidget(nullptr);

        if (m_isMaximizedBeforeFullScreen)
            mainWindow->showMaximized();
        else
            mainWindow->showNormal();

        return true;
    }

    return false;
}

void CentralDocPage::onSheetCountChanged(int count)
{
    if (count == 0)
        return;

    if (count == 1) {
        //tabText(0)???????????????????????????????????????????????????????????????
        QTimer::singleShot(10, this, SLOT(onUpdateTabLabelText()));

        m_tabLabel->setVisible(true);

        m_tabBar->setVisible(false);
    } else {
        m_tabLabel->setVisible(false);

        m_tabBar->setVisible(true);
    }

    MainWindow *mainWindow = dynamic_cast<MainWindow *>(parentWidget()->parentWidget()->parentWidget());

    if (mainWindow && mainWindow->isFullScreen()) {
        mainWindow->resizeFullTitleWidget();

    } else if (m_tabBar->parent() != this) {
        m_tabBar->setParent(this);

        m_mainLayout->insertWidget(0, m_tabBar);

        m_tabBar->setVisible(count > 1);
    }
}

void CentralDocPage::onUpdateTabLabelText()
{
    if (m_tabBar->count() > 0)
        m_tabLabel->setText(m_tabBar->tabText(0));
}

QWidget *CentralDocPage::getTitleLabel()
{
    return m_tabLabel;
}

void CentralDocPage::handleBlockShutdown()
{
    bool bBlock = false;
    QList<DocSheet *> sheets = m_tabBar->getSheets();

    // ?????????????????????????????????
    for (int i = 0; i < sheets.count(); ++i) {
        if (sheets[i] && sheets[i]->existFileChanged()) {
            bBlock = true;
            break;
        }
    }

    if (bBlock) {
        DBusObject::instance()->blockShutdown();    // ???????????????????????????????????????
    } else {
        DBusObject::instance()->unBlockShutdown();  // ????????????????????????????????????????????????
    }
}

void CentralDocPage::zoomIn()
{
    if (getCurSheet()) {
        getCurSheet()->zoomin();
    }
}

void CentralDocPage::zoomOut()
{
    if (getCurSheet()) {
        getCurSheet()->zoomout();
    }
}
