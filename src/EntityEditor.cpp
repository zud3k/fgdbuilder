#include "EntityEditor.h"
#include "KeyvalueEditor.h"
#include "IOEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>

static const QStringList FLAG_HELPER_NAMES = {
    "decal", "overlay", "sprite", "sweptplayerhull", "instance", "animator", "keyframe", "worldtext"
};

static const QStringList STANDARD_BASES = {
    "Targetname", "Parentname", "Angles", "Origin", "EnableDisable", "Shadow",
    "RenderFields", "Global", "ResponseContext", "Studiomodel"
};

EntityEditor::EntityEditor(QWidget *parent, const FGDEntity &entity, const QStringList &knownBases, const FGDClassIndex *index)
    : QDialog(parent), m_original(entity), m_index(index), m_knownBases(knownBases)
{
    setWindowTitle("Entity Editor");
    setMinimumSize(720, 820);
    setupUi();
    populateFrom(entity);
    QSettings settings("FGDBuilder", "FGDBuilder");
    restoreGeometry(settings.value("entityEditorGeometry").toByteArray());
}

void EntityEditor::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabs = new QTabWidget;

    QWidget *generalTab = new QWidget;
    QVBoxLayout *genLayout = new QVBoxLayout(generalTab);
    QFormLayout *form = new QFormLayout;

    m_classType = new QComboBox;
    m_classType->addItems({"@PointClass","@SolidClass","@NPCClass","@KeyFrameClass","@MoveClass","@FilterClass","@BaseClass","@ExtendClass"});
    m_classType->setToolTip(
        "@PointClass - a point entity placed at a specific location\n"
        "@SolidClass - a brush entity (tied to a brush volume)\n"
        "@NPCClass - a point entity for NPCs; enables npcclass KV type\n"
        "@KeyFrameClass - for move_rope/keyframe_rope; links NextKey on copy\n"
        "@MoveClass - for path_track and similar; links target on copy\n"
        "@FilterClass - for filter entities; enables filterclass KV type\n"
        "@BaseClass - not shown in entity list; used only as a base for inheritance\n"
        "@ExtendClass - Hammer++ only; appends keyvalues and helpers to an existing class from another FGD"
    );

    m_className = new QLineEdit;
    m_className->setToolTip(
        "The classname of the entity as used in game code and the VMF file.\n"
        "Must match the C++ class name in the game.\n"
        "Maximum 63 characters in Source (over 80 will crash Hammer on map load).\n"
        "For @ExtendClass this is the name of the class to extend."
    );

    m_description = new QTextEdit;
    m_description->setMaximumHeight(80);
    m_description->setToolTip(
        "Description shown in Hammer's Help box when the mapper clicks 'Help'.\n"
        "\\n works as a line break in entity descriptions. Do not use quotation marks (use apostrophes instead).\n"
        "Lines longer than 120 characters will be split automatically in the FGD output."
    );

    m_url = new QLineEdit;
    m_url->setPlaceholderText("https://... (J.A.C.K. only, optional)");
    m_url->setToolTip("J.A.C.K. only. A web page for this entity, written as a second string after the description.");

    form->addRow("Class type:", m_classType);
    form->addRow("Class name:", m_className);
    form->addRow("Description:", m_description);
    form->addRow("URL:", m_url);

    m_color = new QLineEdit;
    m_color->setPlaceholderText("R G B  e.g. 255 128 0");
    m_color->setToolTip(
        "Color of the entity wireframe box in Hammer's 2D views (R G B, each 0-255).\n"
        "If omitted, defaults to magenta (220 30 220).\n"
        "Note: does not affect color in Hammer 4.x (only text color); fixed in Hammer++."
    );
    m_size = new QLineEdit;
    m_size->setPlaceholderText("e.g. -8 -8 -8, 8 8 8");
    m_size->setToolTip(
        "Defines the bounding box of the entity when no model or sprite is shown.\n"
        "Format: -x -y -z, +x +y +z  (e.g. -8 -8 -8, 8 8 8 for a 16x16x16 cube).\n"
        "Ignored by studioprop() which uses the model's own bounding box."
    );
    m_halfGridSnap = new QCheckBox("halfgridsnap");
    m_halfGridSnap->setToolTip(
        "When moving this entity, it snaps to half the current grid size.\n"
        "Takes no arguments. Supported in Hammer 4.x."
    );

    form->addRow("Color:", m_color);
    form->addRow("Size:", m_size);
    form->addRow("", m_halfGridSnap);

    struct Def { const char *name; bool quoted; const char *placeholder; const char *tip; };
    static const Def defs[] = {
        {"studio", true, "e.g. models/combine_soldier.mdl  (empty = use 'model' KV)",
         "Display a model in the 3D view. If the path is empty, the entity's 'model' keyvalue is used.\nSeveral models can be separated by commas."},
        {"studioprop", true, "e.g. models/props/barrel.mdl  (empty = use 'model' KV)",
         "Like studio(), but uses the model's bounding box instead of size().\nRequired for entities with 'angles' that should be rotatable with the mouse in Hammer."},
        {"iconsprite", true, "e.g. editor/env_cubemap.vmt",
         "Display a sprite icon in the 3D view. Can be used alongside studio()/studioprop().\nUses VMT in Hammer 4.x, VMAT in Hammer 5.x, SPR in other editors."},
        {"sphere", false, "property name, e.g. radius  (empty = radius)",
         "sphere(propertyname) - draws a sphere around the entity. Radius taken from the named KV.\nCan be used several times for different properties."},
        {"line", false, "e.g. 255 255 255, targetname, target, targetname, entity",
         "line(color, start_key, start_value, end_key, end_value) - draws a line between two entities.\nOther lines can be added in the raw helpers box below."},
        {"vecline", false, "property name",
         "vecline(property) - positions a vector property and draws a line from the entity to it."},
        {"axis", false, "property name",
         "axis(property) - positions two points joined by a line in the map."},
        {"wirebox", false, "e.g. mins, maxs",
         "wirebox(min, max) - draws a bounding box between two properties."},
        {"origin", false, "property name, e.g. origin",
         "origin(property) - allows positioning a vector property in the map. Updates when entity is moved."},
        {"lightprop", true, "e.g. models/props/light.mdl",
         "lightprop(\"path\") - like studioprop(), but pitch of the model is inverted. Hammer 4.x."},
    };

    QGroupBox *modelGroup = new QGroupBox("Model / Sprite helpers");
    modelGroup->setToolTip(
        "Controls how this entity is displayed in the map editor's 3D view.\n"
        "A ticked box with an empty field writes the helper without arguments, for example studio()."
    );
    QFormLayout *modelForm = new QFormLayout(modelGroup);
    QGroupBox *extraHelpers = new QGroupBox("Additional helpers");
    extraHelpers->setToolTip("Optional helpers that add extra visual aids in the editor viewport.");
    QFormLayout *extraForm = new QFormLayout(extraHelpers);

    int n = 0;
    for (const Def &d : defs) {
        HelperRow row;
        row.name = d.name;
        row.quoted = d.quoted;
        row.check = new QCheckBox(QString(d.name) + "()");
        row.check->setToolTip(d.tip);
        row.edit = new QLineEdit;
        row.edit->setPlaceholderText(d.placeholder);
        row.edit->setEnabled(false);
        connect(row.check, &QCheckBox::toggled, row.edit, &QLineEdit::setEnabled);
        QHBoxLayout *h = new QHBoxLayout;
        h->addWidget(row.check);
        h->addWidget(row.edit, 1);
        if (n < 3) modelForm->addRow(h);
        else extraForm->addRow(h);
        m_helperRows.append(row);
        ++n;
    }

    QGridLayout *flagHelpers = new QGridLayout;
    QStringList flagTips = {
        "Renders decals on nearby surfaces. Uses the 'texture' KV for the material.",
        "Renders overlays on a surface. Used for info_overlay. Auto-sets UV corner KVs.",
        "Renders the sprite material from the 'model' KV (env_sprite and variants).",
        "Draws 32x32x72 rectangular prisms at point0 and point1 to show movement space.",
        "Renders the instance VMF in the map. Generates additional KVs for instance parameters.",
        "Used with @MoveClass to mark this entity as the first in a chain.",
        "Automatically renames this entity when cloned.",
        "Displays the 'message' KV contents in the 3D viewport. Hammer 4.x / Strata."
    };
    for (int i = 0; i < FLAG_HELPER_NAMES.size(); ++i) {
        QCheckBox *cb = new QCheckBox(FLAG_HELPER_NAMES[i] + "()");
        cb->setToolTip(flagTips[i]);
        flagHelpers->addWidget(cb, i / 4, i % 4);
        m_flagHelpers.append(cb);
    }
    extraForm->addRow("Flag helpers:", flagHelpers);

    m_rawHelpers = new QPlainTextEdit;
    m_rawHelpers->setMinimumHeight(64);
    m_rawHelpers->setMaximumHeight(110);
    m_rawHelpers->setPlaceholderText("lightcone(_inner_cone, _cone, _light, -1)\nline(255 255 255, targetname, cpoint2, targetname, entity)");
    m_rawHelpers->setToolTip(
        "Raw text of helpers that have no field above: unknown helpers (lightcone, frustum, cylinder, sidelist, light,\n"
        "quadbounds, overlay_transition and so on) and repeated helpers such as a second sphere() or many line().\n"
        "One helper per line, written exactly as in the FGD. They are written back unchanged."
    );
    extraForm->addRow("Other helpers (raw):", m_rawHelpers);

    genLayout->addLayout(form);
    genLayout->addWidget(modelGroup);
    genLayout->addWidget(extraHelpers);

    QGroupBox *basesGroup = new QGroupBox("Base classes");
    basesGroup->setToolTip(
        "Attach previously defined @BaseClass entities to inherit their keyvalues and helpers.\n"
        "Multiple bases can be specified. Classes from @include files are marked in italics."
    );
    QVBoxLayout *basesLayout = new QVBoxLayout(basesGroup);
    m_baseList = new QListWidget;
    m_baseList->setMaximumHeight(90);
    QHBoxLayout *baseInputRow = new QHBoxLayout;
    m_baseCombo = new QComboBox;
    m_baseCombo->setEditable(true);
    m_baseCombo->setInsertPolicy(QComboBox::NoInsert);
    m_baseCombo->setPlaceholderText("Base class name");
    m_baseCombo->setToolTip("Select or type the name of a @BaseClass defined in this project or an included FGD.");
    for (auto &b : m_knownBases) {
        m_baseCombo->addItem(b);
        if (m_index && m_index->isExternal(b)) {
            int i = m_baseCombo->count() - 1;
            QFont f = m_baseCombo->font();
            f.setItalic(true);
            m_baseCombo->setItemData(i, f, Qt::FontRole);
            const FGDClassIndex::Entry *en = m_index->find(b);
            m_baseCombo->setItemData(i, "Included class from " + (en ? en->file : QString()), Qt::ToolTipRole);
        }
    }
    m_baseCombo->setCurrentIndex(-1);
    QPushButton *addBaseBtn = new QPushButton("Add");
    QPushButton *removeBaseBtn = new QPushButton("Remove");
    QPushButton *presetBtn = new QPushButton("Standard bases");
    QMenu *presetMenu = new QMenu(presetBtn);
    for (const QString &name : STANDARD_BASES) {
        QAction *a = presetMenu->addAction(name);
        connect(a, &QAction::triggered, this, [this, name]{ addBasePreset(name); });
    }
    presetBtn->setMenu(presetMenu);
    presetBtn->setToolTip("Add a common base class from base.fgd with one click.");
    baseInputRow->addWidget(m_baseCombo, 1);
    baseInputRow->addWidget(addBaseBtn);
    baseInputRow->addWidget(removeBaseBtn);
    baseInputRow->addWidget(presetBtn);
    basesLayout->addWidget(m_baseList);
    basesLayout->addLayout(baseInputRow);
    genLayout->addWidget(basesGroup);
    genLayout->addStretch();
    QScrollArea *generalScroll = new QScrollArea;
    generalScroll->setWidgetResizable(true);
    generalScroll->setFrameShape(QFrame::NoFrame);
    generalScroll->setWidget(generalTab);
    tabs->addTab(generalScroll, "General");

    QWidget *kvTab = new QWidget;
    QVBoxLayout *kvLayout = new QVBoxLayout(kvTab);
    QLabel *kvHint = new QLabel(
        "Keyvalues (KVs) define the entity's configurable properties. "
        "Each KV has an internal name, a type, an optional display name, default value, and description."
    );
    kvHint->setWordWrap(true);
    kvLayout->addWidget(kvHint);
    m_kvSearch = new QLineEdit;
    m_kvSearch->setPlaceholderText("Search keyvalues by name, type, default, description...");
    m_kvSearch->setClearButtonEnabled(true);
    kvLayout->addWidget(m_kvSearch);
    m_kvTable = new QTableWidget(0, 4);
    m_kvTable->setHorizontalHeaderLabels({"Name","Type","Default","Description"});
    m_kvTable->horizontalHeader()->setStretchLastSection(true);
    m_kvTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kvTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kvTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QHBoxLayout *kvBtns = new QHBoxLayout;
    QPushButton *addKV = new QPushButton("Add Keyvalue");
    QPushButton *editKV = new QPushButton("Edit");
    QPushButton *dupKV = new QPushButton("Duplicate");
    QPushButton *removeKV = new QPushButton("Remove");
    m_kvUp = new QPushButton("Up");
    m_kvDown = new QPushButton("Down");
    kvBtns->addWidget(addKV);
    kvBtns->addWidget(editKV);
    kvBtns->addWidget(dupKV);
    kvBtns->addWidget(removeKV);
    kvBtns->addWidget(m_kvUp);
    kvBtns->addWidget(m_kvDown);
    kvBtns->addStretch();
    kvLayout->addWidget(m_kvTable);
    kvLayout->addLayout(kvBtns);
    tabs->addTab(kvTab, "Keyvalues");

    QWidget *ioTab = new QWidget;
    QVBoxLayout *ioLayout = new QVBoxLayout(ioTab);
    QLabel *ioHint = new QLabel(
        "Inputs are actions that can be triggered on this entity via I/O connections. "
        "Outputs are events this entity fires that can trigger other entities."
    );
    ioHint->setWordWrap(true);
    ioLayout->addWidget(ioHint);
    m_ioSearch = new QLineEdit;
    m_ioSearch->setPlaceholderText("Search inputs and outputs by name, type, description...");
    m_ioSearch->setClearButtonEnabled(true);
    ioLayout->addWidget(m_ioSearch);
    m_ioTable = new QTableWidget(0, 4);
    m_ioTable->setHorizontalHeaderLabels({"Direction","Name","Type","Description"});
    m_ioTable->horizontalHeader()->setStretchLastSection(true);
    m_ioTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ioTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ioTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QHBoxLayout *ioBtns = new QHBoxLayout;
    QPushButton *addIOBtn = new QPushButton("Add Input/Output");
    QPushButton *editIOBtn = new QPushButton("Edit");
    QPushButton *dupIOBtn = new QPushButton("Duplicate");
    QPushButton *removeIOBtn = new QPushButton("Remove");
    m_ioUp = new QPushButton("Up");
    m_ioDown = new QPushButton("Down");
    QPushButton *ioPresetBtn = new QPushButton("Presets");
    QMenu *ioMenu = new QMenu(ioPresetBtn);
    auto input = [](const QString &name, const QString &desc) { FGDIO io; io.isInput = true; io.name = name; io.type = "void"; io.description = desc; return io; };
    auto output = [](const QString &name, const QString &desc) { FGDIO io; io.isInput = false; io.name = name; io.type = "void"; io.description = desc; return io; };
    ioMenu->addAction("Enable", this, [this, input]{ addIoPreset({input("Enable", "Enable this entity.")}); });
    ioMenu->addAction("Disable", this, [this, input]{ addIoPreset({input("Disable", "Disable this entity.")}); });
    ioMenu->addAction("Toggle", this, [this, input]{ addIoPreset({input("Toggle", "Toggles this entity between enabled and disabled states.")}); });
    ioMenu->addAction("Kill", this, [this, input]{ addIoPreset({input("Kill", "Removes this entity from the world.")}); });
    ioMenu->addSeparator();
    ioMenu->addAction("Enable, Disable, Toggle", this, [this, input]{
        addIoPreset({input("Enable", "Enable this entity."), input("Disable", "Disable this entity."),
                     input("Toggle", "Toggles this entity between enabled and disabled states.")});
    });
    ioMenu->addAction("FireUser1-4 and OnUser1-4", this, [this, input, output]{
        QList<FGDIO> items;
        for (int i = 1; i <= 4; ++i)
            items << input(QString("FireUser%1").arg(i), QString("Causes this entity's OnUser%1 output to be fired.").arg(i));
        for (int i = 1; i <= 4; ++i)
            items << output(QString("OnUser%1").arg(i), QString("Fired in response to FireUser%1 input.").arg(i));
        addIoPreset(items);
    });
    ioPresetBtn->setMenu(ioMenu);
    ioPresetBtn->setToolTip("Add standard inputs and outputs with one click. Ones that already exist are skipped.");
    ioBtns->addWidget(addIOBtn);
    ioBtns->addWidget(editIOBtn);
    ioBtns->addWidget(dupIOBtn);
    ioBtns->addWidget(removeIOBtn);
    ioBtns->addWidget(m_ioUp);
    ioBtns->addWidget(m_ioDown);
    ioBtns->addWidget(ioPresetBtn);
    ioBtns->addStretch();
    ioLayout->addWidget(m_ioTable);
    ioLayout->addLayout(ioBtns);
    tabs->addTab(ioTab, "Inputs / Outputs");

    QWidget *inhTab = new QWidget;
    QVBoxLayout *inhLayout = new QVBoxLayout(inhTab);
    m_inheritedInfo = new QLabel;
    m_inheritedInfo->setWordWrap(true);
    inhLayout->addWidget(m_inheritedInfo);
    m_inheritedTable = new QTableWidget(0, 6);
    m_inheritedTable->setHorizontalHeaderLabels({"Kind","Name","Type","Default","Inherited from","Note"});
    m_inheritedTable->horizontalHeader()->setStretchLastSection(true);
    m_inheritedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_inheritedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    inhLayout->addWidget(m_inheritedTable);
    tabs->addTab(inhTab, "Inherited");

    mainLayout->addWidget(tabs);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);

    connect(addBaseBtn, &QPushButton::clicked, this, &EntityEditor::addBase);
    connect(removeBaseBtn, &QPushButton::clicked, this, &EntityEditor::removeBase);
    connect(addKV, &QPushButton::clicked, this, &EntityEditor::addKeyvalue);
    connect(editKV, &QPushButton::clicked, this, &EntityEditor::editKeyvalue);
    connect(dupKV, &QPushButton::clicked, this, &EntityEditor::duplicateKeyvalue);
    connect(removeKV, &QPushButton::clicked, this, &EntityEditor::removeKeyvalue);
    connect(m_kvUp, &QPushButton::clicked, this, [this]{ moveKeyvalue(-1); });
    connect(m_kvDown, &QPushButton::clicked, this, [this]{ moveKeyvalue(1); });
    connect(addIOBtn, &QPushButton::clicked, this, &EntityEditor::addIO);
    connect(editIOBtn, &QPushButton::clicked, this, &EntityEditor::editIO);
    connect(dupIOBtn, &QPushButton::clicked, this, &EntityEditor::duplicateIO);
    connect(removeIOBtn, &QPushButton::clicked, this, &EntityEditor::removeIO);
    connect(m_ioUp, &QPushButton::clicked, this, [this]{ moveIO(-1); });
    connect(m_ioDown, &QPushButton::clicked, this, [this]{ moveIO(1); });
    connect(m_kvSearch, &QLineEdit::textChanged, this, &EntityEditor::filterKeyvalues);
    connect(m_ioSearch, &QLineEdit::textChanged, this, &EntityEditor::filterIOs);
    connect(m_kvTable, &QTableWidget::cellDoubleClicked, this, &EntityEditor::editKeyvalue);
    connect(m_ioTable, &QTableWidget::cellDoubleClicked, this, &EntityEditor::editIO);
    connect(m_className, &QLineEdit::textChanged, this, &EntityEditor::refreshInherited);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString EntityEditor::helperDisplay(const QString &args, bool quoted) {
    if (quoted && args.length() >= 2 && args.startsWith('"') && args.endsWith('"') && args.indexOf('"', 1) == args.length() - 1)
        return args.mid(1, args.length() - 2);
    return args;
}

