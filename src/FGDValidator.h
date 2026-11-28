#pragma once
#include "FGDData.h"
#include "FGDResolver.h"
#include <QList>
#include <QString>

struct FGDIssue {
    enum Level { Info, Warning, Error };
    Level level = Warning;
    QString entity;
    QString property;
    QString message;
    int line = 0;
};

class FGDValidator {
public:
    static QList<FGDIssue> validate(const FGDProject &project, const FGDIncludeResult *externals);
    static QString levelName(FGDIssue::Level level);
    static QString formatIssue(const FGDIssue &issue);
    static int count(const QList<FGDIssue> &issues, FGDIssue::Level level);
};
