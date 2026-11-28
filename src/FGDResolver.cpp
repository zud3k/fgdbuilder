#include "FGDResolver.h"
#include "FGDImporter.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>

QString FGDIncludes::locate(const QString &name, const QStringList &dirs) {
    QFileInfo direct(name);
    if (direct.isAbsolute()) return direct.exists() ? direct.canonicalFilePath() : QString();
    for (const QString &d : dirs) {
        if (d.isEmpty()) continue;
        QFileInfo fi(QDir(d).filePath(name));
        if (fi.exists() && fi.isFile()) return fi.canonicalFilePath();
    }
    return QString();
}

void FGDIncludes::loadFile(const QString &path, QStringList &stack, QStringList &visited, FGDIncludeResult &out, int depth) {
    if (depth > 16) {
        out.warnings << "@include nesting is deeper than 16 levels at " + path + ", the rest was not loaded.";
        out.complete = false;
        return;
    }
    QString err;
    FGDProject proj = FGDImporter::importFromFile(path, err, nullptr);
    if (!err.isEmpty()) {
        out.warnings << err;
        out.complete = false;
        return;
    }
    visited << path;
    stack << path;
    QString dir = QFileInfo(path).absolutePath();
    for (const QString &inc : proj.includes) {
        QString found = locate(inc, QStringList() << dir);
        if (found.isEmpty()) {
            out.warnings << "Included file '" + inc + "' (from " + QFileInfo(path).fileName() + ") was not found in " + dir + ".";
            out.complete = false;
            continue;
        }
        if (stack.contains(found)) {
            out.warnings << "Circular @include: " + QFileInfo(found).fileName() + " includes itself through " + QFileInfo(path).fileName() + ".";
            continue;
        }
        if (visited.contains(found)) continue;
        loadFile(found, stack, visited, out, depth + 1);
    }
    stack.removeLast();
    out.loadedFiles << path;
    QString name = QFileInfo(path).fileName();
    for (const FGDEntity &e : proj.entities) {
        out.classes.append(e);
        out.classFiles << name;
    }
}

FGDIncludeResult FGDIncludes::load(const QStringList &includes, const QString &baseDir) {
    FGDIncludeResult out;
    QStringList stack, visited;
    for (const QString &inc : includes) {
        QString found = locate(inc, QStringList() << baseDir);
        if (found.isEmpty()) {
            out.warnings << "Included file '" + inc + "' was not found in " + (baseDir.isEmpty() ? QString("(no base folder, save the project or import the FGD first)") : baseDir) + ".";
            out.complete = false;
            continue;
        }
        if (visited.contains(found)) continue;
        loadFile(found, stack, visited, out, 1);
    }
    return out;
}

void FGDClassIndex::build(const QList<FGDEntity> &own, const FGDIncludeResult *externals) {
    m_all.clear();
    m_byName.clear();
    if (externals) {
        for (int i = 0; i < externals->classes.size(); ++i) {
            Entry en;
            en.entity = externals->classes[i];
            en.external = true;
            en.file = i < externals->classFiles.size() ? externals->classFiles[i] : QString();
            m_all.append(en);
        }
    }
    for (const FGDEntity &e : own) {
        Entry en;
        en.entity = e;
        m_all.append(en);
    }
    for (int i = 0; i < m_all.size(); ++i) {
        if (m_all[i].entity.classType == EntityClassType::ExtendClass) continue;
        m_byName.insert(m_all[i].entity.className, i);
    }
}

const FGDClassIndex::Entry *FGDClassIndex::find(const QString &name) const {
    auto it = m_byName.constFind(name);
    if (it == m_byName.constEnd()) return nullptr;
    return &m_all[it.value()];
}

bool FGDClassIndex::isExternal(const QString &name) const {
    const Entry *en = find(name);
    return en && en->external;
}

QStringList FGDClassIndex::classNames(bool baseClassesOnly) const {
    QStringList names;
    QSet<QString> seen;
    for (const Entry &en : m_all) {
        if (en.entity.classType == EntityClassType::ExtendClass) continue;
        if (baseClassesOnly && en.entity.classType != EntityClassType::BaseClass) continue;
        QString key = en.entity.className;
        if (seen.contains(key)) continue;
        seen.insert(key);
        names << en.entity.className;
    }
    return names;
}

QStringList FGDClassIndex::entityNames() const {
    QStringList names;
    QSet<QString> seen;
    for (const Entry &en : m_all) {
        EntityClassType t = en.entity.classType;
        if (t == EntityClassType::ExtendClass || t == EntityClassType::BaseClass) continue;
        if (seen.contains(en.entity.className)) continue;
        seen.insert(en.entity.className);
        names << en.entity.className;
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

void FGDClassIndex::addOwn(const FGDEntity &e, QList<Item> &out, bool append) {
    for (const FGDKeyvalue &kv : e.keyvalues) {
        for (Item &it : out) {
            if (it.isIO || it.overridden) continue;
            if (it.kv.name.compare(kv.name, Qt::CaseInsensitive) != 0) continue;
            if (it.kv.type == "flags" && kv.type == "flags") continue;
            it.overridden = true;
            it.overriddenBy = e.className;
        }
        if (append) {
            Item item;
            item.kv = kv;
            item.from = e.className;
            out.append(item);
        }
    }
    for (const FGDIO &io : e.ios) {
        for (Item &it : out) {
            if (!it.isIO || it.overridden) continue;
            if (it.io.isInput != io.isInput || it.io.name.compare(io.name, Qt::CaseInsensitive) != 0) continue;
            it.overridden = true;
            it.overriddenBy = e.className;
        }
        if (append) {
            Item item;
            item.isIO = true;
            item.io = io;
            item.from = e.className;
            out.append(item);
        }
    }
}

void FGDClassIndex::collectFrom(const QString &className, QList<Item> &out, QStringList &stack) const {
    const Entry *en = find(className);
    if (!en) return;
    QString key = en->entity.className;
    if (stack.contains(key)) return;
    stack << key;
    for (const QString &b : en->entity.bases) collectFrom(b, out, stack);
    addOwn(en->entity, out, true);
    stack.removeLast();
}

QList<FGDClassIndex::Item> FGDClassIndex::inherited(const FGDEntity &e) const {
    QList<Item> out;
    QStringList stack;
    stack << e.className;
    for (const QString &b : e.bases) collectFrom(b, out, stack);
    addOwn(e, out, false);
    return out;
}

int FGDClassIndex::effectiveKeyvalueCount(const FGDEntity &e) const {
    QSet<QString> names;
    for (const Item &it : inherited(e))
        if (!it.isIO) names.insert(it.kv.name.toLower());
    for (const FGDKeyvalue &kv : e.keyvalues) names.insert(kv.name.toLower());
    return names.size();
}

bool FGDClassIndex::cycleDfs(const QString &name, QStringList &path) const {
    int at = path.indexOf(name);
    if (at >= 0) {
        path = path.mid(at);
        path << name;
        return true;
    }
    const Entry *en = find(name);
    if (!en) return false;
    path << en->entity.className;
    for (const QString &b : en->entity.bases) {
        QStringList p = path;
        if (cycleDfs(b, p)) { path = p; return true; }
    }
    return false;
}

QStringList FGDClassIndex::findCycle(const FGDEntity &e) const {
    QStringList path;
    path << e.className;
    for (const QString &b : e.bases) {
        QStringList p = path;
        if (cycleDfs(b, p)) {
            int at = -1;
            for (int i = 0; i < p.size() - 1; ++i)
                if (p[i] == p.last()) { at = i; break; }
            return at >= 0 ? p.mid(at) : p;
        }
    }
    return QStringList();
}
