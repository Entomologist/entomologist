#ifndef TRACKERTABWIDGET_H
#define TRACKERTABWIDGET_H

#include <QTabWidget>

// Reimplement the tracker tab widget in order
// to capture right-clicks on the tabs
class TrackerTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit TrackerTabWidget(QWidget *parent = 0);

signals:
    void showMenu(int tabIndex);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
};

#endif // TRACKERTABWIDGET_H
