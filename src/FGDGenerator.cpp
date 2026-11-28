#include "FGDGenerator.h"
#include <QStringList>
#include <QHash>
#include <QSet>
#include <algorithm>
#include <functional>

QString FGDGenerator::escapeDescription(const QString &desc) {
    QString r = desc;
    r.replace("\"", "'");
    return r;
}

QString FGDGenerator::quoted(const QString &s) {
    return "\"" + s + "\"";
}

bool FGDGenerator::isNumericValue(const QString &s) {
    int i = 0;
    int n = s.length();
    if (i < n && s[i] == '-') ++i;
    int digits = 0;
    while (i < n && s[i].isDigit()) { ++i; ++digits; }
    if (digits == 0) return false;
    if (i < n && s[i] == '.') {
        ++i;
        int frac = 0;
        while (i < n && s[i].isDigit()) { ++i; ++frac; }
        if (frac == 0) return false;
    }
    return i == n;
}

static bool isBareNumber(const QString &s) {
    if (FGDGenerator::isNumericValue(s)) return true;
    QString t = s.startsWith('-') ? s.mid(1) : s;
    if (t.length() > 2 && (t.startsWith("0x") || t.startsWith("0X"))) {
        for (int i = 2; i < t.length(); ++i)
            if (!t[i].isDigit() && !(t[i].toLower() >= 'a' && t[i].toLower() <= 'f')) return false;
        return true;
    }
    return false;
}

bool FGDGenerator::defaultNeedsQuotes(const FGDKeyvalue &kv) {
    if (kv.type == "float") return true;
    if (kv.type == "integer" || kv.type == "boolean")
        return !kv.defaultValue.isEmpty() && !isBareNumber(kv.defaultValue);
    if (kv.type == "choices") {
        if (!kv.defaultValue.isEmpty() && !isBareNumber(kv.defaultValue)) return true;
        for (const auto &c : kv.choiceItems)
            if (!isNumericValue(c.first)) return true;
        return false;
    }
    return true;
}

QString FGDGenerator::generateKeyvalue(const FGDKeyvalue &kv) {
    bool isFlags = (kv.type == "flags");
    QString name = (isFlags && kv.name.isEmpty()) ? QString("spawnflags") : kv.name;
    QString line = "\t" + QString(kv.keyKeyword ? "key " : "") + name + "(" + kv.type + ")";
    if (kv.readOnly) line += " readonly";
    if (kv.report)   line += " report";
    if (!kv.extra.isEmpty()) line += " " + kv.extra;

    if (isFlags) {
        bool hasDisplay = !kv.displayName.isEmpty();
        bool hasDesc = !kv.description.isEmpty();
        if (hasDisplay || hasDesc) {
            line += " : " + quoted(escapeDescription(kv.displayName));
            if (hasDesc) line += " : " + quoted(escapeDescription(kv.description));
        }
        line += " =\n\t[\n";
        for (int i = 0; i < kv.flagItems.size(); ++i) {
            auto &f = kv.flagItems[i];
            QString desc = (i < kv.flagDescriptions.size()) ? kv.flagDescriptions[i] : QString();
            if (!desc.isEmpty()) {
                line += QString("\t\t%1 : \"%2\" : %3 : \"%4\"\n")
                    .arg(f.first)
                    .arg(escapeDescription(f.second.first))
                    .arg(f.second.second)
                    .arg(escapeDescription(desc));
            } else {
                line += QString("\t\t%1 : \"%2\" : %3\n")
                    .arg(f.first)
                    .arg(escapeDescription(f.second.first))
                    .arg(f.second.second);
            }
        }
        line += "\t]\n";
        return line;
    }

    bool hasDisplay = !kv.displayName.isEmpty();
    bool hasDefault = !kv.defaultValue.isEmpty();
    bool hasDesc    = !kv.description.isEmpty();

    if (hasDisplay || hasDefault || hasDesc) {
        line += " : " + quoted(escapeDescription(kv.displayName));
        if (hasDefault || hasDesc) {
            bool needQuotes = defaultNeedsQuotes(kv);
            if (hasDefault) {
                line += " : " + (needQuotes ? quoted(kv.defaultValue) : kv.defaultValue);
            } else if (kv.type == "string" || kv.type == "float" || (kv.type == "choices" && needQuotes)) {
                line += " : \"\"";
            } else {
                line += " :";
            }
            if (hasDesc) {
                line += " : " + quoted(escapeDescription(kv.description));
            }
        }
    }

    if (kv.type == "choices") {
        line += " =\n\t[\n";
        for (int i = 0; i < kv.choiceItems.size(); ++i) {
            auto &c = kv.choiceItems[i];
            QString desc = (i < kv.choiceDescriptions.size()) ? kv.choiceDescriptions[i].second : QString();
            QString value = isNumericValue(c.first) ? c.first : quoted(c.first);
            if (!desc.isEmpty()) {
                line += "\t\t" + value + " : " + quoted(escapeDescription(c.second)) + " : " + quoted(escapeDescription(desc)) + "\n";
            } else {
                line += "\t\t" + value + " : " + quoted(escapeDescription(c.second)) + "\n";
            }
        }
        line += "\t]";
    }
    return line + "\n";
}