QString EntityEditor::helperArgs(const QString &text, bool quoted) {
    QString t = text.trimmed();
    if (t.isEmpty()) return QString();
    if (quoted && !t.contains('"')) return "\"" + t + "\"";
    return t;
}

bool EntityEditor::parseHelperLine(const QString &line, FGDHelper &out) {
    static const QRegularExpression re("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*(?:\\((.*)\\))?\\s*$");
    QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) return false;
    out.name = m.captured(1);
    out.parens = m.capturedStart(2) >= 0;
    out.args = out.parens ? m.captured(2).simplified() : QString();
    return true;
}

void EntityEditor::populateFrom(const FGDEntity &entity) {
    int idx = m_classType->findText(classTypeToString(entity.classType));
    if (idx >= 0) m_classType->setCurrentIndex(idx);
    m_className->setText(entity.className);
    m_description->setPlainText(entity.description);
    m_url->setText(entity.url);
    m_color->setText(entity.color);
    m_size->setText(entity.size);
    m_halfGridSnap->setChecked(entity.halfGridSnap);

    QList<bool> used;
    for (int i = 0; i < m_helperRows.size(); ++i) used << false;
    QList<bool> flagUsed;
    for (int i = 0; i < m_flagHelpers.size(); ++i) flagUsed << false;
    QStringList raw;
    for (const FGDHelper &h : entity.helpers) {
        bool placed = false;
        for (int i = 0; i < m_helperRows.size() && !placed; ++i) {
            if (used[i] || !h.parens || h.name.compare(m_helperRows[i].name, Qt::CaseInsensitive) != 0) continue;
            m_helperRows[i].check->setChecked(true);
            m_helperRows[i].edit->setText(helperDisplay(h.args, m_helperRows[i].quoted));
            used[i] = true;
            placed = true;
        }
        for (int i = 0; i < m_flagHelpers.size() && !placed; ++i) {
            if (flagUsed[i] || !h.parens || !h.args.isEmpty() || h.name.compare(FLAG_HELPER_NAMES[i], Qt::CaseInsensitive) != 0) continue;
            m_flagHelpers[i]->setChecked(true);
            flagUsed[i] = true;
            placed = true;
        }
        if (!placed) raw << (h.parens ? h.name + "(" + h.args + ")" : h.name);
    }
    m_rawHelpers->setPlainText(raw.join("\n"));

    for (auto &b : entity.bases) {
        QListWidgetItem *item = new QListWidgetItem(b);
        m_baseList->addItem(item);
        markBaseItem(item);
    }
    m_keyvalues = entity.keyvalues;
    m_ios = entity.ios;
    refreshKvTable();
    refreshIoTable();
    refreshInherited();
}

