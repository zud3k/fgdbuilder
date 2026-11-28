#include "FGDProjectIO.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

static QJsonObject kvToJson(const FGDKeyvalue &kv) {
    QJsonObject o;
    o["name"] = kv.name;
    o["type"] = kv.type;
    o["displayName"] = kv.displayName;
    o["defaultValue"] = kv.defaultValue;
    o["description"] = kv.description;
    o["readOnly"] = kv.readOnly;
    o["report"] = kv.report;
    o["keyKeyword"] = kv.keyKeyword;
    o["extra"] = kv.extra;
    QJsonArray choices;
    for (int i = 0; i < kv.choiceItems.size(); ++i) {
        auto &c = kv.choiceItems[i];
        QString desc = (i < kv.choiceDescriptions.size()) ? kv.choiceDescriptions[i].second : QString();
        QJsonObject ci; ci["value"] = c.first; ci["label"] = c.second; ci["desc"] = desc;
        choices.append(ci);
    }
    o["choices"] = choices;
    QJsonArray flags;
    for (int i = 0; i < kv.flagItems.size(); ++i) {
        auto &f = kv.flagItems[i];
        QString desc = (i < kv.flagDescriptions.size()) ? kv.flagDescriptions[i] : QString();
        QJsonObject fi; fi["bit"] = f.first; fi["label"] = f.second.first; fi["def"] = f.second.second; fi["desc"] = desc;
        flags.append(fi);
    }
    o["flags"] = flags;
    return o;
}

static FGDKeyvalue kvFromJson(const QJsonObject &o) {
    FGDKeyvalue kv;
    kv.name = o["name"].toString();
    kv.type = o["type"].toString();
    kv.displayName = o["displayName"].toString();
    kv.defaultValue = o["defaultValue"].toString();
    kv.description = o["description"].toString();
    kv.readOnly = o["readOnly"].toBool();
    kv.report = o["report"].toBool();
    kv.keyKeyword = o["keyKeyword"].toBool();
    kv.extra = o["extra"].toString();
    for (auto v : o["choices"].toArray()) {
        auto ci = v.toObject();
        kv.choiceItems.append({ci["value"].toString(), ci["label"].toString()});
        kv.choiceDescriptions.append({ci["value"].toString(), ci["desc"].toString()});
    }
    for (auto v : o["flags"].toArray()) {
        auto fi = v.toObject();
        kv.flagItems.append({fi["bit"].toString(), {fi["label"].toString(), fi["def"].toInt()}});
        kv.flagDescriptions.append(fi["desc"].toString());
    }
    return kv;
}

static QJsonObject ioToJson(const FGDIO &io) {
    QJsonObject o;
    o["isInput"] = io.isInput;
    o["name"] = io.name;
    o["type"] = io.type;
    o["description"] = io.description;
    return o;
}

static FGDIO ioFromJson(const QJsonObject &o) {
    FGDIO io;
    io.isInput = o["isInput"].toBool(true);
    io.name = o["name"].toString();
    io.type = o["type"].toString();
    io.description = o["description"].toString();
    return io;
}

static QJsonObject childToJson(const FGDAutoVisGroupChild &c) {
    QJsonObject co;
    co["name"] = c.name;
    QJsonArray entArr;
    for (auto &e : c.entities) entArr.append(e);
    co["entities"] = entArr;
    QJsonArray childArr;
    for (auto &sc : c.children) childArr.append(childToJson(sc));
    co["children"] = childArr;
    return co;
}

static FGDAutoVisGroupChild childFromJson(const QJsonObject &co) {
    FGDAutoVisGroupChild c;
    c.name = co["name"].toString();
    for (auto ev : co["entities"].toArray()) c.entities << ev.toString();
    for (auto cv : co["children"].toArray()) c.children << childFromJson(cv.toObject());
    return c;
}

static QJsonObject entityToJson(const FGDEntity &e) {
    QJsonObject o;
    o["classType"]   = classTypeToName(e.classType);
    o["className"]   = e.className;
    o["description"] = e.description;
    o["url"]         = e.url;
    o["bases"]       = QJsonArray::fromStringList(e.bases);
    o["color"]       = e.color;
    o["size"]        = e.size;
    o["halfGridSnap"] = e.halfGridSnap;
    QJsonArray helpers;
    for (const FGDHelper &h : e.helpers) {
        QJsonObject ho;
        ho["name"] = h.name;
        ho["args"] = h.args;
        ho["parens"] = h.parens;
        helpers.append(ho);
    }
    o["helpers"] = helpers;
    QJsonArray kvs;
    for (auto &kv : e.keyvalues) kvs.append(kvToJson(kv));
    o["keyvalues"] = kvs;
    QJsonArray ios;
    for (auto &io : e.ios) ios.append(ioToJson(io));
    o["ios"] = ios;
    return o;
}

static void addLegacyHelper(FGDEntity &e, const QString &name, const QString &value, bool quote) {
    if (value.isEmpty()) return;
    FGDHelper h;
    h.name = name;
    h.args = quote ? "\"" + value + "\"" : value;
    e.helpers.append(h);
}

static void addLegacyFlag(FGDEntity &e, const QString &name, bool on) {
    if (!on) return;
    FGDHelper h;
    h.name = name;
    e.helpers.append(h);
}

