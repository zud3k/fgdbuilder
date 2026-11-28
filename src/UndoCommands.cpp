#include "UndoCommands.h"

EntityReplaceCommand::EntityReplaceCommand(const UndoContext &ctx, const QList<int> &indexes, const QList<FGDEntity> &before,
                                           const QList<FGDEntity> &after, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_indexes(indexes), m_before(before), m_after(after) {}

void EntityReplaceCommand::undo() {
    for (int i = 0; i < m_indexes.size(); ++i) m_ctx.project->entities[m_indexes[i]] = m_before[i];
    m_ctx.changed();
}

void EntityReplaceCommand::redo() {
    for (int i = 0; i < m_indexes.size(); ++i) m_ctx.project->entities[m_indexes[i]] = m_after[i];
    m_ctx.changed();
}

EntityInsertCommand::EntityInsertCommand(const UndoContext &ctx, int index, const QList<FGDEntity> &items,
                                         const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_index(index), m_items(items) {}

void EntityInsertCommand::undo() {
    for (int i = 0; i < m_items.size(); ++i) m_ctx.project->entities.removeAt(m_index);
    m_ctx.changed();
}

void EntityInsertCommand::redo() {
    for (int i = 0; i < m_items.size(); ++i) m_ctx.project->entities.insert(m_index + i, m_items[i]);
    m_ctx.changed();
}

EntityRemoveCommand::EntityRemoveCommand(const UndoContext &ctx, const QList<int> &ascendingIndexes, const QList<FGDEntity> &items,
                                         const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_indexes(ascendingIndexes), m_items(items) {}

void EntityRemoveCommand::undo() {
    for (int i = 0; i < m_indexes.size(); ++i) m_ctx.project->entities.insert(m_indexes[i], m_items[i]);
    m_ctx.changed();
}

void EntityRemoveCommand::redo() {
    for (int i = m_indexes.size() - 1; i >= 0; --i) m_ctx.project->entities.removeAt(m_indexes[i]);
    m_ctx.changed();
}

EntityMoveCommand::EntityMoveCommand(const UndoContext &ctx, int from, int to, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_from(from), m_to(to) {}

void EntityMoveCommand::undo() {
    m_ctx.project->entities.move(m_to, m_from);
    m_ctx.changed();
}

void EntityMoveCommand::redo() {
    m_ctx.project->entities.move(m_from, m_to);
    m_ctx.changed();
}

EntityPermuteCommand::EntityPermuteCommand(const UndoContext &ctx, const QVector<int> &order, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_order(order) {}

void EntityPermuteCommand::undo() {
    QList<FGDEntity> restored = m_ctx.project->entities;
    for (int i = 0; i < m_order.size(); ++i) restored[m_order[i]] = m_ctx.project->entities[i];
    m_ctx.project->entities = restored;
    m_ctx.changed();
}

void EntityPermuteCommand::redo() {
    QList<FGDEntity> reordered;
    reordered.reserve(m_order.size());
    for (int i = 0; i < m_order.size(); ++i) reordered.append(m_ctx.project->entities[m_order[i]]);
    m_ctx.project->entities = reordered;
    m_ctx.changed();
}

SettingsCommand::SettingsCommand(const UndoContext &ctx, const FGDProjectSettings &before, const FGDProjectSettings &after,
                                 const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_ctx(ctx), m_before(before), m_after(after) {}

void SettingsCommand::undo() {
    setProjectSettings(*m_ctx.project, m_before);
    m_ctx.changed();
}

void SettingsCommand::redo() {
    setProjectSettings(*m_ctx.project, m_after);
    m_ctx.changed();
}