void EntityEditor::markBaseItem(QListWidgetItem *item) {
    if (!m_index) return;
    const FGDClassIndex::Entry *en = m_index->find(item->text());
    QFont f = item->font();
    f.setItalic(en && en->external);
    item->setFont(f);
    if (en && en->external) item->setToolTip("Included class from " + en->file + " (read only)");
    else if (!en) item->setToolTip("Not found in the project or in included files");
    else item->setToolTip(QString());
}

QStringList EntityEditor::currentBases() const {
    QStringList bases;
    for (int i = 0; i < m_baseList->count(); ++i) bases << m_baseList->item(i)->text();
    return bases;
}

QStringList EntityEditor::rawHelperLines() const {
    QStringList lines;
    for (const QString &l : m_rawHelpers->toPlainText().split('\n'))
        if (!l.trimmed().isEmpty()) lines << l.trimmed();
    return lines;
}

static QString kvSearchText(const FGDKeyvalue &kv) {
    QString t = kv.name + " " + kv.type + " " + kv.defaultValue + " " + kv.displayName + " " + kv.description;
    for (const auto &c : kv.choiceItems) t += " " + c.first + " " + c.second;
    for (const auto &f : kv.flagItems) t += " " + f.first + " " + f.second.first;
    return t.toLower();
}

