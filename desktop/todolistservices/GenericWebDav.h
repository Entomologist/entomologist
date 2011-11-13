#ifndef GENERICWEBDAV_H
#define GENERICWEBDAV_H
#include "ServicesBackend.h"

class GenericWebDav : public ServicesBackend
{
Q_OBJECT
public:
    GenericWebDav();
    void login();
    void setupList();
    void addTask(ToDoItem* item);
    void deleteTask(ToDoItem* item);
    void updateCompleted(ToDoItem* item);
    void updateDate(ToDoItem* item);
    void newUser();
};

#endif // GENERICWEBDAV_H
