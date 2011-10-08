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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDate>

#include <qjson/json_parser.hh>
#include <qjson/serializer.h>
#include "GoogleTasks.h"
#include "ServicesBackend.h"

GoogleTasks::GoogleTasks(QString service) : mService(service)
{

    mServiceType = "Google Tasks";

    mAuthURL = "https://accounts.google.com/o/oauth2/";
    mScope = "https://www.googleapis.com/auth/tasks";
    mClientID = "458756674768.apps.googleusercontent.com";
    mRedirectURL = "urn:ietf:wg:oauth:2.0:oob";
    mSecret = "paPO2oW-YwsxggNkHZfH2AFs";

    mRFC3339DateFormat = "yyyy-MM-dd";
    mRFC3339TimeFormat = "Thh:mm:ss.zzzZ";
    mDateFormat = "dd/MMM/yyyy"; // This is our internal date format.
    mUrl = mAuthURL
            + "auth?"
            + "client_id="
            + mClientID
            + "&redirect_uri="
            + mRedirectURL
            + "&scope="
            + mScope
            + "&response_type=code";

}
GoogleTasks::~GoogleTasks()
{

}

void
GoogleTasks::login()
{

    QString authToken,refreshToken;
    QSqlQuery query("SELECT auth_key,refresh_token FROM services WHERE name=:name");
    query.bindValue(":name",mService);

    query.exec();

    while(query.next())
    {

        authToken  = query.value(0).toString();
        refreshToken = query.value(1).toString();

    }

    if(authToken.compare("") != 0)
    {
        mToken = authToken;
        mRefreshToken = refreshToken;
        newUser();

    }
    else
    {

        QString url = mUrl;
        QDesktopServices::openUrl(QUrl(url));
        emit loginWaiting();

    }

}

void
GoogleTasks::newUser()
{
    // This has to swap our token for a new one so we can actually access the API.
    QString swapURL = mAuthURL + "token";
    QNetworkRequest req = QNetworkRequest((QUrl(swapURL)));
    QString query;
    if(mRefreshToken.isEmpty())
        query = "client_id="
                + mClientID
                + "&client_secret="
                + mSecret + "&code="
                + mToken
                + "&redirect_uri="
                + mRedirectURL
                + "&grant_type=authorization_code";
    else
        query = "client_id="
                + mClientID
                + "&client_secret="
                + mSecret + "&refresh_token="
                + mRefreshToken
                + "&grant_type=refresh_token";

    QNetworkReply* rep = pManager->post(req,query.toAscii());
    connect(rep,SIGNAL(finished()),this,SLOT(getSwapToken()));

}

void
GoogleTasks::getSwapToken()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    rep->deleteLater();
    bool ok;
    QJson::Parser parser;
    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();

    if(ok)
    {
        if(json.contains("error"))
        {
            reRequestToken();
        }

        mAccessToken = json["access_token"].toString();
        QString refreshToken = json["refresh_token"].toString();

        if(mRefreshToken.compare(refreshToken) != 0)
        {
            if(refreshToken.compare("") != 0)
            {
                insertKey(refreshToken,"refresh_token");
                mRefreshToken = refreshToken;
                emit regSuccess();
            }
        }

        emit authCompleted();

    }
    else
        emit serviceError(parser.errorString());


}

void
GoogleTasks::reRequestToken()
{

    if(!mRefreshToken.isEmpty() && !mAccessToken.isEmpty())
    {
        QNetworkRequest req = QNetworkRequest(QUrl( mAuthURL + "token"));
        QString query = "client_id=" + mClientID + "&client_secret=" + mSecret
                + "&refresh_token=" + mRefreshToken + "&grant_type=refresh_token";
        QNetworkReply* rep = pManager->post(req,query.toAscii());
        connect(rep,SIGNAL(finished()),this,SLOT(getSwapToken()));
    }

}

void
GoogleTasks::setupList()
{

    getLists();
}

void
GoogleTasks::getLists()
{

    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists?pp=1&access_token="
            + mAccessToken
            + "&key="
            + this->mToken;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply* rep = pManager->get(req);
    connect(rep,SIGNAL(finished()),this,SLOT(getListRepsonse()));


}

void
GoogleTasks::getListRepsonse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    rep->deleteLater();
    bool ok;
    QJson::Parser parser;
    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();
    if(ok)
    {
        if(!json.contains("error"))
        {
            QVariantList items = json["items"].toList();

            foreach(QVariant i, items)
            {
                QVariantMap item = i.toMap();
                ToDoList* newItem = new ToDoList(item["title"].toString());
                newItem->setGoogleTasksListID(item["id"].toString());
                remoteLists.append(newItem);
                mRemoteListsAsJSON.insert(item["id"].toString(),i);

            }
            if(!listExists(mTodoList->googleTasksListID()) || mTodoList->status() == ToDoList::NEW)
                addList();

            else if(mTodoList->status() == ToDoList::UPDATED)
                updateList();

            else
                getTasksList();
        }
        else
            emit serviceError("Error getting lists: " + response);

    }

    else
        emit serviceError("Could not get Lists: " + parser.errorString());

}

