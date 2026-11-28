#include "FGDValidator.h"
#include <QSet>

QString FGDValidator::levelName(FGDIssue::Level level) {
    switch (level) {
        case FGDIssue::Error:   return "error";
        case FGDIssue::Warning: return "warning";
        case FGDIssue::Info:    return "info";
    }
    return "warning";
}

QString FGDValidator::formatIssue(const FGDIssue &issue) {
    QString who = issue.entity;
    if (!issue.property.isEmpty()) who += (who.isEmpty() ? "" : ".") + issue.property;
    QString text = levelName(issue.level).toUpper() + ": ";
    if (!who.isEmpty()) text += who + ": ";
    return text + issue.message;
}

int FGDValidator::count(const QList<FGDIssue> &issues, FGDIssue::Level level) {
    int n = 0;
    for (const FGDIssue &i : issues) if (i.level == level) ++n;
    return n;
}

static void add(QList<FGDIssue> &out, FGDIssue::Level level, const FGDEntity &e, const QString &prop, const QString &msg, int line = 0) {
    FGDIssue i;
    i.level = level;
    i.entity = e.className;
    i.property = prop;
    i.message = msg;
    i.line = line ? line : e.line;
    out.append(i);
}

static bool hasQuote(const QString &s) {
    return s.contains('"');
}

static bool choiceValuesEqual(const QString &a, const QString &b) {
    if (a == b) return true;
    bool ok1, ok2;
    double x = a.toDouble(&ok1);
    double y = b.toDouble(&ok2);
    return ok1 && ok2 && x == y;
}

static void checkKeyvalue(QList<FGDIssue> &out, const FGDEntity &e, const FGDKeyvalue &kv) {
    QString prop = kv.name;
    int line = kv.line;
    if (kv.name.isEmpty() && kv.type != "flags")
        add(out, FGDIssue::Error, e, QString(), "Keyvalue with an empty name.", line);
    if (kv.type.isEmpty())
        add(out, FGDIssue::Error, e, prop, "Keyvalue has no type.", line);
    if (kv.name.length() > 30)
        add(out, FGDIssue::Warning, e, prop, QString("Internal name is %1 characters long, Hammer supports up to 30.").arg(kv.name.length()), line);
    if (kv.name.contains('.') || kv.name.contains('#'))
        add(out, FGDIssue::Warning, e, prop, "Internal name contains '.' or '#', Hammer cannot load such names from an FGD.", line);

    if (hasQuote(kv.displayName)) add(out, FGDIssue::Error, e, prop, "Display name contains a quotation mark, use apostrophes instead.", line);
    if (hasQuote(kv.description)) add(out, FGDIssue::Error, e, prop, "Description contains a quotation mark, use apostrophes instead.", line);
    if (kv.description.contains("\\n"))
        add(out, FGDIssue::Warning, e, prop, "Description contains \\n, which only works in Hammer++ for keyvalue descriptions.", line);

    if (kv.type == "boolean" && kv.defaultValue != "0" && kv.defaultValue != "1")
        add(out, FGDIssue::Error, e, prop, "Default of a boolean must be exactly 0 or 1" + (kv.defaultValue.isEmpty() ? QString(", it is empty.") : QString(", it is '" + kv.defaultValue + "'.")), line);

    if (kv.type == "choices") {
        for (int i = 0; i < kv.choiceItems.size(); ++i) {
            if (hasQuote(kv.choiceItems[i].second))
                add(out, FGDIssue::Error, e, prop, "Choice '" + kv.choiceItems[i].first + "' has a quotation mark in its name.", line);
            if (i < kv.choiceDescriptions.size() && hasQuote(kv.choiceDescriptions[i].second))
                add(out, FGDIssue::Error, e, prop, "Choice '" + kv.choiceItems[i].first + "' has a quotation mark in its description.", line);
        }
        if (!kv.defaultValue.isEmpty()) {
            bool found = false;
            for (const auto &c : kv.choiceItems)
                if (choiceValuesEqual(c.first, kv.defaultValue)) { found = true; break; }
            if (!found)
                add(out, FGDIssue::Warning, e, prop, "Default '" + kv.defaultValue + "' is not one of the choice values.", line);
        }
    }

    if (kv.type == "flags") {
        for (int i = 0; i < kv.flagItems.size(); ++i) {
            const auto &fi = kv.flagItems[i];
            bool ok;
            qlonglong val = fi.first.toLongLong(&ok);
            if (!ok || val <= 0 || (val & (val - 1)) != 0)
                add(out, FGDIssue::Error, e, prop, "Flag value '" + fi.first + "' is not a power of two.", line);
            else if (val == 2147483648LL)
                add(out, FGDIssue::Warning, e, prop, "Flag value 2147483648 is bit 31, Hammer only uses the first 31 flags reliably.", line);
            else if (val > 2147483648LL)
                add(out, FGDIssue::Error, e, prop, "Flag value " + fi.first + " does not fit in 32 bits.", line);
            if (hasQuote(fi.second.first))
                add(out, FGDIssue::Error, e, prop, "Flag " + fi.first + " has a quotation mark in its name.", line);
            if (i < kv.flagDescriptions.size() && hasQuote(kv.flagDescriptions[i]))
                add(out, FGDIssue::Error, e, prop, "Flag " + fi.first + " has a quotation mark in its description.", line);
        }
    }
}

