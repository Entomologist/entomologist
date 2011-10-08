# -------------------------------------------------
# Project created by QtCreator 2011-01-20T11:51:19
# -------------------------------------------------
unix:isEmpty(PREFIX):PREFIX = /usr
QT += network \
    sql \
    xml
macx:ICON = graphics/entomologist.icns
win32:DEFINES += QJSON_MAKEDLL
win32:RC_FILE = Entomologist.rc
QMAKE_INFO_PLIST = Entomologist.plist
TARGET = entomologist
TEMPLATE = app
TRANSLATIONS = entomologist_en.ts
VERSION = 1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
isEmpty(LOCALE_PREFIX):LOCALE_PREFIX = $$PREFIX
DEFINES += LOCALE_PREFIX=\\\"$$LOCALE_PREFIX\\\"
SOURCES += main.cpp \
    MainWindow.cpp \
    libmaia/maiaXmlRpcServerConnection.cpp \
    libmaia/maiaXmlRpcServer.cpp \
    libmaia/maiaXmlRpcClient.cpp \
    libmaia/maiaObject.cpp \
    libmaia/maiaFault.cpp \
    trackers/Backend.cpp \
    trackers/Bugzilla.cpp \
    NewTracker.cpp \
    trackers/NovellBugzilla.cpp \
    Autodetector.cpp \
    SqlBugModel.cpp \
    SqlBugDelegate.cpp \
    CommentFrame.cpp \
    UploadDialog.cpp \
    Preferences.cpp \
    About.cpp \
    SqlWriterThread.cpp \
    PlaceholderLineEdit.cpp \
    qtsoap/qtsoap.cpp \
    trackers/Mantis.cpp \
    ChangelogWindow.cpp \
    ChangelogListDelegate.cpp \
    qjson/parserrunnable.cpp \
    qjson/parser.cpp \
    qjson/json_scanner.cpp \
    qjson/serializerrunnable.cpp \
    qjson/serializer.cpp \
    qjson/qobjecthelper.cpp \
    qjson/json_parser.cc \
    trackers/Trac.cpp \
    Utilities.cpp \ # qtmain_android.cpp \
    ErrorHandler.cpp \
    ErrorReport.cpp \
    qtsingleapplication/qtsinglecoreapplication.cpp \
    qtsingleapplication/qtsingleapplication.cpp \
    qtsingleapplication/qtlockedfile_win.cpp \
    qtsingleapplication/qtlockedfile_unix.cpp \
    qtsingleapplication/qtlockedfile.cpp \
    qtsingleapplication/qtlocalpeer.cpp \
    Translator.cpp \
    MonitorDialog.cpp \
    tracker_uis/BugzillaUI.cpp \
    tracker_uis/TracUI.cpp \
    tracker_uis/MantisUI.cpp \
    SqlUtilities.cpp \
    tracker_uis/BackendUI.cpp \
    NewCommentsDialog.cpp \
    tracker_uis/TracDetails.cpp \
    TrackerTabWidget.cpp \
    tracker_uis/MantisDetails.cpp \
    tracker_uis/BugzillaDetails.cpp \
    ClickableText.cpp \
    tracker_uis/BackendDetails.cpp \
    SearchTab.cpp \
    SqlSearchModel.cpp \
    ToDoListView.cpp \
    ToDoListServiceAdd.cpp \
    ToDoListPreferences.cpp \
    ToDoListExport.cpp \
    ToDoList.cpp \
    ToDoItem.cpp \
    BugTreeWidget.cpp \
    BugTreeItemDelegate.cpp \
    todolistservices/ServicesBackend.cpp \
    todolistservices/RememberTheMilk.cpp \
    todolistservices/GenericWebDav.cpp \
    TrackerTableView.cpp \
    SqlSearchDelegate.cpp \
    todolistservices/GoogleTasks.cpp