void
GoogleTasks::setList(ToDoList *list)
{

    mTodoList = list;

}

void
GoogleTasks::addList()
{
    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists?pp=1&access_token="
            + mAccessToken
            + "&key="
            + this->mToken;

    QVariantMap title;
    title.insert("title",mTodoList->listName());

    QJson::Serializer serializer;
    QByteArray json = serializer.serialize(title);

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* rep = pManager->post(req,json);
    connect(rep,SIGNAL(finished()),this,SLOT(addListResponse()));

}

void
GoogleTasks::addListResponse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();

    rep->deleteLater();

    if(response.contains("error") || rep->error())
        emit serviceError("Could not add list: " + response);
    bool ok;
    QJson::Parser parser;
    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();

    if(ok)
    {
        if(!json.contains("error"))
        {

            QString listID = json["id"].toString();
            insertListID(mTodoList->listName(),listID);
            getTasksList();

        }
        else
            emit serviceError("Could not add List:  " + response);
    }

    else
        emit serviceError("Could not add List: " + parser.errorString());
}

void
GoogleTasks::deleteList()
{

    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists/" + mTodoList->googleTasksListID() + "?pp=1&key=" + mToken + "&access_token=" + mAccessToken;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply* rep = pManager->deleteResource(req);
    connect(rep,SIGNAL(finished()),this,SLOT(deleteListResponse()));

}

void
GoogleTasks::deleteListResponse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());

    QString reply = rep->readAll();
    rep->deleteLater();

    if(!reply.contains("error") || rep->error())
        emit serviceError("Could not delete list: " + reply);

}

void
GoogleTasks::updateList()
{
    QVariantMap listJSON = mRemoteListsAsJSON.value(mTodoList->googleTasksListID()).toMap();
    listJSON["title"] = mTodoList->listName();
    QJson::Serializer serializer;
    QByteArray json = serializer.serialize(listJSON);

    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists/" + mTodoList->googleTasksListID() + "?pp=1&key=" + mToken + "&access_token=" + mAccessToken;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* rep = pManager->put(req,json);
    connect(rep,SIGNAL(finished()),this,SLOT(updateListResponse()));
}

void
GoogleTasks::updateListResponse()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());

    QString reply = rep->readAll();
    rep->deleteLater();
    if(!reply.contains("error") || rep->error())
        emit serviceError("Failed to update list" + reply);
}

void
GoogleTasks::getTasksList()
{

    QString url = "https://www.googleapis.com/tasks/v1/lists/" + mTodoList->googleTasksListID()
            + "/tasks?pp=1" + "&access_token=" + mAccessToken + "&key=" + mToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* rep = pManager->get(req);
    connect(rep,SIGNAL(finished()),this,SLOT(getTasksListResponse()));
}

void
GoogleTasks::getTasksListResponse()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    rep->deleteLater();
    if(response.contains("error") || rep->error())
        emit serviceError("Could Not get tasks: " + response);
    bool ok;
    QJson::Parser parser;
    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();
    if(ok)
    {
        QVariantList items = json["items"].toList();

        foreach(QVariant i, items)
        {
            QVariantMap temp = i.toMap();

            mTasksList.insert(temp["id"].toString(),i);
        }
        rep->close();
        emit readyToAddItems();
    }
    else
        emit serviceError("Could not get tasks");
}

void
GoogleTasks::addTask(ToDoItem* item)
{
    mCurrentItem = item;
    insertItem(item->bugID(),mService);
    QVariantMap itemJSON;
    itemJSON.insert("due",QDateTime::fromString(item->date(),mDateFormat).toString(mRFC3339DateFormat)
                    + QTime::currentTime().toString(mRFC3339TimeFormat));

    itemJSON.insert("title",item->name());
    QJson::Serializer serializer;
    QByteArray json = serializer.serialize(itemJSON);

    QString url = "https://www.googleapis.com/tasks/v1/lists/" + mTodoList->googleTasksListID()
            + "/tasks?pp=1" + "&access_token=" + mAccessToken + "&key=" + mToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* rep = pManager->post(req,json);
    connect(rep,SIGNAL(finished()),this,SLOT(addTaskResponse()));
}

void
GoogleTasks::addTaskResponse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    rep->deleteLater();
    if(response.contains("error") || rep->error())
        emit serviceError("Could not add task: " + response);
    bool ok;
    QJson::Parser parser;
    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();

    if(ok)
    {
        QString itemID = json["id"].toString();
        mCurrentItem->setGoogleTasksID(itemID);
        updateItemID(mCurrentItem,mService);
    }

}

void
GoogleTasks::deleteTask(ToDoItem* item)
{
    QString itemID = getTaskIDFromDB(item->bugID(),mService);

    mCurrentItem = item;

    QString url = "https://www.googleapis.com/tasks/v1/lists/" + mTodoList->googleTasksListID()
            + "/tasks/" + itemID + "?pp=1" + "&access_token=" + mAccessToken + "&key=" + mToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));

    QNetworkReply* rep = pManager->deleteResource(req);
    connect(rep,SIGNAL(finished()),this,SLOT(deleteTaskResponse()));
}