void EntityEditor::refreshKvTable(int select) {
    m_kvTable->setRowCount(0);
    for (const FGDKeyvalue &kv : m_keyvalues) {
        int row = m_kvTable->rowCount();
        m_kvTable->insertRow(row);
        m_kvTable->setItem(row, 0, new QTableWidgetItem(kv.name));
        m_kvTable->setItem(row, 1, new QTableWidgetItem(kv.type));
        m_kvTable->setItem(row, 2, new QTableWidgetItem(kv.defaultValue));
        m_kvTable->setItem(row, 3, new QTableWidgetItem(kv.description));
    }
    filterKeyvalues(m_kvSearch->text());
    if (select >= 0 && select < m_kvTable->rowCount()) m_kvTable->selectRow(select);
}

void EntityEditor::refreshIoTable(int select) {
    m_ioTable->setRowCount(0);
    for (const FGDIO &io : m_ios) {
        int row = m_ioTable->rowCount();
        m_ioTable->insertRow(row);
        m_ioTable->setItem(row, 0, new QTableWidgetItem(io.isInput ? "Input" : "Output"));
        m_ioTable->setItem(row, 1, new QTableWidgetItem(io.name));
        m_ioTable->setItem(row, 2, new QTableWidgetItem(io.type));
        m_ioTable->setItem(row, 3, new QTableWidgetItem(io.description));
    }
    filterIOs(m_ioSearch->text());
    if (select >= 0 && select < m_ioTable->rowCount()) m_ioTable->selectRow(select);
}

