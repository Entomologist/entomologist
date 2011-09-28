#include <QMovie>
#include <QSettings>

#include "SqlSearchModel.h"
#include "SqlSearchDelegate.h"
#include "SqlUtilities.h"
#include "SearchTab.h"
#include "ui_SearchTab.h"

SearchTab::SearchTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SearchTab)
{
    QSettings settings("Entomologist");
    QString lastQuery = settings.value("last-search-query", "").toString();

    ui->setupUi(this);
    mSearchCount = 0;
    pSpinnerMovie = new QMovie(this);
    pSpinnerMovie->setFileName(":/spinner");
    pSpinnerMovie->setScaledSize(QSize(48,48));
    ui->spinnerLabel->setMovie(pSpinnerMovie);
    ui->searchSpinnerWidget->hide();

    if (lastQuery.isEmpty())
    {
        ui->searchQuery->setPlaceholderText(tr("Search summaries and comments"));
    }
    else
    {
        ui->searchQuery->setText(lastQuery);
        ui->searchQuery->setCursorPosition(lastQuery.size());
    }

    ui->trackerCombo->addItem("All Trackers");

    pModel = new SqlSearchModel(this);
    pModel->setTable("search_results");
    pModel->select();
    SqlSearchDelegate *delegate = new SqlSearchDelegate();
    ui->tableView->setItemDelegate(delegate);
    ui->tableView->setModel(pModel);
    pModel->setHeaderData(1, Qt::Horizontal, tr("Tracker"));
    pModel->setHeaderData(2, Qt::Horizontal, tr("Bug ID"));
    pModel->setHeaderData(3, Qt::Horizontal, tr("Summary"));
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setGridStyle(Qt::PenStyle(Qt::DotLine));
    ui->tableView->hideColumn(0); // Hide the internal row id
    ui->tableView->verticalHeader()->hide(); // Hide the Row numbers
    ui->tableView->resizeColumnsToContents();
    ui->tableView->resizeRowsToContents();
    ui->tableView->setAlternatingRowColors(true);
    connect(ui->searchButton, SIGNAL(clicked()),
            this, SLOT(searchButtonClicked()));
    connect(ui->searchQuery, SIGNAL(returnPressed()),
            this, SLOT(searchButtonClicked()));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(itemDoubleClicked(QModelIndex)));
}

SearchTab::~SearchTab()
{
    delete ui;
}

void
SearchTab::itemDoubleClicked(const QModelIndex &index)
{
    // First, find the tracker
    QString trackerName = index.sibling(index.row(), 1).data().toString();
    QString bugId = index.sibling(index.row(), 2).data().toString();
    emit openSearchedBug(trackerName, bugId);
}

void
SearchTab::searchFinished()
{
    mSearchCount--;
    qDebug() << "searchFinished, count: " << mSearchCount;
    if (mSearchCount == 0)
    {
        pModel->select();
        pSpinnerMovie->stop();
        ui->searchSpinnerWidget->hide();
        ui->searchWidget->show();
    }
}

void
SearchTab::searchButtonClicked()
{
    QString search = ui->searchQuery->text();

    if (search.isEmpty())
        return;

    SqlUtilities::clearSearch();
    ui->searchWidget->hide();
    ui->searchSpinnerWidget->show();
    pSpinnerMovie->start();
    mSearchCount = 0;
    int index = ui->trackerCombo->currentIndex();
    QString selected = ui->trackerCombo->currentText();
    QSettings settings("Entomologist");
    settings.setValue("last-search-query", search);

    QMapIterator<QString, Backend*> i(mIdMap);
    while (i.hasNext())
    {
        i.next();
        Backend *b = i.value();

        if ((index == 0)
            || (b->name() == selected))
        {
            mSearchCount++;
            qDebug() << "mSearchCount: " << mSearchCount;
            b->search(search);
        }
     }
}

void
SearchTab::addTracker(Backend *b)
{
    if (!b) return;
    mIdMap[b->id()] = b;

    if (ui->trackerCombo->count() == 1)
        ui->trackerCombo->insertSeparator(1);
    connect(b, SIGNAL(searchFinished()),
            this, SLOT(searchFinished()));
    connect(b, SIGNAL(backendError(QString)),
            this, SLOT(searchFinished()));
    ui->trackerCombo->addItem(b->name());
}

void SearchTab::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
