# -------------------------------------------------
# Project created by QtCreator 2011-01-20T11:51:19
# -------------------------------------------------
QT += network \
    sql \
    xml
TARGET = Entomologist
TEMPLATE = app
TRANSLATIONS = entomologist_en.ts
VERSION = 0.2
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
SOURCES += main.cpp \
    MainWindow.cpp \
    libmaia/maiaXmlRpcServerConnection.cpp \
    libmaia/maiaXmlRpcServer.cpp \
    libmaia/maiaXmlRpcClient.cpp \
    libmaia/maiaObject.cpp \
    libmaia/maiaFault.cpp \
    Backend.cpp \
    Bugzilla.cpp \
    NewTracker.cpp \
    NovellBugzilla.cpp \
    Autodetector.cpp \
    SqlBugModel.cpp \
    SqlBugDelegate.cpp \
    CommentFrame.cpp \
    UploadDialog.cpp \
    Preferences.cpp \
    About.cpp \
    SqlWriterThread.cpp \
    SqlWriter.cpp \
    PlaceholderLineEdit.cpp
HEADERS += MainWindow.h \
    libmaia/maiaXmlRpcServerConnection.h \
    libmaia/maiaXmlRpcServer.h \
    libmaia/maiaXmlRpcClient.h \
    libmaia/maiaObject.h \
    libmaia/maiaFault.h \
    Backend.h \
    Bugzilla.h \
    NewTracker.h \
    NovellBugzilla.h \
    Autodetector.h \
    SqlBugModel.h \
    SqlBugDelegate.h \
    CommentFrame.h \
    UploadDialog.h \
    Preferences.h \
    About.h \
    SqlWriterThread.h \
    SqlWriter.h \
    PlaceholderLineEdit.h
FORMS += MainWindow.ui \
    NewTracker.ui \
    CommentFrame.ui \
    UploadDialog.ui \
    Preferences.ui \
    About.ui
RESOURCES += resources.qrc
