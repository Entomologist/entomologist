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
#include "Utilities.hpp"
#ifdef Q_OS_ANDROID
#include <jni.h>
#endif

void
Utilities::openAndroidUrl(QUrl url)
{
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
