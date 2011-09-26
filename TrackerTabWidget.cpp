#include <QTabBar>
#include <QContextMenuEvent>
#include "TrackerTabWidget.h"

TrackerTabWidget::TrackerTabWidget(QWidget *parent) :
    QTabWidget(parent)
{
}

void TrackerTabWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int index = tabBar()->tabAt(tabBar()->mapFrom(this, event->pos()));
    if (index == -1)
        return;

    if (currentIndex() != index)
        setCurrentIndex(index);

    emit showMenu(index);
}
