#include <QEvent>
#include <QDateEdit>
#include <QCalendarWidget>

#include "BugTreeItemDelegate.h"
#include "QDebug"

BugTreeItemDelegate::BugTreeItemDelegate(QObject *parent) :
    QItemDelegate(parent)
{
    dateFormat = "dd/MMM/yyyy";
}



QWidget*
BugTreeItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    if(QDate::fromString(index.data().toString(),dateFormat).isValid())
    {
        qDebug() << QDate::fromString(index.data().toString()).isValid();
        QDateEdit *editor = new QDateEdit(parent);
        QCalendarWidget* cal = new QCalendarWidget;
        editor->setDate(QDate::fromString(index.data().toString(),dateFormat));
        editor->setCalendarPopup(cal);
        editor->setDisplayFormat(dateFormat);
        connect(editor,SIGNAL(editingFinished()),this,SLOT(finishedEditing()));
        return editor;

    }

    else
        return NULL; // Stop creating an editor if it isn't a date.

}

void
BugTreeItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QDateEdit* edit = static_cast<QDateEdit *> (editor);
    model->setData(index,edit->date().toString(dateFormat));
}

void
BugTreeItemDelegate::finishedEditing()
{
    emit(dateFinished());

}
