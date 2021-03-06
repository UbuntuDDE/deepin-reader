/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     leiyu <leiyu@uniontech.com>
*
* Maintainer: leiyu <leiyu@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef SIDEBARIMAGEVIEWMODEL_H
#define SIDEBARIMAGEVIEWMODEL_H

#include <QAbstractListModel>
#include <QMap>

namespace deepin_reader {
class Annotation;
}

typedef enum E_SideBar {
    SIDE_THUMBNIL = 0,
    SIDE_BOOKMARK,
    SIDE_CATALOG,
    SIDE_NOTE,
    SIDE_SEARCH
} E_SideBar;

typedef enum ImageinfoType_e {
    IMAGE_PIXMAP       = Qt::UserRole,
    IMAGE_BOOKMARK     = Qt::UserRole + 1,
    IMAGE_ROTATE       = Qt::UserRole + 2,
    IMAGE_INDEX_TEXT   = Qt::UserRole + 3,
    IMAGE_CONTENT_TEXT = Qt::UserRole + 4,
    IMAGE_SEARCH_COUNT = Qt::UserRole + 5,
    IMAGE_PAGE_SIZE    = Qt::UserRole + 6,
} ImageinfoType_e;

typedef struct ImagePageInfo_t {
    int pageIndex;

    QString strcontents;
    QString strSearchcount;
    deepin_reader::Annotation *annotation = nullptr;

    explicit ImagePageInfo_t(int index = -1);

    bool operator == (const ImagePageInfo_t &other) const;

    bool operator < (const ImagePageInfo_t &other) const;

    bool operator > (const ImagePageInfo_t &other) const;
} ImagePageInfo_t;
Q_DECLARE_METATYPE(ImagePageInfo_t);

class DocSheet;
/**
 * @brief The SideBarImageViewModel class
 * ImageVIew Model
 */
class SideBarImageViewModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SideBarImageViewModel(DocSheet *sheet, QObject *parent = nullptr);

public:
    /**
     * @brief resetData
     * ????????????
     */
    void resetData();

    /**
     * @brief initModelLst
     * ?????????MODEL??????
     * @param pagelst
     * @param sort
     */
    void initModelLst(const QList<ImagePageInfo_t> &pagelst, bool sort = false);

    /**
     * @brief setBookMarkVisible
     * ??????????????????????????????
     * @param pageIndex
     * @param visible
     * @param updateIndex
     */
    void setBookMarkVisible(int pageIndex, bool visible, bool updateIndex = true);

    /**
     * @brief insertPageIndex
     * ????????????????????????
     * @param pageIndex
     */
    void insertPageIndex(int pageIndex);

    /**
     * @brief insertPageIndex
     * ?????????????????????
     * @param tImagePageInfo
     */
    void insertPageIndex(const ImagePageInfo_t &tImagePageInfo);

    /**
     * @brief removePageIndex
     * ????????????????????????
     * @param pageIndex
     */
    void removePageIndex(int pageIndex);

    /**
     * @brief removeItemForAnno
     * ????????????????????????
     * @param annotation
     */
    void removeItemForAnno(deepin_reader::Annotation *annotation);

    /**
     * @brief changeModelData
     * ??????model??????
     * @param pagelst
     */
    void changeModelData(const QList<ImagePageInfo_t> &pagelst);

    /**
     * @brief getModelIndexForPageIndex
     * ????????????????????????MODEL INDEX
     * @param pageIndex
     * @return
     */
    QList<QModelIndex> getModelIndexForPageIndex(int pageIndex);

    /**
     * @brief getModelIndexImageInfo
     * ??????????????????,??????ImagePageInfo_t??????
     * @param modelIndex
     * @param tImagePageInfo
     */
    void getModelIndexImageInfo(int modelIndex, ImagePageInfo_t &tImagePageInfo);

    /**
     * @brief getPageIndexForModelIndex
     * ?????????????????????,??????????????????
     * @param row
     * @return
     */
    int getPageIndexForModelIndex(int row);

    /**
     * @brief findItemForAnno
     * ?????????????????????????????????????????????
     * @param annotation
     * @return
     */
    int findItemForAnno(deepin_reader::Annotation *annotation);

    /**
     * @brief handleThumbnail
     * ???????????????
     * @param index
     * @param pixmap
     */
    void handleRenderThumbnail(int index, QPixmap pixmap);

public slots:
    /**
     * @brief onUpdateImage
     * ??????????????????????????????
     * @param index
     */
    void onUpdateImage(int index);

protected:
    /**
     * @brief columnCount
     * view??????
     * @return
     */
    int columnCount(const QModelIndex &) const override;

    /**
     * @brief rowCount
     * view??????
     * @param parent
     * @return
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief data
     * view????????????
     * @param index
     * @param role
     * @return
     */
    QVariant data(const QModelIndex &index, int role) const override;

    /**
     * @brief setData
     * ??????model??????
     * @param index
     * @param data
     * @param role
     * @return
     */
    bool setData(const QModelIndex &index, const QVariant &data, int role) override;

private:
    QObject *m_parent = nullptr;
    DocSheet *m_sheet = nullptr;
    QList<ImagePageInfo_t> m_pagelst;
    QMap<int, bool> m_cacheBookMarkMap;
    static QMap<QObject *, QVector<QPixmap>> g_sheetPixmapMap;
};

#endif // SIDEBARIMAGEVIEWMODEL_H