void EntityEditor::filterKeyvalues(const QString &text) {
    QString f = text.trimmed().toLower();
    for (int i = 0; i < m_kvTable->rowCount() && i < m_keyvalues.size(); ++i)
        m_kvTable->setRowHidden(i, !f.isEmpty() && !kvSearchText(m_keyvalues[i]).contains(f));
    bool moveOk = f.isEmpty();
    m_kvUp->setEnabled(moveOk);
    m_kvDown->setEnabled(moveOk);
    m_kvUp->setToolTip(moveOk ? QString() : "Clear the search to reorder keyvalues.");
    m_kvDown->setToolTip(moveOk ? QString() : "Clear the search to reorder keyvalues.");
}

void EntityEditor::filterIOs(const QString &text) {
    QString f = text.trimmed().toLower();
    for (int i = 0; i < m_ioTable->rowCount() && i < m_ios.size(); ++i) {
        const FGDIO &io = m_ios[i];
        QString t = (io.isInput ? "input " : "output ") + io.name + " " + io.type + " " + io.description;
        m_ioTable->setRowHidden(i, !f.isEmpty() && !t.toLower().contains(f));
    }
    bool moveOk = f.isEmpty();
    m_ioUp->setEnabled(moveOk);
    m_ioDown->setEnabled(moveOk);
    m_ioUp->setToolTip(moveOk ? QString() : "Clear the search to reorder inputs and outputs.");
    m_ioDown->setToolTip(moveOk ? QString() : "Clear the search to reorder inputs and outputs.");
}

