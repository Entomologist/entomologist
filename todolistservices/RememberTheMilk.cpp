/*
 *  Copyright (c) 2011 Novell, Inc.
 *  All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, contact Novell, Inc.
 *
 *  To contact Novell about this file by physical or electronic mail,
 *  you may find current contact information at www.novell.com
 *
 *  Author: David Williams <redache@googlemail.com>
 *
 */
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QDateTime>

#include "RememberTheMilk.h"
#include "ServicesBackend.h"
#include "QDebug"
#include <qjson/json_parser.hh>
#include "ToDoItem.h"

RememberTheMilk::RememberTheMilk(QString service, bool state)
    : serviceName(service),
      loginState(state)
{
    serviceType_ = "Remember The Milk";
    apiKey = "7714673fe0ea7961dd885fb6c895aa79";
    secret = "0aecb45bfbd2cab5";
    manager = new QNetworkAccessManager();
    mURL= "https://api.rememberthemilk.com/services/rest/?";
}

void
RememberTheMilk::setList(ToDoList *list)
{
    todoList = list;
}

void
RememberTheMilk::login()
{
    if(!loginState)
    {
        QSqlQuery query("SELECT auth_key FROM services WHERE name=:name");
        query.bindValue(":name",serviceName);

        query.exec();

        while(query.next())
        {
            authToken = query.value(0).toString();
        }
        if(!authToken.isEmpty())
            checkToken();
        else
        {
            loginState=true;
            login();
        }
    }
    else
    {

        loginState = false;
        generateFrob();
    }
}

void
RememberTheMilk::generateFrob()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append("format=json");
    parameters.append("method=rtm.auth.getFrob");

    QString api_sig=generateSig(parameters);
    QNetworkRequest req = QNetworkRequest(QUrl(mURL));

    QString query = QString("%1&api_sig=%2&%3&%4")
            .arg(parameters.at(0))
            .arg(api_sig)
            .arg(parameters.at(1))
            .arg(parameters.at(2));

    QNetworkReply* rep = manager->post(req,query.toAscii());

    connect(rep,SIGNAL(finished()),this,SLOT(frobResponse()));

}

void
RememberTheMilk::frobResponse()
{

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if(reply->error())
    {
        emit serviceError(reply->errorString());
        reply->close();
        return;

    }

    QString response = reply->readAll();
    reply->close();

    QJson::Parser parser;
    bool ok;
    QVariant frob = parser.parse(response.toAscii(), &ok);
    if(ok)
    {
        QVariant outerMap = frob.toMap().value("rsp");
        QVariant innerMap = outerMap.toMap().value("frob");
        validFrob = innerMap.toString();
        regUser();

    }
    else
    {
        emit serviceError(parser.errorString());
    }
}

void
RememberTheMilk::generateToken()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("frob=%1").arg(validFrob));
    parameters.append("format=json");
    parameters.append("method=rtm.auth.getToken");

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&api_sig=%5")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));

    QNetworkReply* res = manager->post(req,query.toAscii());

    connect(res,SIGNAL(finished()),this,SLOT(tokenResponse()));

}

void
RememberTheMilk::tokenResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if(reply->error())
    {
        emit serviceError(reply->errorString());
        reply->close();
        return;
    }

    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;
    QVariant json = parser.parse(response.toAscii(), &ok);
    if(ok)
    {
        QVariant rsp = json.toMap().value("rsp");
        QVariant auth = rsp.toMap().value("auth");
        QVariant token = auth.toMap().value("token");
        authToken = token.toString();
        if(authToken.compare("") != 0)
        {
            emit regSuccess();
            insertKey();
            checkToken();

        }
        else
            login();

    }
    else
    {
        emit serviceError(parser.errorString());
        return;
    }

}

void
RememberTheMilk::insertKey()
{

    QSqlQuery query("UPDATE services SET auth_key =:key WHERE name=:name");
    query.bindValue(":key",authToken);
    query.bindValue(":name",serviceName);
    query.exec();

}

void
RememberTheMilk::checkToken()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append("format=json");
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("method=rtm.auth.checkToken");

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&api_sig=%5")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(api_sig);
    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(checkResponse()));
}

void
RememberTheMilk::checkResponse()
{

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(reply->error())
    {
        emit ServicesBackend::serviceError(reply->errorString());
        reply->close();
        return;

    }
    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;
    QVariant frob = parser.parse(response.toAscii(), &ok);
    QVariant outerMap = frob.toMap().value("rsp");
    QString status = outerMap.toMap().value("stat").toString();

    if(status.compare("fail") == 0)
    {

        loginState = true;
        login();

    }
    else
    {
        loginState = false;
        emit authCompleted();

    }

}

