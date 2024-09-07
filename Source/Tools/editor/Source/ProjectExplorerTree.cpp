#include "ProjectExplorerTree.hpp"
#include "qfilesystemmodel.h"
#include "EditorMainWindow.hpp"
#include <QLineEdit>
#include "qdebug.h"
#include "qsortfilterproxymodel.h"

ProjectExplorerTree::ProjectExplorerTree(QWidget* parent, QLineEdit* pLineEdit, EditorMainWindow* pMainWindow, ProjectFile projectFile)
    : QTreeView(parent)
{
    mMainWindow = pMainWindow;
    mLineEdit = pLineEdit;

    connect(mLineEdit, &QLineEdit::textChanged, this, &ProjectExplorerTree::FilterFiles);

    mFileModel = new QFileSystemModel(this);

    mFileModel->setRootPath(projectFile.rootDir);
    mFileModel->setNameFilterDisables(false);
    mFileModel->setNameFilters(QStringList("path.json"));
    mFileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);

    setModel(mFileModel);

    setRootIndex(mFileModel->index(projectFile.rootDir));

    // Only show the name column
    for (int i = 1; i < mFileModel->columnCount(); i++)
    {
        hideColumn(i);
    }
}

void ProjectExplorerTree::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QTreeView::selectionChanged(selected, deselected);

    if (selected.isEmpty())
    {
        return;
    }

    auto pathFileDir = mFileModel->filePath(selected.indexes().first());
    QFileInfo file(pathFileDir);
    if (file.exists() && !file.isDir() && file.fileName() == "path.json")
    {
        mMainWindow->OpenPath(pathFileDir, false);
    }
}

void ProjectExplorerTree::FilterFiles(const QString& filter)
{
    if (filter.isEmpty())
    {
        mFileModel->setNameFilters(QStringList("path.json"));
        mFileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);
        collapseAll();
        return;
    }

    expandAll();
    mFileModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files);
    mFileModel->setNameFilters(QStringList() << (filter + "*"));
    keyboardSearch(filter);
}