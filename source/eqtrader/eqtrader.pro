#-------------------------------------------------
#
# Project created by QtCreator 2018-01-12T22:54:14
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = quantManage4
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
LIBS += -lnanomsg
LIBS +=-lyaml-cpp

SOURCES += \
    ctickevent.cpp \
    clientmq.cpp \
    main.cpp \
    mainwindow.cpp \
    strategymanager.cpp \
    strategybase.cpp

HEADERS += \
    enums.h \
    clientmq.h \
    ctickevent.h \
    mainwindow.h \
    strategymanager.h \
    strategybase.h

FORMS += \
        mainwindow.ui

RESOURCES += \
    qdarkstyle/style.qrc

TRANSLATIONS+=en.ts \
			  cn.ts
