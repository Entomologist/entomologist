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
#include <QVariantMap>
#include <QSqlQuery>
#include <QSqlError>
#include <qjson/json_parser.hh>
#include <qjson/serializer.h>
#include "GoogleTasks.h"
#include "ServicesBackend.h"

GoogleTasks::GoogleTasks(QString service,bool state) : mService(service), mState(state)
{
    serviceType_ = "Google Tasks";

    mApiUrl = "https://accounts.google.com/o/oauth2/";
    mScope = "https://www.googleapis.com/auth/tasks";
    mClientID = "458756674768.apps.googleusercontent.com";
    mRedirectURL = "urn:ietf:wg:oauth:2.0:oob";
    pManager = new QNetworkAccessManager(this);
    mSecret = "paPO2oW-YwsxggNkHZfH2AFs";

    mUrl = mApiUrl + "auth?" + "client_id=" + mClientID + "&redirect_uri=" + mRedirectURL + "&scope=" + mScope + "&response_type=code";


}

GoogleTasks::~GoogleTasks()
{
    delete(pManager);
}

// this SHOULD be empty but Google wants us to open a browser window.
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
        qDebug() << query.value(1).toString();
    }

    if(authToken.compare("") != 0)
    {

        mToken = authToken;
        mRefreshToken = refreshToken;
        mGrantType = "refresh_token";
        newUser();

    }
    else
    {
        QString url = mUrl;
        mState = false;
        QDesktopServices::openUrl(QUrl(url));
        mGrantType="authorization_code";
        emit loginWaiting();

    }



}



void
GoogleTasks::newUser()
{
    // This has to swap our token for a new one so we can actually access the API.
    QString swapURL = mApiUrl + "token";
    QNetworkRequest req = QNetworkRequest((QUrl(swapURL)));
    QString query ;
    if(mRefreshToken.isEmpty())
        query = "client_id=" + mClientID + "&client_secret=" + mSecret + "&code=" + mToken
                + "&redirect_uri=" + mRedirectURL + "&grant_type=authorization_code";
    else
        query = "client_id=" + mClientID + "&client_secret=" + mSecret + "&refresh_token=" + mRefreshToken + "&grant_type=refresh_token";

    QNetworkReply* rep = pManager->post(req,query.toAscii());
    connect(rep,SIGNAL(finished()),this,SLOT(getSwapToken()));

}

void
GoogleTasks::getSwapToken()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    qDebug() << response;
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
            }
        }
        emit authCompleted();
    }
    else
        emit serviceError(parser.errorString());


}

// If Our token expires we need to get a new one.
void
GoogleTasks::reRequestToken()
{
    if(!mRefreshToken.isEmpty() && !mAccessToken.isEmpty())
    {
        QNetworkRequest req = QNetworkRequest(QUrl( mApiUrl + "token"));
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
    qDebug() << "gettingLists";
}

void
GoogleTasks::getLists()
{

    QString url = "https://www.googleapis.com/tasks/v1/lists?pp=1&key=" + mAccessToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply* rep = pManager->get(req);
    connect(rep,SIGNAL(finished()),this,SLOT(getListRepsonse()));

}

void
GoogleTasks::getListRepsonse()
{

    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    qDebug() << rep->readAll();
}


void
GoogleTasks::setList(ToDoList *list)
{
    QVariantMap title;
    title.insert("title:",list->listName());
    QJson::Serializer serial;
    QByteArray json = serial.serialize(title);

    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists?pp=1&key=" + mAccessToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply* rep = pManager->post(req,json);
    connect(rep,SIGNAL(finished()),this,SLOT(setListResponse()));

}

void
GoogleTasks::setListResponse()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
    qDebug() << reply->readAll();
}

void
GoogleTasks::deleteList()
{

    QString url = "https://www.googleapis.com/tasks/v1/users/@me/lists/?pp=1&key=" + mAccessToken;
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    QNetworkReply* rep = pManager->deleteResource(req);
    connect(rep,SIGNAL(finished()),this,SLOT(deleteListResponse()));
}

void
GoogleTasks::deleteListResponse()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
    qDebug() << reply->readAll();

}

void
GoogleTasks::addTask(ToDoItem* item)
{


    QString url = "https://www.googleapis.com/tasks/v1/lists/";
    QString finishedURL = url + "" + "?pp-1&key=" + mAccessToken;

    QNetworkRequest req = QNetworkRequest(QUrl(url));
    //   QNetworkReply* rep = pManager->post(req,);
    //   connect(rep,SIGNAL(finished()),this,SLOT(deleteLater()));
}

QString
GoogleTasks::getTaskIDFromDB(const QString &taskID, const QString &serviceName)
{
    QString itemID;
    QSqlQuery query("SELECT item_id FROM service_tasks WHERE task_id = :taskName AND service_name = :serviceName");
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

void
GoogleTasks::deleteTask(ToDoItem* item)
{
    Q_UNUSED(item);
}




void
GoogleTasks::insertKey(QString token, QString item)
{
    QSqlQuery query;
    if(item.compare("auth_key") == 0)
        query.prepare("UPDATE services SET auth_key=:key WHERE name=:name");
    else
    {
        qDebug() << "Blah blah blah";
        query.prepare("UPDATE services SET refresh_token=:key WHERE name=:name");
    }

    query.bindValue(":key",token);
    query.bindValue(":name",mService);
    query.exec();
    qDebug() << query.lastError();
    emit authCompleted(); // seems to be the only place where we can send this.


}
void
GoogleTasks::updateCompleted(ToDoItem* item)
{
    Q_UNUSED(item);
}

void
GoogleTasks::updateDate(ToDoItem* item)
{
    Q_UNUSED(item);
}
