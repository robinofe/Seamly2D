# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

HEADERS += \
    $$PWD/reference_lines/reference_line_tool.h \
    $$PWD/images/image_dialog.h \
    $$PWD/images/image_tool.h \
    $$PWD/stable.h \
    $$PWD/images/image_item.h

SOURCES += \
    $$PWD/reference_lines/reference_line_tool.cpp \
    $$PWD/images/image_dialog.cpp \
    $$PWD/images/image_item.cpp \
    $$PWD/images/image_tool.cpp

FORMS += \
    $$PWD/images/image_dialog.ui

*msvc*:SOURCES += $$PWD/stable.cpp
