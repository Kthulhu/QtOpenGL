#-------------------------------------------------
#
# Project created by QtCreator 2015-01-11T15:14:10
#
#-------------------------------------------------

TEMPLATE = lib
CONFIG += staticlib c++11
TARGET = OpenGL
QT += widgets
include(../config.pri)

SOURCES += \
    openglprofiler.cpp \
    openglframetimer.cpp \
    openglframeresults.cpp \
    openglerror.cpp \
    openglshaderprogram.cpp \
    openglprofilervisualizer.cpp \
    openglmarkerresult.cpp \
    openglwidget.cpp \
    openglmesh.cpp \
    opengluniformbufferobject.cpp \
    openglslparser.cpp \
    openglinstancegroup.cpp \
    openglframebufferobject.cpp \
    openglrenderbufferobject.cpp \
    opengltexture.cpp \
    openglmaterial.cpp \
    openglinstance.cpp \
    openglpointlight.cpp \
    openglpointlightgroup.cpp \
    opengldebugdraw.cpp \
    opengluniformbuffermanager.cpp \
    openglspotlightgroup.cpp \
    openglabstractlightgroup.cpp \
    openglelementtype.cpp \
    opengldirectionlight.cpp \
    opengldirectionlightgroup.cpp \
    openglspotlight.cpp \
    openglrenderblock.cpp \
    opengllight.cpp \
    openglfunctions.cpp

HEADERS += \
    openglprofiler.h \
    openglframetimer.h \
    openglframeresults.h \
    openglmarkerresult.h \
    openglfunctions.h \
    openglcommon.h \
    openglerror.h \
    openglmarkerscoped.h \
    openglbuffer.h \
    openglshaderprogram.h \
    openglvertexarrayobject.h \
    openglprofilervisualizer.h \
    openglwidget.h \
    openglmesh.h \
    openglfunctions_es3_0.h \
    openglfunctions_3_3_core.h \
    opengluniformbufferobject.h \
    openglslparser.h \
    openglinstancegroup.h \
    openglframebufferobject.h \
    opengltexture.h \
    openglrenderbufferobject.h \
    openglmaterial.h \
    openglinstance.h \
    openglpointlight.h \
    openglpointlightgroup.h \
    openglstorage.h \
    opengldebugdraw.h \
    opengluniformbuffermanager.h \
    openglspotlightgroup.h \
    openglabstractlightgroup.h \
    opengllightgroup.h \
    openglpointlightdata.h \
    opengldynamicbuffer.h \
    openglelementtype.h \
    openglinstancedata.h \
    opengldirectionlight.h \
    opengldirectionlightdata.h \
    opengldirectionlightgroup.h \
    openglspotlight.h \
    openglspotlightdata.h \
    openglrenderblock.h \
    openglrenderblockdata.h \
    opengllight.h \
    opengltranslationlight.h \
    openglrenderpass.h \
    openglrenderer.h