void
RememberTheMilk::newUser()
{

    generateToken();

}

void
RememberTheMilk::regUser()
{

    QStringList parameters;
    QString url= "http://www.rememberthemilk.com/services/auth/?";
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append("perms=delete");
    parameters.append(QString("frob=%1").arg(validFrob));
    QString api_sig = generateSig(parameters);
    QString query = QString("%1&%2&%3&api_sig=%4")

            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(api_sig);

    QDesktopServices::openUrl(QUrl(url + query));

    emit loginWaiting();

}

void
RememberTheMilk::setupList()
{

    createTimeline();
}

// Timelines are for undo - We don't need it but it's required by the API.
void
RememberTheMilk::createTimeline()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.timelines.create");

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&api_sig=%5")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(timelineResponse()));
}

void
RememberTheMilk::timelineResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    QJson::Parser parser;
    bool ok;
    QVariant initial = parser.parse(response.toAscii(), &ok);
    if(ok)
    {
        QVariant outer = initial.toMap().value("rsp");
        timeline = outer.toMap().value("timeline").toString();
    }
    else
        emit serviceError(parser.errorString());
    reply->close();

    getLists();

}

void
RememberTheMilk::getLists()
{

    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.lists.getList");

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&api_sig=%5")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(getListsResponse()));
}

void
RememberTheMilk::getListsResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(reply->error())
    {

        emit serviceError(reply->errorString());
        reply->close();

    }

    QString response = reply->readAll();

    QJson::Parser parser;
    bool ok;
    QVariantMap initial = parser.parse(response.toAscii(),&ok).toMap();
    QVariantMap rsp = initial["rsp"].toMap();
    QVariant stat = initial["stat"];
    if(stat.toString().compare("fail") == 0)
        emit serviceError("An unkown error has occured " + response);

    QVariantMap list = rsp["lists"].toMap();
    QVariantList lists = list["list"].toList();

    if(lists.size() > 0)
    {

        foreach(QVariant t, lists)
        {
            QString name = t.toMap().value("name").toString();
            QString id = t.toMap().value("id").toString();

            if(!reservedList(name))
            {
                ToDoList* list = new ToDoList(name);
                list->setListID(id);
                remoteLists.append(list);

            }
        }

    }

    reply->close();
    if(!listExists(todoList->listName()) || todoList->status()== ToDoList::NEW)
        createToDoList();

    else if(todoList->status() == ToDoList::UPDATED)
        updateList();

    else
    {
        qDebug() << "Getting Tasks";
        getTasks();
    }
}

bool
RememberTheMilk::reservedList(QString name)
{
    QStringList reserved;
    reserved << "Inbox"  << "Sent" << "All Tasks" << "Work";

    return(reserved.contains(name));
}

bool
RememberTheMilk::listExists(QString name)
{
    bool exists = false;

    foreach(ToDoList* list, remoteLists)
        if(list->listName().compare(name) == 0)
            exists = true;


    return exists;
}

void
RememberTheMilk::createToDoList()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.lists.add");
    parameters.append(QString("name=%1").arg(todoList->listName()));
    parameters.append(QString("timeline=%1").arg(timeline));

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&%5&%6&api_sig=%7")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(parameters.at(4))
            .arg(parameters.at(5))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(todoListResponse()));
}

void
RememberTheMilk::todoListResponse()
{

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;
    QVariant initial = parser.parse(response.toAscii(),&ok);

    QVariant list = initial.toMap().value("rsp");
    QVariant list2 = list.toMap().value("list");
    QVariant list3 = list2.toMap().value("id");
    todoList->setListID(list3.toString());
    insertListID(todoList->listName(),todoList->listID());

    emit readyToAddItems();
}

void
RememberTheMilk::updateList()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.lists.setName");
    parameters.append(QString("list_id=%1").arg(todoList->listID()));
    parameters.append(QString("name=%1").arg(todoList->listName()));
    parameters.append(QString("timeline=%1").arg(timeline));

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&%5&%6&%7&api_sig=%8")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(parameters.at(4))
            .arg(parameters.at(5))
            .arg(parameters.at(6))
            .arg(api_sig);
    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(updateToDoListResponse()));

}

