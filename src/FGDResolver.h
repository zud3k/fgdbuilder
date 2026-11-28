#pragma once
#include "FGDData.h"
#include <QHash>
#include <QString>
#include <QStringList>

struct FGDIncludeResult {
    QList<FGDEntity> classes;
    QStringList classFiles;
    QStringList loadedFiles;
    QStringList warnings;
    bool complete = true;
};

class FGDIncludes {
public:
    static FGDIncludeResult load(const QStringList &includes, const QString &baseDir);
private:
    static void loadFile(const QString &path, QStringList &stack, QStringList &visited, FGDIncludeResult &out, int depth);
    static QString locate(const QString &name, const QStringList &dirs);
};

class FGDClassIndex {
public:
    struct Entry {
        FGDEntity entity;
        bool external = false;
        QString file;
    };

    struct Item {
        bool isIO = false;
        FGDKeyvalue kv;
        FGDIO io;
        QString from;
        bool overridden = false;
        QString overriddenBy;
        QString name() const { return isIO ? io.name : kv.name; }
    };

    void build(const QList<FGDEntity> &own, const FGDIncludeResult *externals);
    const Entry *find(const QString &name) const;
    bool contains(const QString &name) const { return find(name) != nullptr; }
    QStringList classNames(bool baseClassesOnly = false) const;
    QStringList entityNames() const;
    QList<Item> inherited(const FGDEntity &e) const;
    int effectiveKeyvalueCount(const FGDEntity &e) const;
    QStringList findCycle(const FGDEntity &e) const;
    bool isExternal(const QString &name) const;
private:
    QList<Entry> m_all;
    QHash<QString, int> m_byName;

    void collectFrom(const QString &className, QList<Item> &out, QStringList &stack) const;
    static void addOwn(const FGDEntity &e, QList<Item> &out, bool append);
    bool cycleDfs(const QString &name, QStringList &path) const;
};
