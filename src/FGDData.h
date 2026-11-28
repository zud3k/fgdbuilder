#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

enum class EntityClassType {
    PointClass,
    SolidClass,
    NPCClass,
    KeyFrameClass,
    MoveClass,
    FilterClass,
    BaseClass,
    ExtendClass
};

inline QString classTypeToString(EntityClassType t) {
    switch (t) {
        case EntityClassType::PointClass:    return "@PointClass";
        case EntityClassType::SolidClass:    return "@SolidClass";
        case EntityClassType::NPCClass:      return "@NPCClass";
        case EntityClassType::KeyFrameClass: return "@KeyFrameClass";
        case EntityClassType::MoveClass:     return "@MoveClass";
        case EntityClassType::FilterClass:   return "@FilterClass";
        case EntityClassType::BaseClass:     return "@BaseClass";
        case EntityClassType::ExtendClass:   return "@ExtendClass";
    }
    return "@PointClass";
}

inline QString classTypeToName(EntityClassType t) {
    return classTypeToString(t).mid(1);
}

inline bool classTypeFromName(const QString &name, EntityClassType &out) {
    QString n = name.trimmed();
    if (n.startsWith('@')) n = n.mid(1);
    n = n.toLower();
    if (n == "pointclass")         { out = EntityClassType::PointClass;    return true; }
    if (n == "solidclass")         { out = EntityClassType::SolidClass;    return true; }
    if (n == "npcclass")           { out = EntityClassType::NPCClass;      return true; }
    if (n == "keyframeclass")      { out = EntityClassType::KeyFrameClass; return true; }
    if (n == "moveclass")          { out = EntityClassType::MoveClass;     return true; }
    if (n == "filterclass")        { out = EntityClassType::FilterClass;   return true; }
    if (n == "baseclass")          { out = EntityClassType::BaseClass;     return true; }
    if (n == "extendclass")        { out = EntityClassType::ExtendClass;   return true; }
    return false;
}

inline bool classTypeFromLegacyIndex(int idx, EntityClassType &out) {
    if (idx < 0 || idx > 6) return false;
    out = static_cast<EntityClassType>(idx);
    return true;
}

struct FGDKeyvalue {
    QString name;
    QString type;
    QString displayName;
    QString defaultValue;
    QString description;
    QString extra;
    bool readOnly = false;
    bool report = false;
    bool keyKeyword = false;
    int line = 0;
    QList<QPair<QString,QString>> choiceItems;
    QList<QPair<QString,QString>> choiceDescriptions;
    QList<QPair<QString, QPair<QString,int>>> flagItems;
    QStringList flagDescriptions;
};

inline bool operator==(const FGDKeyvalue &a, const FGDKeyvalue &b) {
    return a.name == b.name && a.type == b.type && a.displayName == b.displayName
        && a.defaultValue == b.defaultValue && a.description == b.description
        && a.extra == b.extra && a.readOnly == b.readOnly && a.report == b.report
        && a.keyKeyword == b.keyKeyword && a.choiceItems == b.choiceItems
        && a.choiceDescriptions == b.choiceDescriptions && a.flagItems == b.flagItems
        && a.flagDescriptions == b.flagDescriptions;
}

struct FGDIO {
    bool isInput = true;
    QString name;
    QString type;
    QString description;
    int line = 0;
};

inline bool operator==(const FGDIO &a, const FGDIO &b) {
    return a.isInput == b.isInput && a.name == b.name && a.type == b.type && a.description == b.description;
}

struct FGDHelper {
    QString name;
    QString args;
    bool parens = true;
};

inline bool operator==(const FGDHelper &a, const FGDHelper &b) {
    return a.name == b.name && a.args == b.args && a.parens == b.parens;
}

struct FGDEntity {
    EntityClassType classType = EntityClassType::PointClass;
    QString className;
    QString description;
    QString url;
    QStringList bases;
    QString color;
    QString size;
    bool halfGridSnap = false;
    QList<FGDHelper> helpers;
    QList<FGDKeyvalue> keyvalues;
    QList<FGDIO> ios;
    int line = 0;
};

inline bool operator==(const FGDEntity &a, const FGDEntity &b) {
    return a.classType == b.classType && a.className == b.className && a.description == b.description
        && a.url == b.url && a.bases == b.bases && a.color == b.color && a.size == b.size
        && a.halfGridSnap == b.halfGridSnap && a.helpers == b.helpers
        && a.keyvalues == b.keyvalues && a.ios == b.ios;
}

struct FGDAutoVisGroupChild {
    QString name;
    QStringList entities;
    QList<FGDAutoVisGroupChild> children;
};

inline bool operator==(const FGDAutoVisGroupChild &a, const FGDAutoVisGroupChild &b) {
    return a.name == b.name && a.entities == b.entities && a.children == b.children;
}

struct FGDAutoVisGroup {
    QString parent;
    QList<FGDAutoVisGroupChild> children;
};

inline bool operator==(const FGDAutoVisGroup &a, const FGDAutoVisGroup &b) {
    return a.parent == b.parent && a.children == b.children;
}

struct FGDProjectSettings {
    QString headerComment;
    QStringList includes;
    int mapsizeMin = -16384;
    int mapsizeMax = 16384;
    bool useMapsSize = false;
    int fgdVersion = 0;
    bool useGridNav = false;
    int gridNavEdgeSize = 64;
    int gridNavOffsetX = 32;
    int gridNavOffsetY = 32;
    int gridNavTraceHeight = 3500;
    QList<FGDAutoVisGroup> autoVisGroups;
    QStringList materialExclusions;
    QStringList rawBlocks;
};

inline bool operator==(const FGDProjectSettings &a, const FGDProjectSettings &b) {
    return a.headerComment == b.headerComment && a.includes == b.includes
        && a.mapsizeMin == b.mapsizeMin && a.mapsizeMax == b.mapsizeMax && a.useMapsSize == b.useMapsSize
        && a.fgdVersion == b.fgdVersion && a.useGridNav == b.useGridNav
        && a.gridNavEdgeSize == b.gridNavEdgeSize && a.gridNavOffsetX == b.gridNavOffsetX
        && a.gridNavOffsetY == b.gridNavOffsetY && a.gridNavTraceHeight == b.gridNavTraceHeight
        && a.autoVisGroups == b.autoVisGroups && a.materialExclusions == b.materialExclusions
        && a.rawBlocks == b.rawBlocks;
}

struct FGDProject : FGDProjectSettings {
    QList<FGDEntity> entities;
};

inline FGDProjectSettings projectSettings(const FGDProject &p) {
    return static_cast<const FGDProjectSettings &>(p);
}

inline void setProjectSettings(FGDProject &p, const FGDProjectSettings &s) {
    static_cast<FGDProjectSettings &>(p) = s;
}

inline const FGDHelper *findHelper(const FGDEntity &e, const QString &name) {
    for (const FGDHelper &h : e.helpers)
        if (h.name.compare(name, Qt::CaseInsensitive) == 0) return &h;
    return nullptr;
}
