#pragma once
#include <QString>

#ifndef FGDBUILDER_VERSION
#error "FGDBUILDER_VERSION is not defined, build the project with qmake (see fgdbuilder.pro)"
#endif

inline QString appVersion() { return QStringLiteral(FGDBUILDER_VERSION); }
inline QString appVersionLabel() { return QStringLiteral("FGDBuilder v") + appVersion(); }
