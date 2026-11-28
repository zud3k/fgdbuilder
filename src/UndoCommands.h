#pragma once
#include "FGDData.h"
#include <QUndoCommand>
#include <QVector>
#include <functional>

struct UndoContext {
    FGDProject *project = nullptr;
    std::function<void()> changed;
};

class EntityReplaceCommand : public QUndoCommand {
public:
    EntityReplaceCommand(const UndoContext &ctx, const QList<int> &indexes, const QList<FGDEntity> &before,
                         const QList<FGDEntity> &after, const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    QList<int> m_indexes;
    QList<FGDEntity> m_before, m_after;
};

class EntityInsertCommand : public QUndoCommand {
public:
    EntityInsertCommand(const UndoContext &ctx, int index, const QList<FGDEntity> &items,
                        const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    int m_index;
    QList<FGDEntity> m_items;
};

class EntityRemoveCommand : public QUndoCommand {
public:
    EntityRemoveCommand(const UndoContext &ctx, const QList<int> &ascendingIndexes, const QList<FGDEntity> &items,
                        const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    QList<int> m_indexes;
    QList<FGDEntity> m_items;
};

class EntityMoveCommand : public QUndoCommand {
public:
    EntityMoveCommand(const UndoContext &ctx, int from, int to, const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    int m_from, m_to;
};

class EntityPermuteCommand : public QUndoCommand {
public:
    EntityPermuteCommand(const UndoContext &ctx, const QVector<int> &order, const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    QVector<int> m_order;
};

class SettingsCommand : public QUndoCommand {
public:
    SettingsCommand(const UndoContext &ctx, const FGDProjectSettings &before, const FGDProjectSettings &after,
                    const QString &text, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    UndoContext m_ctx;
    FGDProjectSettings m_before, m_after;
};
