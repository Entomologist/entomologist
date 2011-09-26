#ifndef GOOGLECALENDAR_H
#define GOOGLECALENDAR_H
#include "ServicesBackend.h"
class GoogleTasks : public ServicesBackend
{
Q_OBJECT
public:
    GoogleTasks();
    void login();
    void setupList();
    void addTask(ToDoItem* item);
    void deleteTask(ToDoItem* item);
    void newUser();
    void updateCompleted(ToDoItem* item);
    void updateDate(ToDoItem* item);
};

#endif // GOOGLECALENDAR_H