void
RememberTheMilk::updateToDoListResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;
    QVariant initial = parser.parse(response.toAscii(),&ok);
    QVariant inner = initial.toMap().value("rsp");
    QVariant stat = inner.toMap().value("stat");
    QVariant error = inner.toMap().value("error");

    if(stat.toString().compare("fail") == 0)
        emit serviceError("Failed to update ToDoList " + error.toString() );

    getTasks();
}

void RememberTheMilk::deleteList()
{
    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.lists.delete");
    parameters.append(QString("list_id=%1").arg(todoList->listID()));;
    parameters.append(QString("timeline=%1").arg(timeline));

    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&%5&%6&api_sig=%7")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(parameters.at(4))
            .arg(parameters.at(5))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(deleteListResponse()));
}


void
RememberTheMilk::deleteListResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;
    QVariant initial = parser.parse(response.toAscii(),&ok);
    QVariant inner = initial.toMap().value("rsp");
    QVariant stat = inner.toMap().value("stat");
    QVariant error = inner.toMap().value("error");

    if(stat.toString().compare("fail") == 0)
        emit serviceError("Failed to delete ToDoList " + error.toString() );

}

void
RememberTheMilk::getTasks()
{

    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.tasks.getList");
    parameters.append(QString("list_id=%1").arg(todoList->listID()));
    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&%5&api_sig=%6")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(parameters.at(4))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(getTasksResponse()));

}

void
RememberTheMilk::getTasksResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(reply->error())
    {

        emit serviceError(reply->errorString());
        reply->close();

    }

    QString response = reply->readAll();
    QJson::Parser parser;
    bool ok;
    QVariantMap initial = parser.parse(response.toAscii(),&ok).toMap();
    QVariantMap rsp = initial["rsp"].toMap();
    QVariant stat = initial["stat"];
    if(stat.toString().compare("fail") == 0)
        emit serviceError("An unkown error has occured " + response);
    QVariantMap task = rsp["tasks"].toMap();
    QVariantMap list = task["list"].toMap();
    QVariant series = list["taskseries"];

    QVariantList tasks = series.toList();
    if(tasks.length() < 0)
        tasks.append(series);

    if(tasks.length() > 0)
    {
        QVariant task = series.toMap();

        foreach(QVariant t, tasks)
        {
            QStringList name = t.toMap().value("name").toString().split(" : ",QString::SkipEmptyParts);
            ToDoItem* item = new ToDoItem(todoList->listID(),"",name.at(0),name.at(1),t.toMap().value("due").toString()
                                          ,t.toMap().value("modified").toString(),
                                          t.toMap().value("completed").toBool());
            QVariant taskid = t.toMap().value("task");

            item->setTaskID(taskid.toMap().value("id").toString());
            item->setTaskSeries(t.toMap().value("id").toString());
            syncTasks.append(item);
        }

    }
    reply->close();

    emit readyToAddItems();

}

void
RememberTheMilk::addTask(ToDoItem* item)
{

    QStringList parameters;
    parameters.append(QString("api_key=%1").arg(apiKey));
    parameters.append(QString("auth_token=%1").arg(authToken));
    parameters.append("format=json");
    parameters.append("method=rtm.tasks.add");
    parameters.append(QString("name=%1").arg(item->name() + " " + item->date().replace("/"," ")));
    parameters.append(QString("list_id=%1").arg(todoList->listID()));
    parameters.append(QString("timeline=%1").arg(timeline));
    parameters.append("parse=1");
    QString api_sig = generateSig(parameters);

    QString query = QString("%1&%2&%3&%4&%5&%6&%7&%8&api_sig=%9")
            .arg(parameters.at(0))
            .arg(parameters.at(1))
            .arg(parameters.at(2))
            .arg(parameters.at(3))
            .arg(parameters.at(4))
            .arg(parameters.at(5))
            .arg(parameters.at(6))
            .arg(parameters.at(7))
            .arg(api_sig);

    QNetworkRequest req = QNetworkRequest(QUrl(mURL));
    QNetworkReply* res = manager->post(req,query.toAscii());
    connect(res,SIGNAL(finished()),this,SLOT(addTaskResponse()));
}

