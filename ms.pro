TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    tools.cpp \
    scanner.cpp \
    compiler.cpp \
    parser.cpp \
    translator.cpp

HEADERS += \
    scanner.hpp \
    tools.hpp \
    compiler.hpp \
    parser.hpp \
    translator.hpp
