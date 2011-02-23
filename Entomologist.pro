# -------------------------------------------------
# Project created by QtCreator 2011-01-20T11:51:19
# -------------------------------------------------
unix:isEmpty(PREFIX):PREFIX = /usr
QT += network \
    sql \
    xml
TARGET = entomologist
TEMPLATE = app
TRANSLATIONS = entomologist_en.ts
VERSION = 0.3
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


QMAKE_EXTRA_TARGETS += distfile
DISTFILE_MAKEDIR = .tmp/entomologist-$$VERSION
DISTFILE_EXTRAFILES = $$RESOURCES COPYING Entomologist.pro README INSTALL *.qm *.ts entomologist.desktop entomologist.spec graphics/ install_icons/ libmaia/
distfile.commands = mkdir -p $$DISTFILE_MAKEDIR && cp -r -f --parent $$SOURCES $$HEADERS $$FORMS $$DISTFILE_EXTRAFILES $$DISTFILE_MAKEDIR \
                    && cd .tmp && tar cvzf entomologist-$$VERSION\.tar.gz entomologist-$$VERSION && mv entomologist-$$VERSION\.tar.gz .. && cd ..

entomologist.path = $$PREFIX/bin
entomologist.files = entomologist

translations.path = $$PREFIX/share/entomologist
translations.files = *.qm

desktop.path = $$PREFIX/share/applications/
desktop.files += entomologist.desktop
icon32.path = $$PREFIX/share/icons/hicolor/32x32/apps
icon32.files += install_icons/32x32/entomologist.png
icon64.path = $$PREFIX/share/icons/hicolor/64x64/apps
icon64.files += install_icons/64x64/entomologist.png
icon128.path = $$PREFIX/share/icons/hicolor/128x128/apps
icon128.files += install_icons/128x128/entomologist.png

INSTALLS += entomologist \
            translations \
            icon32 \
            icon64 \
            icon128 \
            desktop