void
RememberTheMilk::addTaskResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(reply->error())
    {

        emit serviceError(reply->errorString());
        reply->close();

    }

    QString response = reply->readAll();
    QJson::Parser parser;
    bool ok;
    QVariantMap initial = parser.parse(response.toAscii(),&ok).toMap();
    QVariantMap rsp = initial["rsp"].toMap();
    QVariant stat = rsp["stat"];
    qDebug() << response;
    if(stat.toString().compare("fail") == 0)
        emit serviceError("Task could not be added");

    else
    {
        QVariantMap list = rsp["list"].toMap();
        QVariantMap taskseries = list["taskseries"].toMap();
        QVariantMap task = taskseries["task"].toMap();
        QVariant id = task["id"];

    }

    reply->close();
}


// TODO: Connect this to serviceTasks table so we can keep a track of service task ids.
void
RememberTheMilk::insertTaskID(ToDoItem *item,QString itemID)
{
    Q_UNUSED(item);
    Q_UNUSED(itemID);

}

void
RememberTheMilk::deleteTask(ToDoItem* item)
{
    QStringList taskID = getTaskID(item->name());
    qDebug() << item->name();
    qDebug() << syncTasks.length();

    if(taskID.length() > 0)
    {
        if(taskID.at(0).compare("") != 0 && taskID.at(1).compare("") != 0)
        {

            QStringList parameters;
            parameters.append(QString("api_key=%1").arg(apiKey));
            parameters.append(QString("auth_token=%1").arg(authToken));
            parameters.append(QString("timeline=%1").arg(timeline));
            parameters.append("format=json");
            parameters.append("method=rtm.tasks.delete");
            parameters.append(QString("list_id=%1").arg(todoList->listID()));
            parameters.append(QString("task_id=%1").arg(taskID.at(0)));
            parameters.append(QString("taskseries_id=%1").arg(taskID.at(1)));
            QString api_sig = generateSig(parameters);

            QString query = QString("%1&%2&%3&%4&%5&%6&%7&%8&api_sig=%9")
                    .arg(parameters.at(0))
                    .arg(parameters.at(1))
                    .arg(parameters.at(2))
                    .arg(parameters.at(3))
                    .arg(parameters.at(4))
                    .arg(parameters.at(5))
                    .arg(parameters.at(6))
                    .arg(parameters.at(7))
                    .arg(api_sig);

            QNetworkRequest req = QNetworkRequest(QUrl(mURL));
            QNetworkReply* res = manager->post(req,query.toAscii());
            connect(res,SIGNAL(finished()),this,SLOT(deleteTaskResponse()));
        }
    }
    else
        qDebug() << "Task doesn't exist on remote service";

}

void
RememberTheMilk::deleteTaskResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(reply->error())
    {

        emit serviceError(reply->errorString());
        reply->close();

    }

    QString response = reply->readAll();
    reply->close();
    QJson::Parser parser;
    bool ok;

    QVariant initial = parser.parse(response.toAscii(),&ok);
    QVariant rsp = initial.toMap().value("rsp");
    QVariant stat = rsp.toMap().value("stat");

    if(stat.toString().compare("fail")==0)
        emit serviceError("Failed to delete task");


}

void
RememberTheMilk::updateCompleted(ToDoItem* item)
{

    QStringList taskID = getTaskID(item->name());

    if(taskID.length() > 0)
    {

        if(taskID.at(0).compare("") != 0 && taskID.at(1).compare("") != 0)
        {
            QStringList parameters;
            parameters.append(QString("api_key=%1").arg(apiKey));
            parameters.append(QString("auth_token=%1").arg(authToken));
            parameters.append("format=json");


            if(item->isCompleted())
                parameters.append("method=rtm.tasks.complete");
            else
                parameters.append("method=rtm.tasks.uncomplete");

            parameters.append(QString("task_id=%1").arg(taskID.at(0)));
            parameters.append(QString("taskseries_id=%1").arg(taskID.at(1)));
            parameters.append(QString("list_id=%1").arg(todoList->listID()));
            parameters.append(QString("timeline=%1").arg(timeline));

            QString api_sig = generateSig(parameters);

            QString query = QString("%1&%2&%3&%4&%5&%6&%7&%8&api_sig=%9")
                    .arg(parameters.at(0))
                    .arg(parameters.at(1))
                    .arg(parameters.at(2))
                    .arg(parameters.at(3))
                    .arg(parameters.at(4))
                    .arg(parameters.at(5))
                    .arg(parameters.at(6))
                    .arg(parameters.at(7))
                    .arg(api_sig);

            QNetworkRequest req = QNetworkRequest(QUrl(mURL));
            QNetworkReply* res = manager->post(req,query.toAscii());
            connect(res,SIGNAL(finished()),this,SLOT(updateDateResponse()));
        }
    }
    else
        addTask(item);

}
void
RememberTheMilk::updateCompletedResponse()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());

    if(reply->error())
        emit serviceError(reply->errorString());

    QString response = reply->readAll();
    reply->close();

    QJson::Parser parser;
    bool ok;

    QVariant initial = parser.parse(response.toAscii(),&ok);
    QVariant rsp = initial.toMap().value("rsp");
    QVariant stat = rsp.toMap().value("stat");

    if(stat.toString().compare("fail"))
        emit serviceError("Failed to update completed status");



}