void
GoogleTasks::deleteTaskResponse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    if(response.contains("error") || rep->error())
        emit serviceError("failed to delete task:" + response);
    rep->deleteLater();

}

void
GoogleTasks::updateCompleted(ToDoItem* item)
{
    QString itemID = getTaskIDFromDB(item->bugID(),mService);

    if(itemID.compare("") == 0)
        addTask(item);

    else
    {
        QString status;
        QVariantMap currentTask = mTasksList.value(itemID).toMap();

        if(item->isCompleted())
        {
            status = "completed";
            currentTask["completed"] = QDateTime::currentDateTime().toString(mRFC3339DateFormat + mRFC3339TimeFormat);
        }
        else
        {
            status = "needsAction";
            currentTask.remove("completed");
        }

        mCurrentItem = item;

        currentTask["status"] = status;
        QJson::Serializer serializer;
        QByteArray json = serializer.serialize(currentTask);
        QString url = "https://www.googleapis.com/tasks/v1/lists/" + mTodoList->googleTasksListID()
                + "/tasks/" + itemID + "?pp=1" + "&access_token=" + mAccessToken + "&key=" + mToken;
        QNetworkRequest req = QNetworkRequest(QUrl(url));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* rep = pManager->put(req,json);
        connect(rep,SIGNAL(finished()),this,SLOT(updateCompletedResponse()));



    }

}

void
GoogleTasks::updateCompletedResponse()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());

    QString response = rep->readAll();

    if(response.contains("error") || rep->error())
        emit serviceError("failed to update task completed:" + response);
    rep->deleteLater();
}

void
GoogleTasks::updateDate(ToDoItem* item)
{
    QString itemID = getTaskIDFromDB(item->bugID(),mService);

    if(itemID.compare("") == 0)
        addTask(item);

    else
    {
        QVariantMap currentTask = mTasksList.value(itemID).toMap();
        currentTask.remove("completed");
        currentTask["due"] = QDate::fromString(item->date(),mDateFormat).toString(mRFC3339DateFormat)
                + QTime::currentTime().toString(mRFC3339TimeFormat);
        mCurrentItem = item;


        QJson::Serializer serializer;
        QByteArray json = serializer.serialize(currentTask);
        QString url = "https://www.googleapis.com/tasks/v1/lists/" + mTodoList->googleTasksListID()
                + "/tasks/" + itemID + "?pp=1" + "&access_token=" + mAccessToken + "&key=" + mToken;
        QNetworkRequest req = QNetworkRequest(QUrl(url));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* rep = pManager->put(req,json);
        connect(rep,SIGNAL(finished()),this,SLOT(updateDateResponse()));
    }
}

void
GoogleTasks::updateDateResponse()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    rep->deleteLater();
    if(response.contains("error") || rep->error())
        emit serviceError("Could not update date:" + response);

}

void
GoogleTasks::insertListID(const QString &listName, const QString &listID)
{

    QSqlQuery query("UPDATE todolist SET google_listid = :listID WHERE name = :listName");
    query.bindValue(":listid",listID);
    query.bindValue(":listName",listName);

    if(!query.exec())
    {
        qDebug() << "Error on Query";
        qDebug() << query.lastError() << " Bound Values: " << query.boundValues();
        qDebug() << query.lastQuery();

    }

    mTodoList->setGoogleTasksListID(listID);

}

void
GoogleTasks::insertKey(const QString &token, const QString &item)
{
    QSqlQuery query;
    if(item.compare("auth_key") == 0)
        query.prepare("UPDATE services SET auth_key=:key WHERE name=:name");
    else
    {
        query.prepare("UPDATE services SET refresh_token=:key WHERE name=:name");
    }

    query.bindValue(":key",token);
    query.bindValue(":name",mService);
    query.exec();


}

void
GoogleTasks::updateItemID(ToDoItem *item, const QString &serviceName)
{

    QSqlQuery query("UPDATE service_tasks SET item_id = :id WHERE service_name = :name AND task_id = :taskid");

    query.bindValue(":id",item->googleTaskID());
    query.bindValue(":name",serviceName);
    query.bindValue(":taskid",item->bugID());

    if(!query.exec())
    {

        qDebug() << "Error on Query";
        qDebug() << query.lastError() << " Bound Values: " << query.boundValues();
        qDebug() << query.lastQuery();

    }


}

QString
GoogleTasks::getTaskIDFromDB(const QString &taskID, const QString &serviceName)
{
    QString itemID;
    QSqlQuery query("SELECT item_id FROM service_tasks WHERE task_id = :taskName "
                    "AND service_name = :serviceName");
    query.bindValue(":taskName",taskID);
    query.bindValue(":serviceName",serviceName);

    if(!query.exec())
    {

        qDebug() << "Error on Query";
        qDebug() << query.lastError() << " Bound Values: " << query.boundValues();
        qDebug() << query.lastQuery();

    }

    while(query.next())
        itemID = query.value(0).toString();

    return itemID;
}

bool
GoogleTasks::listExists(const QString &listID)
{
    bool exists = false;

    foreach(ToDoList* list, remoteLists)
        if(list->googleTasksListID().compare(listID) == 0)
            exists = true;

    return exists;
}