QString FGDGenerator::generateIO(const FGDIO &io) {
    QString line = "\t";
    line += io.isInput ? "input " : "output ";
    line += io.name + "(" + io.type + ")";
    if (!io.description.isEmpty()) {
        line += " : " + quoted(escapeDescription(io.description));
    }
    return line + "\n";
}

static QString wrapDescription(const QString &desc, int lineLen) {
    if (desc.length() <= lineLen) return " : \"" + desc + "\"";
    QStringList parts;
    int start = 0;
    while (start < desc.length()) {
        if (desc.length() - start <= lineLen) {
            parts << desc.mid(start);
            break;
        }
        int cut = start + lineLen;
        int space = desc.lastIndexOf(' ', cut);
        int end;
        if (space > start) {
            end = space + 1;
        } else {
            end = cut;
            if (end - 1 > start && desc[end - 1] == '\\' && desc[end] == 'n') --end;
        }
        parts << desc.mid(start, end - start);
        start = end;
    }
    return " :\n\t\"" + parts.join("\" +\n\t\"") + "\"";
}

QString FGDGenerator::generateEntity(const FGDEntity &entity) {
    QString out;
    out += classTypeToString(entity.classType);
    if (!entity.bases.isEmpty())
        out += " base(" + entity.bases.join(", ") + ")";
    if (!entity.color.isEmpty())
        out += " color(" + entity.color + ")";
    if (!entity.size.isEmpty())
        out += " size(" + entity.size + ")";
    for (const FGDHelper &h : entity.helpers) {
        out += " " + h.name;
        if (h.parens) out += "(" + h.args + ")";
    }
    if (entity.halfGridSnap)    out += " halfgridsnap";

    out += " = " + entity.className;
    bool hasDesc = !entity.description.isEmpty();
    if (hasDesc) {
        out += wrapDescription(escapeDescription(entity.description), 120);
    }
    if (!entity.url.isEmpty()) {
        if (!hasDesc) out += " : \"\"";
        out += " : " + quoted(entity.url);
    }
    out += "\n[\n";
    for (auto &kv : entity.keyvalues)
        out += generateKeyvalue(kv);
    bool hasInputs = false, hasOutputs = false;
    for (auto &io : entity.ios) {
        if (io.isInput)  hasInputs  = true;
        else             hasOutputs = true;
    }
    if (hasInputs) {
        out += "\n\t// Inputs\n";
        for (auto &io : entity.ios)
            if (io.isInput) out += generateIO(io);
    }
    if (hasOutputs) {
        out += "\n\t// Outputs\n";
        for (auto &io : entity.ios)
            if (!io.isInput) out += generateIO(io);
    }
    out += "]\n\n";
    return out;
}

QString FGDGenerator::generateEntityText(const FGDEntity &entity) {
    return generateEntity(entity);
}