void
RememberTheMilk::updateDate(ToDoItem* item)
{

    QStringList taskID = getTaskID(item->name());
    QDate date = QDate::fromString(item->date(),"dd/MMM/yyyy");

    if(taskID.length() > 0)
    {
        if(taskID.at(0).compare("") != 0 && taskID.at(1).compare("") != 0)
        {
            QStringList parameters;
            parameters.append(QString("api_key=%1").arg(apiKey));
            parameters.append(QString("auth_token=%1").arg(authToken));
            parameters.append("format=json");
            parameters.append("method=rtm.tasks.setDueDate");
            parameters.append(QString("due=%1").arg(date.toString("yyyy-MM-dd")));
            parameters.append(QString("task_id=%1").arg(taskID.at(0)));
            parameters.append(QString("taskseries_id=%1").arg(taskID.at(1)));
            parameters.append(QString("list_id=%1").arg(todoList->listID()));
            parameters.append(QString("timeline=%1").arg(timeline));

            QString api_sig = generateSig(parameters);

            QString query = QString("%1&%2&%3&%4&%5&%6&%7&%8&%9&api_sig=%10")
                    .arg(parameters.at(0))
                    .arg(parameters.at(1))
                    .arg(parameters.at(2))
                    .arg(parameters.at(3))
                    .arg(parameters.at(4))
                    .arg(parameters.at(5))
                    .arg(parameters.at(6))
                    .arg(parameters.at(7))
                    .arg(parameters.at(8))
                    .arg(api_sig);
            QNetworkRequest req = QNetworkRequest(QUrl(mURL));
            QNetworkReply* res = manager->post(req,query.toAscii());
            connect(res,SIGNAL(finished()),this,SLOT(updateDateResponse()));
        }
    }
    else
        addTask(item);


}
void
RememberTheMilk::updateDateResponse()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());

    if(reply->error())
        emit serviceError(reply->errorString());

    QString response = reply->readAll();
    reply->close();

    QJson::Parser parser;
    bool ok;
    QVariant initial = parser.parse(response.toAscii(),&ok);
    QVariant rsp = initial.toMap().value("rsp");
    QVariant stat = rsp.toMap().value("stat");
    QVariant err = rsp.toMap().value("err");
    if(stat.toString().compare("fail") == 0)
        emit serviceError("Failed to update date because: " + err.toString());



}


// Utility functions
QStringList
RememberTheMilk::getTaskID(QString taskName)
{
    QStringList taskid;
    foreach(ToDoItem* t, syncTasks)
    {
        qDebug() << "Tasks" << t->name();
        if(taskName.compare(t->name()) == 0)
        {

            taskid.append(t->taskID());
            taskid.append(t->taskSeries());
        }
    }

    return taskid;
}


bool
RememberTheMilk::compareTaskDates(QString task1, QString task2)
{
    Q_UNUSED(task1);
    Q_UNUSED(task2);

    return false;
}



void
RememberTheMilk::insertListID(QString listName, QString listID)
{

    QSqlQuery query("UPDATE todolist SET list_id = :listID WHERE name = :listName");
    query.bindValue(":listid",listID);
    query.bindValue(":listName",listName);

    if(!query.exec())
    {
        qDebug() << "Error on Query";
        qDebug() << query.lastError() << " Bound Values: " << query.boundValues();
        qDebug() << query.lastQuery();

    }

    todoList->setListID(listID);

}
/*
 * This is for Remember the Milks API signature, which is each parameter sorted alphabetically,
 * concatenated with the secret and md5 hashed.
 */
QString
RememberTheMilk::generateSig(QStringList parameters)
{
    parameters.sort();
    QString sig;

    foreach(QString q , parameters)
    {
        q.remove("=");
        sig.append(q);
    }

    sig = secret + sig;
    QByteArray hash;
    hash = QCryptographicHash::hash(sig.toUtf8(),QCryptographicHash::Md5);
    return hash.toHex();
}
