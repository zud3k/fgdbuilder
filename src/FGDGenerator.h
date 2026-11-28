#pragma once
#include "FGDData.h"
#include <QString>
#include <QVector>

class FGDGenerator {
public:
    static QString generate(const FGDProject &project);
    static QString generateEntityText(const FGDEntity &entity);
    static QList<FGDEntity> exportOrder(const QList<FGDEntity> &entities);
    static QVector<int> sortPermutation(const QList<FGDEntity> &entities);
    static bool isNumericValue(const QString &s);
private:
    static QString generateEntity(const FGDEntity &entity);
    static QString generateKeyvalue(const FGDKeyvalue &kv);
    static QString generateIO(const FGDIO &io);
    static QString escapeDescription(const QString &desc);
    static QString quoted(const QString &s);
    static bool defaultNeedsQuotes(const FGDKeyvalue &kv);
};