void EntityEditor::refreshInherited() {
    m_inheritedTable->setRowCount(0);
    if (!m_index) {
        m_inheritedInfo->setText("Inherited properties are not available here.");
        return;
    }
    FGDEntity tmp;
    tmp.className = m_className->text().trimmed();
    tmp.bases = currentBases();
    tmp.keyvalues = m_keyvalues;
    tmp.ios = m_ios;
    QStringList missing;
    for (const QString &b : tmp.bases)
        if (!m_index->contains(b)) missing << b;
    QList<FGDClassIndex::Item> items = m_index->inherited(tmp);
    for (int i = 0; i < m_baseList->count(); ++i) markBaseItem(m_baseList->item(i));
    QString info = QString("%1 definitions inherited through base(). Overridden definitions are shown in gray italics.")
        .arg(items.size());
    if (!missing.isEmpty()) info += " Not found: " + missing.join(", ") + ".";
    m_inheritedInfo->setText(info);
    for (const auto &it : items) {
        int row = m_inheritedTable->rowCount();
        m_inheritedTable->insertRow(row);
        QString kind = it.isIO ? (it.io.isInput ? "Input" : "Output") : "Keyvalue";
        QString from = it.from;
        if (m_index->isExternal(it.from)) from += " (included)";
        QString note;
        if (it.overridden) note = "overridden by " + it.overriddenBy;
        QStringList cells = { kind, it.name(), it.isIO ? it.io.type : it.kv.type, it.isIO ? QString() : it.kv.defaultValue, from, note };
        for (int c = 0; c < cells.size(); ++c) {
            QTableWidgetItem *cell = new QTableWidgetItem(cells[c]);
            if (it.overridden) {
                QFont f = cell->font();
                f.setItalic(true);
                cell->setFont(f);
                cell->setForeground(Qt::gray);
            }
            m_inheritedTable->setItem(row, c, cell);
        }
    }
    m_inheritedTable->resizeColumnsToContents();
}

