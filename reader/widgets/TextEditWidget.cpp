/*
 * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     duanxiaohui <wangzhixuan@uniontech.com>
 *
 * Maintainer: duanxiaohui <duanxiaohui@uniontech.com>
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
#include "TextEditWidget.h"
#include "DocSheet.h"
#include "MsgHeader.h"
#include "Model.h"
#include "TransparentTextEdit.h"
#include "SheetBrowser.h"
#include "Application.h"
#include "Utils.h"
#include "DBusObject.h"

#include <DPlatformWindowHandle>
#include <DWindowManagerHelper>

#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPointF>
#include <QMenu>

TextEditShadowWidget::TextEditShadowWidget(QWidget *parent)
    : DWidget(parent)
{
    setWindowFlag(Qt::Popup);

    setAttribute(Qt::WA_TranslucentBackground);

    setMaximumSize(QSize(278, 344));

    QHBoxLayout *pHLayoutContant = new QHBoxLayout;

    pHLayoutContant->setMargin(12);

    this->setLayout(pHLayoutContant);

    m_TextEditWidget = new TextEditWidget(this);

    pHLayoutContant->addWidget(m_TextEditWidget);

    m_TextEditWidget->m_brower = dynamic_cast<SheetBrowser *>(parent);

    setObjectName("TextEditShadowWidget");
}

TextEditWidget *TextEditShadowWidget::getTextEditWidget()
{
    return m_TextEditWidget;
}

void TextEditShadowWidget::showWidget(const QPoint &point)
{
    QPoint pos = point;

    move(pos);

    raise();

    show();

    m_TextEditWidget->setEditFocus();
}

TextEditWidget::TextEditWidget(DWidget *parent)
    : BaseWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);

    initWidget();

    m_showMenuTimer = new  QTimer(this);
    m_showMenuTimer->setInterval(1000);
    connect(m_showMenuTimer, &QTimer::timeout, this, [ = ] {
        m_showMenuTimer->stop();
        this->onShowMenu();
    });

    connect(DBusObject::instance(), &DBusObject::sigTouchPadEventSignal, this, &TextEditWidget::onTouchPadEvent);
}

void TextEditWidget::onShowMenu()
{
    if (this->isVisible() && m_pTextEdit) {
        QMenu *menu =  m_pTextEdit->createStandardContextMenu();
        if (menu) {
            menu->exec(QCursor::pos());
            delete  menu;
            menu = nullptr;
        }
    }
}

void TextEditWidget::setEditText(const QString &note)
{
    m_pTextEdit->clear();
    m_pTextEdit->setPlainText(note);
    m_strNote = note;

    QTextCursor cursor = m_pTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_pTextEdit->setTextCursor(cursor);
}

void TextEditWidget::setAnnotation(deepin_reader::Annotation *annotation)
{
    m_annotation = annotation;
}

void TextEditWidget::setEditFocus()
{
    m_pTextEdit->setFocus();
}

void TextEditWidget::hideEvent(QHideEvent *event)
{
    BaseWidget::hideEvent(event);
    QString sText = m_pTextEdit->toPlainText().trimmed();

    if (m_annotation == nullptr)
        return;

    if (m_annotation->type() == deepin_reader::Annotation::AText &&  sText.isEmpty()) {
        emit sigRemoveAnnotation(m_annotation, !m_annotation->contents().isEmpty());
    } else if (m_annotation->contents() != sText) {
        emit sigUpdateAnnotation(m_annotation, sText);
    }

    emit sigHide();
}

void TextEditWidget::initWidget()
{
    setFixedSize(QSize(254, 320));
    setMinimumHeight(310);
    setMaximumHeight(320);

    QHBoxLayout *pHLayoutContant = new QHBoxLayout;
    pHLayoutContant->setContentsMargins(20, 20, 6, 20);

    m_pTextEdit = new TransparentTextEdit(this);
    connect(m_pTextEdit, SIGNAL(sigNeedShowTips(const QString &, int)), this, SIGNAL(sigNeedShowTips(const QString &, int)));

    pHLayoutContant->addWidget(m_pTextEdit);
    this->setLayout(pHLayoutContant);

    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setOffset(4, 4);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    shadowEffect->setBlurRadius(16);
    setGraphicsEffect(shadowEffect);

    onBlurWindowChanged();
    QObject::connect(DWindowManagerHelper::instance(), &DWindowManagerHelper::hasCompositeChanged, this, &TextEditWidget::onBlurWindowChanged);
}

void TextEditWidget::onBlurWindowChanged()
{
    if (DWindowManagerHelper::instance()->hasComposite()) {
        this->graphicsEffect()->setEnabled(true);
        parentWidget()->layout()->setMargin(12);

    } else {
        this->graphicsEffect()->setEnabled(false);
        parentWidget()->layout()->setMargin(0);
    }
}

void TextEditWidget::onTouchPadEvent(QString name, QString direction, int fingers)
{
    Q_UNUSED(name)

    if (fingers == 0) {
        if (direction == "up") {
            m_showMenuTimer->stop();
        } else if (direction == "down") {
            if (!m_showMenuTimer->isActive()) {
                m_showMenuTimer->start();
            }
        }
    }
}

void TextEditWidget::paintEvent(QPaintEvent *event)
{
    BaseWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    QPainterPath clippath;
    clippath.setFillRule(Qt::WindingFill);
    int minRadius = 0;
    int maxRadius = 0;
    if (DWindowManagerHelper::instance()->hasComposite()) {
        minRadius = 16;
        maxRadius = 32;
    }
    clippath.moveTo(minRadius, 0);
    clippath.lineTo(this->width() - minRadius, 0);
    clippath.arcTo(this->width() - minRadius * 2, 0, minRadius * 2, minRadius * 2, 90, -90);
    clippath.lineTo(this->width(), this->height() - minRadius * 2);
    clippath.arcTo(this->width() - maxRadius * 2, this->height() - maxRadius * 2, maxRadius * 2, maxRadius * 2, 0, -90);
    clippath.lineTo(minRadius, this->height());
    clippath.arcTo(0, this->height() - minRadius * 2, minRadius * 2, minRadius * 2, 270, -90);
    clippath.lineTo(0, minRadius);
    clippath.arcTo(0, 0, minRadius * 2, minRadius * 2, 180, -90);
    clippath.closeSubpath();
    painter.setClipPath(clippath);

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(this->width(), this->height()));
    linearGrad.setColorAt(0, QColor(255, 244, 196));
    linearGrad.setColorAt(0.48, QColor(255, 238, 189));
    linearGrad.setColorAt(0.90, QColor(255, 229, 161));
    linearGrad.setColorAt(0.96, QColor(255, 251, 225));
    linearGrad.setColorAt(1, QColor(255, 251, 225));
    painter.setBrush(linearGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRect(this->rect());

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0, 0, 0, 51));
    painter.drawPath(clippath);
}
