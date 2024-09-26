win32-msvc* {
    QMAKE_CXXFLAGS += /source-charset:utf-8 /execution-charset:utf-8
}

QT       += core gui
#QT       += multimedia
QT       += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ./inc

SOURCES += \
    camerathread.cpp \
    crc16.cpp \
    gaugepanel.cpp \
    login.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    CustomTitleBar.h \
    camerathread.h \
    crc16.h \
    gaugepanel.h \
    login.h \
    mainwindow.h \
    nncam.h \
    rectItem.h \
    myGraphicsScene.h

FORMS += \
    login.ui \
    mainwindow.ui

LIBS += -L$$PWD/x64 -lnncam

CONFIG(debug, debug|release): LIBS += -L$$PWD/x64 -lopencv_world480d
else:CONFIG(release, debug|release): LIBS += -L$$PWD/x64 -lopencv_world480

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource/qdarkstyle/dark/darkstyle.qrc \
    resource/resource.qrc
