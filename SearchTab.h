#ifndef SEARCHTAB_H
#define SEARCHTAB_H

#include <QWidget>
#include "trackers/Backend.h"


namespace Ui {
    class SearchTab;
}

class SqlSearchModel;
class QMovie;

class SearchTab : public QWidget {
    Q_OBJECT

public:
    SearchTab(QWidget *parent = 0);
    ~SearchTab();

    void addTracker(Backend *b);
    void removeTracker(Backend *b);
    void renameTracker(const QString &oldName,
                       const QString &newName);
signals:
    void openSearchedBug(const QString &trackerName,
                         const QString &bugId);
public slots:
    void searchButtonClicked();
    void searchFinished();
    void itemDoubleClicked(const QModelIndex &index);

protected:
    void changeEvent(QEvent *e);

private:
    Ui::SearchTab *ui;
    QMap<QString, Backend *> mIdMap;
    int mSearchCount;
    SqlSearchModel *pModel;
    QMovie *pSpinnerMovie;
};

#endif // SEARCHTAB_H
