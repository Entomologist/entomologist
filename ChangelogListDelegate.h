#ifndef CHANGELOGLISTDELEGATE_H
#define CHANGELOGLISTDELEGATE_H

#include <QStyledItemDelegate>

class ChangelogListDelegate : public QStyledItemDelegate
{
public:
    ChangelogListDelegate(QObject *parent = 0);
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem & option,
                   const QModelIndex & index ) const;
};


#endif // CHANGELOGLISTDELEGATE_H