QList<FGDIssue> FGDValidator::validate(const FGDProject &proj, const FGDIncludeResult *externals) {
    QList<FGDIssue> out;
    FGDClassIndex index;
    index.build(proj.entities, externals);
    bool includesComplete = !externals || externals->complete;
    if (externals) {
        for (const QString &w : externals->warnings) {
            FGDIssue i;
            i.level = FGDIssue::Warning;
            i.message = w;
            out.append(i);
        }
    }

    QSet<QString> seenNames;
    QSet<QString> ownNormal;
    for (const FGDEntity &e : proj.entities)
        if (e.classType != EntityClassType::ExtendClass) ownNormal.insert(e.className);

    for (const FGDEntity &e : proj.entities) {
        if (e.className.isEmpty()) {
            add(out, FGDIssue::Error, e, QString(), "Entity with an empty class name.");
            continue;
        }
        if (e.classType != EntityClassType::ExtendClass) {
            QString key = e.className;
            if (seenNames.contains(key))
                add(out, FGDIssue::Error, e, QString(), "Duplicate entity name.");
            seenNames.insert(key);
        }
        int len = e.className.length();
        if (len > 63)
            add(out, FGDIssue::Error, e, QString(), QString("Class name is %1 characters long, Hammer crashes when it shows the properties of a class name over 63 characters.").arg(len));
        if (len > 80)
            add(out, FGDIssue::Error, e, QString(), QString("Class name is over 80 characters (%1), Hammer crashes while loading a map that uses it.").arg(len));
        if (hasQuote(e.description))
            add(out, FGDIssue::Error, e, QString(), "Description contains a quotation mark, use apostrophes instead.");

        QStringList cycle = index.findCycle(e);
        if (!cycle.isEmpty()) {
            QString msg = "base() cycle: " + cycle.join(" -> ") + ".";
            if (cycle.size() == 2) msg += " A class that lists itself in base() makes Hammer hang.";
            add(out, FGDIssue::Error, e, QString(), msg);
        }
        for (const QString &base : e.bases) {
            if (index.contains(base)) continue;
            add(out, includesComplete ? FGDIssue::Error : FGDIssue::Warning, e, QString(),
                "Base class '" + base + "' was not found in the project" + (includesComplete ? QString(" or in the included files.") : QString(" (some included files could not be loaded, so it may be defined there).")));
        }
        if (e.classType == EntityClassType::ExtendClass) {
            if (!index.contains(e.className))
                add(out, FGDIssue::Warning, e, QString(), "@ExtendClass target '" + e.className + "' was not found, Hammer++ ignores such a class.");
        }

        QSet<QString> ownKv;
        for (const FGDKeyvalue &kv : e.keyvalues) {
            checkKeyvalue(out, e, kv);
            QString key = kv.name.toLower();
            if (!kv.name.isEmpty()) {
                if (ownKv.contains(key))
                    add(out, FGDIssue::Warning, e, kv.name, "Keyvalue is defined more than once in this entity.", kv.line);
                ownKv.insert(key);
            }
        }
        for (const FGDIO &io : e.ios) {
            if (hasQuote(io.description))
                add(out, FGDIssue::Error, e, io.name, "I/O description contains a quotation mark, use apostrophes instead.", io.line);
        }

        QList<FGDClassIndex::Item> inh = index.inherited(e);
        QSet<QString> reported;
        for (const auto &it : inh) {
            if (!it.overridden || it.overriddenBy != e.className) continue;
            QString key = QString(it.isIO ? "io:" : "kv:") + it.name().toLower();
            if (reported.contains(key)) continue;
            reported.insert(key);
            add(out, FGDIssue::Info, e, it.name(), QString(it.isIO ? "I/O" : "Keyvalue") + " overrides the definition from base '" + it.from + "'.");
        }

        int props = index.effectiveKeyvalueCount(e);
        if (props > 127)
            add(out, FGDIssue::Error, e, QString(), QString("Entity has %1 keyvalues including inherited ones, Hammer crashes above 127.").arg(props));
        else if (props > 118)
            add(out, FGDIssue::Warning, e, QString(), QString("Entity has %1 keyvalues including inherited ones, Hammer crashes when the class of an entity is changed to one with more than 118.").arg(props));
    }
    return out;
}
