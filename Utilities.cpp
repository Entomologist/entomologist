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
 *  Author: Matt Barringer <mbarringer@suse.de>
 *
 */
#include <QUrl>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QVariant>
#include <QNetworkInterface>

#include "Utilities.hpp"
#ifdef Q_OS_ANDROID
#include <jni.h>
#endif

void
Utilities::openAndroidUrl(QUrl url)
{
    Q_UNUSED(url);
#ifdef Q_OS_ANDROID
    JNIEnv *env;
    JavaVM* m_javaVM = QPlatformNativeInterface::nativeResourceForWidget("JavaVM",0);

    if (m_javaVM->AttachCurrentThread(&env, NULL)<0)
    {
        qCritical()<<"AttachCurrentThread failed";
        return;
    }

    jclass applicationClass = env->GetObjectClass(objptr);
    if (applicationClass)
    {
        jmethodID openBrowserId = env->GetStaticMethodID(applicationClass,
                                            "openBrowser", "(Ljava/lang/String;)V");
        jstring path = env->NewStringUTF(url.toString().toLocal8Bit().constData());
        env->CallVoidMethod(applicationClass, openBrowserId, path);
        env->DeleteLocalRef(path);
    }
    m_javaVM->DetachCurrentThread();
#endif
}

bool
Utilities::isOnline(QNetworkAccessManager *manager)
{
    bool ret = false;
    QSettings settings("Entomologist");
    if (settings.value("work-offline", false).toBool())
        return(false);

#if QT_VERSION >= 0x040700
    if (manager->networkAccessible() != QNetworkAccessManager::NotAccessible)
        ret = true;
    else
        ret = false;
#else
    QNetworkInterface iface;
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    for (int i = 0; i < ifaces.count(); ++i)
    {
        iface = ifaces.at(i);
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack) )
        {
            if (iface.addressEntries().count() >= 1)
            {
                ret = true;
                break;
            }
        }
    }
#endif

    return(ret);
}

// The following code comes from the DiVinE project:

/***************************************************************************
 *   Copyright (C) 2009 by Martin Moracek                                  *
 *   xmoracek@fi.muni.cz                                                   *
 *                                                                         *
 *   DiVinE is free software, distributed under GNU GPL and BSD licences.  *
 *   Detailed licence texts may be found in the COPYING file in the        *
 *   distribution tarball. The tool is a product of the ParaDiSe           *
 *   Laboratory, Faculty of Informatics of Masaryk University.             *
 *                                                                         *
 *   This distribution includes freely redistributable third-party code.   *
 *   Please refer to AUTHORS and COPYING included in the distribution for  *
 *   copyright and licensing details.                                      *
 ***************************************************************************/


QString
Utilities::quoteString(const QString & src)
{
  QString res(src);

  res.replace("\"", "\\\"");

  res.prepend("\"");
  res.append("\"");
  return src;
}

QString
Utilities::unquoteString(const QString & src)
{
  QString res(src);

  res.remove(0, 1);
  res.remove(res.size() - 1, 1);

  res.replace("\\\"", "\"");
  return src;
}

QString
Utilities::prettyPrint(const QVariantMap & map)
{
  QString res;
  res.append("{\n");

  QVariantMap::const_iterator itr = map.begin();
  res.append(QString("%1=%2").arg(itr.key(), prettyPrint(itr.value())));

  for (++itr; itr != map.end(); ++itr) {
    res.append(QString("; %1=%2").arg(itr.key(), prettyPrint(itr.value())));
  }

  res.append("}\n");
  return res;
}

QString
Utilities::prettyPrint(const QVariantList & array)
{
  QString res;
  res.append("{\n");

  for (int i = 0; i < array.size(); ++i) {
    res.append(prettyPrint(array.at(i)));

    if (i < array.size() - 1)
      res.append(",");
  }

  res.append("}\n");
  return res;
}

QString
Utilities::prettyPrint(const QVariant & var)
{
  if (var.type() == QVariant::List) {
    return prettyPrint(var.toList());
  } else if (var.type() == QVariant::Map) {
    return prettyPrint(var.toMap());
  } else if (var.type() == QVariant::String) {
    return quoteString(var.toString());
  } else {
    return var.toString();
  }
}
