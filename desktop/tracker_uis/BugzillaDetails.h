#ifndef BUGZILLADETAILS_H
#define BUGZILLADETAILS_H

#include <QWidget>
#include "BackendDetails.h"

namespace Ui {
    class BugzillaDetails;
}

class BugzillaDetails : public BackendDetails
{
    Q_OBJECT

public:
    explicit BugzillaDetails(QWidget *parent = 0);
    ~BugzillaDetails();
    void setSeverities(const QString &selected, QStringList severities);
    void setPriorities(const QString &selected, QStringList priorities);
    void setStatuses(const QString &selected, QStringList statuses);
    void setResolutions(const QString &selected, QStringList resolutions);
    void setAssigneds(const QString &assigned, QStringList assignedVals);
    void setProduct(const QString &selected);
    void setComponent(const QString &selected);
    QMap<QString, QString> fieldDetails();

public slots:
    void statusIndexChanged(const QString &value);

private:
    Ui::BugzillaDetails *ui;
    // These are used to see if a combo box is set to a new value
    QString mSeverity, mPriority, mStatus, mResolution, mAssigned;

};

#endif // BUGZILLADETAILS_H
