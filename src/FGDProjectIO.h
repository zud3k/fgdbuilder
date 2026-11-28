#pragma once
#include "FGDData.h"
#include <QString>

class FGDProjectIO {
public:
    static const int CurrentFormatVersion = 2;
    static bool load(const QString &path, FGDProject &out, QString &error);
    static bool save(const QString &path, const FGDProject &project, QString &error);
};