static FGDEntity entityFromJson(const QJsonObject &o) {
    FGDEntity e;
    QJsonValue ct = o["classType"];
    EntityClassType parsed = EntityClassType::PointClass;
    if (ct.isString()) {
        if (classTypeFromName(ct.toString(), parsed)) e.classType = parsed;
    } else if (classTypeFromLegacyIndex(ct.toInt(0), parsed)) {
        e.classType = parsed;
    }
    e.className   = o["className"].toString();
    e.description = o["description"].toString();
    e.url         = o["url"].toString();
    for (auto v : o["bases"].toArray()) e.bases << v.toString();
    e.color       = o["color"].toString();
    e.size        = o["size"].toString();
    e.halfGridSnap = o["halfGridSnap"].toBool();
    if (o.contains("helpers")) {
        for (auto v : o["helpers"].toArray()) {
            QJsonObject ho = v.toObject();
            FGDHelper h;
            h.name = ho["name"].toString();
            h.args = ho["args"].toString();
            h.parens = ho["parens"].toBool(true);
            e.helpers.append(h);
        }
    } else {
        addLegacyHelper(e, "studio", o["studioModel"].toString(), true);
        addLegacyHelper(e, "studioprop", o["studioprop"].toString(), true);
        addLegacyHelper(e, "iconsprite", o["iconSprite"].toString(), true);
        addLegacyHelper(e, "sphere", o["sphere"].toString(), false);
        addLegacyHelper(e, "line", o["line"].toString(), false);
        addLegacyHelper(e, "vecline", o["vecline"].toString(), false);
        addLegacyHelper(e, "axis", o["axis"].toString(), false);
        addLegacyHelper(e, "wirebox", o["wirebox"].toString(), false);
        addLegacyHelper(e, "origin", o["origin"].toString(), false);
        addLegacyHelper(e, "lightprop", o["lightprop"].toString(), true);
        addLegacyFlag(e, "decal", o["decal"].toBool());
        addLegacyFlag(e, "overlay", o["overlay"].toBool());
        addLegacyFlag(e, "sprite", o["sprite"].toBool());
        addLegacyFlag(e, "sweptplayerhull", o["sweptplayerhull"].toBool());
        addLegacyFlag(e, "instance", o["instance_"].toBool());
        addLegacyFlag(e, "animator", o["animator"].toBool());
        addLegacyFlag(e, "keyframe", o["keyframe_"].toBool());
        addLegacyFlag(e, "worldtext", o["worldtext"].toBool());
    }
    for (auto v : o["keyvalues"].toArray()) e.keyvalues.append(kvFromJson(v.toObject()));
    for (auto v : o["ios"].toArray())       e.ios.append(ioFromJson(v.toObject()));
    return e;
}

bool FGDProjectIO::load(const QString &path, FGDProject &out, QString &error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = "Cannot open file: " + path;
        return false;
    }
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    f.close();
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        error = "Not a valid FGDBuilder project (" + path + "): " + pe.errorString();
        return false;
    }
    QJsonObject root = doc.object();
    FGDProject proj;
    proj.headerComment = root["headerComment"].toString();
    for (auto v : root["includes"].toArray()) proj.includes << v.toString();
    proj.fgdVersion = root["fgdVersion"].toInt(0);
    proj.mapsizeMin = root["mapsizeMin"].toInt(-16384);
    proj.mapsizeMax = root["mapsizeMax"].toInt(16384);
    proj.useMapsSize = root["useMapsSize"].toBool(false);
    proj.useGridNav = root["useGridNav"].toBool(false);
    proj.gridNavEdgeSize = root["gridNavEdgeSize"].toInt(64);
    proj.gridNavOffsetX = root["gridNavOffsetX"].toInt(32);
    proj.gridNavOffsetY = root["gridNavOffsetY"].toInt(32);
    proj.gridNavTraceHeight = root["gridNavTraceHeight"].toInt(3500);
    for (auto v : root["materialExclusions"].toArray()) proj.materialExclusions << v.toString();
    for (auto v : root["rawBlocks"].toArray()) proj.rawBlocks << v.toString();
    for (auto v : root["entities"].toArray()) proj.entities.append(entityFromJson(v.toObject()));
    for (auto gv : root["autoVisGroups"].toArray()) {
        FGDAutoVisGroup g;
        QJsonObject go = gv.toObject();
        g.parent = go["parent"].toString();
        for (auto cv : go["children"].toArray())
            g.children.append(childFromJson(cv.toObject()));
        proj.autoVisGroups.append(g);
    }
    out = proj;
    return true;
}

bool FGDProjectIO::save(const QString &path, const FGDProject &p, QString &error) {
    QJsonObject root;
    root["formatVersion"] = CurrentFormatVersion;
    root["headerComment"] = p.headerComment;
    QJsonArray incs;
    for (auto &s : p.includes) incs.append(s);
    root["includes"] = incs;
    root["fgdVersion"] = p.fgdVersion;
    root["mapsizeMin"] = p.mapsizeMin;
    root["mapsizeMax"] = p.mapsizeMax;
    root["useMapsSize"] = p.useMapsSize;
    root["useGridNav"] = p.useGridNav;
    root["gridNavEdgeSize"] = p.gridNavEdgeSize;
    root["gridNavOffsetX"] = p.gridNavOffsetX;
    root["gridNavOffsetY"] = p.gridNavOffsetY;
    root["gridNavTraceHeight"] = p.gridNavTraceHeight;
    root["materialExclusions"] = QJsonArray::fromStringList(p.materialExclusions);
    root["rawBlocks"] = QJsonArray::fromStringList(p.rawBlocks);
    QJsonArray ents;
    for (auto &e : p.entities) ents.append(entityToJson(e));
    root["entities"] = ents;
    QJsonArray avgArr;
    for (auto &g : p.autoVisGroups) {
        QJsonObject go;
        go["parent"] = g.parent;
        QJsonArray childArr;
        for (auto &c : g.children) childArr.append(childToJson(c));
        go["children"] = childArr;
        avgArr.append(go);
    }
    root["autoVisGroups"] = avgArr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        error = "Cannot write file: " + path;
        return false;
    }
    f.write(QJsonDocument(root).toJson());
    f.close();
    return true;
}
