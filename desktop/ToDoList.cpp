
#include <QStringList>
#include "ToDoList.h"

ToDoList::ToDoList(const QString &listName) : mListName(listName)
{
    mStatus = ToDoList::UNCHANGED;
    mSyncCount = 0;

}


