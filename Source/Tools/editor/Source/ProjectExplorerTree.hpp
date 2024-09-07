#pragma once

#include <QTreeView>
#include "EditorMainWindow.hpp"

class ProjectExplorerTree final : public QTreeView
{
public:
    ProjectExplorerTree(QWidget* parent, class QLineEdit* pLineEdit, EditorMainWindow* pMainWindow, ProjectFile projectFile);

private:
    virtual void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    void FilterFiles(const QString& filter);

private:
    class EditorMainWindow* mMainWindow;
    class QLineEdit* mLineEdit;
    class QFileSystemModel* mFileModel;
};