void EntityEditor::addBase() {
    QString name = m_baseCombo->currentText().trimmed();
    if (name.isEmpty()) return;
    for (int i = 0; i < m_baseList->count(); ++i)
        if (m_baseList->item(i)->text() == name) return;
    QListWidgetItem *item = new QListWidgetItem(name);
    m_baseList->addItem(item);
    markBaseItem(item);
    m_baseCombo->setCurrentIndex(-1);
    m_baseCombo->clearEditText();
    refreshInherited();
}

void EntityEditor::addBasePreset(const QString &name) {
    for (int i = 0; i < m_baseList->count(); ++i)
        if (m_baseList->item(i)->text() == name) return;
    QListWidgetItem *item = new QListWidgetItem(name);
    m_baseList->addItem(item);
    markBaseItem(item);
    refreshInherited();
}

void EntityEditor::removeBase() {
    int row = m_baseList->currentRow();
    if (row >= 0) delete m_baseList->takeItem(row);
    refreshInherited();
}

void EntityEditor::addKeyvalue() {
    KeyvalueEditor dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        FGDKeyvalue kv = dlg.getKeyvalue();
        if (kv.name.isEmpty()) return;
        m_keyvalues.append(kv);
        refreshKvTable(m_keyvalues.size() - 1);
        refreshInherited();
    }
}

void EntityEditor::editKeyvalue() {
    int row = m_kvTable->currentRow();
    if (row < 0 || row >= m_keyvalues.size()) return;
    KeyvalueEditor dlg(this, m_keyvalues[row]);
    if (dlg.exec() == QDialog::Accepted) {
        m_keyvalues[row] = dlg.getKeyvalue();
        refreshKvTable(row);
        refreshInherited();
    }
}

void EntityEditor::duplicateKeyvalue() {
    int row = m_kvTable->currentRow();
    if (row < 0 || row >= m_keyvalues.size()) return;
    FGDKeyvalue copy = m_keyvalues[row];
    copy.name += "_copy";
    copy.line = 0;
    m_keyvalues.insert(row + 1, copy);
    refreshKvTable(row + 1);
    refreshInherited();
}

