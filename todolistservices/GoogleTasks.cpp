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
    if(!mState)
    {
        QString authToken;
        QSqlQuery query("SELECT auth_key FROM services WHERE name=:name");
        query.bindValue(":name",mService);

        query.exec();

        while(query.next())
        {
            authToken  = query.value(0).toString();
        }

        if(!authToken.isEmpty())
        {
            mToken = authToken;
            emit authCompleted();
        }
        else
        {

            QString url = mUrl;
            mState = false;
            QDesktopServices::openUrl(QUrl(url));
            emit loginWaiting();

        }
    }
}

void
GoogleTasks::setupList()
{

}

void
GoogleTasks::addTask(ToDoItem* item)
{
    Q_UNUSED(item);
}


void
GoogleTasks::deleteTask(ToDoItem* item)
{
    Q_UNUSED(item);
}



void
GoogleTasks::newUser()
{
    // This has to swap our token for a new one so we can actually access the API.
    QString swapURL = mApiUrl + "token";
    QNetworkRequest req = QNetworkRequest((QUrl(swapURL)));
    QString query = "client_id=" + mClientID + "&client_secret=" + mSecret + "&code=" + mToken
            + "&redirect_uri=" + mRedirectURL + "&grant_type=authorization_code";
    QNetworkReply* rep = pManager->post(req,query.toAscii());
    connect(rep,SIGNAL(finished()),this,SLOT(getSwapToken()));

}

void
GoogleTasks::getSwapToken()
{
    QNetworkReply* rep = static_cast<QNetworkReply*>(sender());
    QString response = rep->readAll();
    bool ok;
    QJson::Parser parser;

    QVariantMap json = parser.parse(response.toAscii(),&ok).toMap();

    if(ok)
    {
        if(json.contains("error"))
        {
            qDebug() << mToken;
            reRequestToken();
        }
        mAccessToken = json["access_token"].toString();
        mRefreshToken = json["refresh_token"].toString();
        insertKey(mRefreshToken,"refresh_token");
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
GoogleTasks::insertKey(QString token, QString item)
{
    QSqlQuery query;
    if(item.compare("auth_key") == 0)
        query.prepare("UPDATE services SET auth_key=:key WHERE name=:name");
    else
        query.prepare("UPDATE services SET refresh_token=:key WHERE name=:name");


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