static void generateAutoVisGroupChildren(QString &out, const QList<FGDAutoVisGroupChild> &children, int depth) {
    QString indent(depth, '\t');
    for (auto &child : children) {
        out += indent + "\"" + child.name + "\"\n" + indent + "[\n";
        for (auto &e : child.entities)
            out += indent + "\t\"" + e + "\"\n";
        generateAutoVisGroupChildren(out, child.children, depth + 1);
        out += indent + "]\n";
    }
}

QList<FGDEntity> FGDGenerator::exportOrder(const QList<FGDEntity> &entities) {
    QList<FGDEntity> result;
    result.reserve(entities.size());
    QHash<QString, int> firstIndex;
    for (int i = 0; i < entities.size(); ++i) {
        if (entities[i].classType == EntityClassType::ExtendClass) continue;
        QString key = entities[i].className;
        if (!firstIndex.contains(key)) firstIndex.insert(key, i);
    }
    QVector<char> state(entities.size(), 0);
    std::function<void(int)> place = [&](int idx) {
        if (state[idx]) return;
        state[idx] = 1;
        for (const QString &b : entities[idx].bases) {
            auto it = firstIndex.constFind(b);
            if (it != firstIndex.constEnd() && it.value() != idx) place(it.value());
        }
        result.append(entities[idx]);
    };
    for (int i = 0; i < entities.size(); ++i)
        if (entities[i].classType == EntityClassType::BaseClass) place(i);
    for (int i = 0; i < entities.size(); ++i)
        if (entities[i].classType != EntityClassType::ExtendClass) place(i);
    for (int i = 0; i < entities.size(); ++i)
        if (entities[i].classType == EntityClassType::ExtendClass) result.append(entities[i]);
    return result;
}

QVector<int> FGDGenerator::sortPermutation(const QList<FGDEntity> &entities) {
    QVector<int> order(entities.size());
    for (int i = 0; i < order.size(); ++i) order[i] = i;
    auto group = [&](int i) {
        EntityClassType t = entities[i].classType;
        return t == EntityClassType::BaseClass ? 0 : (t == EntityClassType::ExtendClass ? 2 : 1);
    };
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        int ga = group(a), gb = group(b);
        if (ga != gb) return ga < gb;
        return entities[a].className.toLower() < entities[b].className.toLower();
    });
    return order;
}

QString FGDGenerator::generate(const FGDProject &project) {
    QString out;
    if (!project.headerComment.isEmpty()) {
        for (auto &l : project.headerComment.split("\n"))
            out += "// " + l + "\n";
        out += "\n";
    }
    if (project.fgdVersion > 0)
        out += QString("@version(%1)\n\n").arg(project.fgdVersion);
    for (auto &inc : project.includes)
        out += "@include \"" + inc + "\"\n";
    if (!project.includes.isEmpty()) out += "\n";
    if (project.useMapsSize)
        out += QString("@mapsize(%1, %2)\n\n").arg(project.mapsizeMin).arg(project.mapsizeMax);
    if (project.useGridNav)
        out += QString("@gridnav(%1, %2, %3, %4)\n\n")
            .arg(project.gridNavEdgeSize).arg(project.gridNavOffsetX)
            .arg(project.gridNavOffsetY).arg(project.gridNavTraceHeight);
    if (!project.materialExclusions.isEmpty()) {
        out += "@MaterialExclusion\n[\n";
        for (auto &m : project.materialExclusions)
            out += "\t\"" + m + "\"\n";
        out += "]\n\n";
    }
    for (auto &avg : project.autoVisGroups) {
        out += "@AutoVisGroup = \"" + avg.parent + "\"\n[\n";
        generateAutoVisGroupChildren(out, avg.children, 1);
        out += "]\n\n";
    }
    QList<FGDEntity> ordered = exportOrder(project.entities);
    bool rawWritten = project.rawBlocks.isEmpty();
    auto writeRaw = [&]() {
        for (const QString &raw : project.rawBlocks) out += raw + "\n\n";
        rawWritten = true;
    };
    for (auto &entity : ordered) {
        if (!rawWritten && entity.classType == EntityClassType::ExtendClass) writeRaw();
        out += generateEntity(entity);
    }
    if (!rawWritten) writeRaw();
    return out;
}