HEADERS += MainWindow.h \
    libmaia/maiaXmlRpcServerConnection.h \
    libmaia/maiaXmlRpcServer.h \
    libmaia/maiaXmlRpcClient.h \
    libmaia/maiaObject.h \
    libmaia/maiaFault.h \
    trackers/Backend.h \
    trackers/Bugzilla.h \
    NewTracker.h \
    trackers/NovellBugzilla.h \
    Autodetector.h \
    SqlBugModel.h \
    SqlBugDelegate.h \
    CommentFrame.h \
    UploadDialog.h \
    Preferences.h \
    About.h \
    SqlWriterThread.h \
    PlaceholderLineEdit.h \
    qtsoap/qtsoap.h \
    trackers/Mantis.h \
    ChangelogWindow.h \
    ChangelogListDelegate.h \
    qjson/qjson_debug.h \
    qjson/position.hh \
    qjson/parserrunnable.h \
    qjson/parser_p.h \
    qjson/parser.h \
    qjson/location.hh \
    qjson/json_scanner.h \
    qjson/json_parser.hh \
    qjson/stack.hh \
    qjson/serializerrunnable.h \
    qjson/serializer.h \
    qjson/qobjecthelper.h \
    qjson/qjson_export.h \
    trackers/Trac.h \
    Utilities.hpp \
    ErrorHandler.h \
    ErrorReport.h \
    qtsingleapplication/qtsinglecoreapplication.h \
    qtsingleapplication/qtsingleapplication.h \
    qtsingleapplication/qtlockedfile.h \
    qtsingleapplication/qtlocalpeer.h \
    Translator.h \
    MonitorDialog.h \
    tracker_uis/BugzillaUI.h \
    tracker_uis/TracUI.h \
    tracker_uis/MantisUI.h \
    SqlUtilities.h \
    tracker_uis/BackendUI.h \
    NewCommentsDialog.h \
    tracker_uis/TracDetails.h \
    TrackerTabWidget.h \
    tracker_uis/MantisDetails.h \
    tracker_uis/BugzillaDetails.h \
    ClickableText.h \
    tracker_uis/BackendDetails.h \
    SearchTab.h \
    SqlSearchModel.h \
    ToDoListView.h \
    ToDoListServiceAdd.h \
    ToDoListPreferences.h \
    ToDoListExport.h \
    ToDoList.h \
    ToDoItem.h \
    BugTreeWidget.h \
    BugTreeItemDelegate.h \
    todolistservices/ServicesBackend.h \
    todolistservices/RememberTheMilk.h \
    todolistservices/GenericWebDav.h \
    TrackerTableView.h \
    SqlSearchDelegate.h \
    todolistservices/GoogleTasks.h
FORMS += MainWindow.ui \
    NewTracker.ui \
    CommentFrame.ui \
    UploadDialog.ui \
    Preferences.ui \
    About.ui \
    ChangelogWindow.ui \
    ErrorReport.ui \
    MonitorDialog.ui \
    tracker_uis/bugzillaui.ui \
    tracker_uis/tracui.ui \
    tracker_uis/mantisui.ui \
    NewCommentsDialog.ui \
    tracker_uis/TracDetails.ui \
    tracker_uis/MantisDetails.ui \
    tracker_uis/BugzillaDetails.ui \
    SearchTab.ui \
    ToDoListServiceAdd.ui \
    ToDoListPreferences.ui \
    ToDoListView.ui \
    ToDoListExport.ui
RESOURCES += resources.qrc
QMAKE_EXTRA_TARGETS += distfile
DISTFILE_MAKEDIR = .tmp/entomologist-$$VERSION
DISTFILE_EXTRAFILES = $$RESOURCES \
    COPYING \
    Entomologist.pro \
    README \
    INSTALL \
    *.qm \
    *.ts \
    entomologist.desktop \
    entomologist.spec \
    graphics/ \
    install_icons/ \
    libmaia/
distfile.commands = mkdir \
    -p \
    $$DISTFILE_MAKEDIR \
    && \
    cp \
    -r \
    -f \
    --parent \
    $$SOURCES \
    $$HEADERS \
    $$FORMS \
    $$DISTFILE_EXTRAFILES \
    $$DISTFILE_MAKEDIR \
    && \
    cd \
    .tmp \
    && \
    tar \
    cvzf \
    entomologist-$$VERSION\.tar.gz \
    entomologist-$$VERSION \
    && \
    mv \
    entomologist-$$VERSION\.tar.gz \
    .. \
    && \
    cd \
    ..
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