void EntityEditor::removeKeyvalue() {
    int row = m_kvTable->currentRow();
    if (row < 0 || row >= m_keyvalues.size()) return;
    m_keyvalues.removeAt(row);
    refreshKvTable(qMin(row, m_keyvalues.size() - 1));
    refreshInherited();
}

void EntityEditor::moveKeyvalue(int delta) {
    int row = m_kvTable->currentRow();
    int to = row + delta;
    if (row < 0 || to < 0 || to >= m_keyvalues.size()) return;
    m_keyvalues.swapItemsAt(row, to);
    refreshKvTable(to);
}

void EntityEditor::addIO() {
    IOEditor dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        FGDIO io = dlg.getIO();
        if (io.name.isEmpty()) return;
        m_ios.append(io);
        refreshIoTable(m_ios.size() - 1);
        refreshInherited();
    }
}

void EntityEditor::editIO() {
    int row = m_ioTable->currentRow();
    if (row < 0 || row >= m_ios.size()) return;
    IOEditor dlg(this, m_ios[row]);
    if (dlg.exec() == QDialog::Accepted) {
        m_ios[row] = dlg.getIO();
        refreshIoTable(row);
        refreshInherited();
    }
}

void EntityEditor::duplicateIO() {
    int row = m_ioTable->currentRow();
    if (row < 0 || row >= m_ios.size()) return;
    FGDIO copy = m_ios[row];
    copy.name += "_copy";
    copy.line = 0;
    m_ios.insert(row + 1, copy);
    refreshIoTable(row + 1);
    refreshInherited();
}

void EntityEditor::removeIO() {
    int row = m_ioTable->currentRow();
    if (row < 0 || row >= m_ios.size()) return;
    m_ios.removeAt(row);
    refreshIoTable(qMin(row, m_ios.size() - 1));
    refreshInherited();
}

void EntityEditor::moveIO(int delta) {
    int row = m_ioTable->currentRow();
    int to = row + delta;
    if (row < 0 || to < 0 || to >= m_ios.size()) return;
    m_ios.swapItemsAt(row, to);
    refreshIoTable(to);
}

void EntityEditor::addIoPreset(const QList<FGDIO> &items) {
    int added = 0;
    for (const FGDIO &io : items) {
        bool exists = false;
        for (const FGDIO &have : m_ios)
            if (have.isInput == io.isInput && have.name == io.name) { exists = true; break; }
        if (exists) continue;
        m_ios.append(io);
        ++added;
    }
    refreshIoTable(m_ios.isEmpty() ? -1 : m_ios.size() - 1);
    refreshInherited();
    if (added == 0)
        QMessageBox::information(this, "Presets", "All of these inputs and outputs already exist in this entity.");
}

void EntityEditor::accept() {
    for (const QString &line : rawHelperLines()) {
        FGDHelper h;
        if (!parseHelperLine(line, h)) {
            QMessageBox::warning(this, "Other helpers",
                "This line is not a valid helper, use the form name(arguments) or name:\n\n" + line);
            return;
        }
    }
    QSettings settings("FGDBuilder", "FGDBuilder");
    settings.setValue("entityEditorGeometry", saveGeometry());
    QDialog::accept();
}

FGDEntity EntityEditor::getEntity() const {
    FGDEntity e;
    EntityClassType ct;
    if (classTypeFromName(m_classType->currentText(), ct)) e.classType = ct;
    e.className   = m_className->text().trimmed();
    QString desc = m_description->toPlainText();
    e.description = (desc.trimmed() == m_original.description.trimmed()) ? m_original.description : desc.trimmed();
    e.url         = m_url->text().trimmed();
    e.color       = m_color->text().trimmed();
    e.size        = m_size->text().trimmed();
    e.halfGridSnap = m_halfGridSnap->isChecked();
    e.line        = m_original.line;
    for (const HelperRow &row : m_helperRows) {
        if (!row.check->isChecked()) continue;
        FGDHelper h;
        h.name = row.name;
        h.args = helperArgs(row.edit->text(), row.quoted);
        e.helpers.append(h);
    }
    for (int i = 0; i < m_flagHelpers.size(); ++i) {
        if (!m_flagHelpers[i]->isChecked()) continue;
        FGDHelper h;
        h.name = FLAG_HELPER_NAMES[i];
        e.helpers.append(h);
    }
    for (const QString &line : rawHelperLines()) {
        FGDHelper h;
        if (parseHelperLine(line, h)) e.helpers.append(h);
    }
    e.bases = currentBases();
    e.keyvalues = m_keyvalues;
    e.ios       = m_ios;
    return e;
}
