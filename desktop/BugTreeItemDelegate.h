#ifndef BUGTREEITEMDELEGATE_H
#define BUGTREEITEMDELEGATE_H

#include <QItemDelegate>


class BugTreeItemDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit BugTreeItemDelegate(QObject *parent = 0);
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    QString dateFormat;
signals:
    void dateFinished();
public slots:
        void finishedEditing();

};

#endif // BUGTREEITEMDELEGATE_H
