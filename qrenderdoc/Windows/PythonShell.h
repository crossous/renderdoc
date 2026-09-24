/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QDir>
#include <QFrame>
#include "Code/Interface/QRDInterface.h"
#include "scintilla/include/qt/ScintillaEdit.h"

class PythonContext;
class QTextEdit;
class RDToolTip;
class RDLabel;
class QTimer;
class QCompleter;
class FindReplace;
class QStringListModel;
class QFileSystemWatcher;
class RDTreeWidgetItem;

namespace Ui
{
class PythonShell;
}

struct CaptureContextInvoker;

class PythonShell;

class EditorWrapper : public QFrame
{
  Q_OBJECT

  PythonShell *m_PyShell;

  ScintillaEdit *m_Scintilla;
  RDLabel *m_Warning;

  QString m_Filename;
  QString m_Title;
  bool m_Modified = false;

  bool m_UIExt = false;

  void updateTitle();

public:
  EditorWrapper(PythonShell *parent);
  virtual ~EditorWrapper();

  ScintillaEdit *scintilla() { return m_Scintilla; }

  QString filename() { return m_Filename; }
  void setFilename(QString filename);

  QString title() { return m_Title; }
  void setTitle(QString title);

  void setWarning(QString text);

  bool isModified() { return m_Modified; }
  void markModified(bool modified);

  bool isUIExtension() { return m_UIExt; }
  void setUIExtension(bool uiext) { m_UIExt = uiext; }

public slots:
  bool checkAllowClose();
};

class PythonShell : public QFrame, public IPythonShell
{
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit PythonShell(ICaptureContext &ctx, QWidget *parent = 0);

  ~PythonShell();

  // for UI-forwarding helper classes
  PythonContext *GetScriptContext();

  // IPythonShell
  QWidget *Widget() override { return this; }
  bool CheckUnsavedChanges() override;
  bool LoadScriptFromFilename(rdcstr filename) override;
  void CreateNewScriptEditor(rdcstr name, rdcstr text) override;
  rdcstr GetScriptText() override;
  void RunScript() override;
  void AttachDebugger(const rdcstr &extensionName) override;

  void SetExtensionOutputFilter(const rdcstr &extensionName) override;
  void SetScriptOutputFilter() override;
  void RemoveOutputFilter() override;
  void ShowOutput() override;
  void ShowREPL() override;
  void ShowHelp() override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

  bool saveEditorAs(EditorWrapper *editor);
  bool saveEditor(EditorWrapper *editor, QString filename);

  void removeEditor(EditorWrapper *editor);

private slots:
  // automatic slots
  void on_findReplace_clicked();
  void on_execute_clicked();
  void on_clear_clicked();
  void on_newScript_clicked();
  void on_openScript_clicked();
  void on_saveScript_clicked();
  void on_saveAsScript_clicked();

  void on_runScript_clicked();
  void on_debugAttach_clicked();
  void on_abortRun_clicked();
  void on_outputContext_currentIndexChanged(int idx);

  void on_projectExplorer_itemActivated(RDTreeWidgetItem *item, int column);

  bool checkAllowClose();

  // manual slots
  void interactive_keypress(QKeyEvent *e);
  void traceLine(const QString &file, int line);
  void exception(const QString &extension, const QString &type, const QString &value, int finalLine,
                 QList<QString> frames);
  void textOutput(const QString &extension, bool isStdError, const QString &output);
  void extensionLoaded(const QString &extension);
  void editor_contextMenu(const QPoint &pos);
  void projectExplorer_contextMenu(const QPoint &pos);
  void editorTab_Changed(int index);
  void doSyntaxCheck();
  void createExtension_clicked();

  void openFileModified(const QString &path);
  void updateExtensionProjects();
  void editorTab_Menu(const QPoint &pos);

private:
  Ui::PythonShell *ui;
  ICaptureContext &m_Ctx;

  ScintillaEdit *runningScriptEditor = NULL;

  RDToolTip *m_ToolTip;
  bool m_FuncTip = false;
  QWidget *m_FuncTipWidget = NULL;
  intptr_t m_FuncTipLine = 0;
  bool m_ContextMenuVisible = false;

  bool m_IgnoreRecovered = false;

  RDTreeWidgetItem *m_UIExtensions, *m_Examples, *m_RecentFiles, *m_NewExtension;

  QFileSystemWatcher *m_Watcher = NULL;

  QList<QString> m_ModifiedExtensions;
  bool m_DebuggerAttached = false;

  QTimer *m_SyntaxCheckTimer;

  QCompleter *m_InteractiveCompleter = NULL;
  QStringListModel *m_InteractiveCompletionModel;
  int m_InteractiveCompletionPrefix = 0;

  RDToolTip *m_CompletionTip;
  QList<QPair<QString, QString>> m_CompletionTipList;
  sptr_t m_CurrentCompletionTip = -1;
  QTimer *m_CompletionTipTimer;

  static const int CURRENT_MARKER = 0;
  static const int STYLE_ERROR = 100;

  bool m_CurLineDirty = false;
  int m_CurLine = 0;
  QTimer *m_CurLineTimer = NULL;

  PythonContext *interactiveContext = NULL, *scriptContext = NULL, *completionContext = NULL,
                *helpContext = NULL;

  QList<QString> history;
  int historyidx = -1;

  QString m_storedLines;

  struct ScriptOutputLine
  {
    QString extension;
    QString text;
  };

  rdcarray<QString> loadedExtensions;
  rdcarray<ScriptOutputLine> scriptOutputLines;
  size_t lastDisplayedLine = 0;

  QList<ScintillaEdit *> m_Scintillas;
  QList<EditorWrapper *> m_Editors;

  FindReplace *m_FindReplace;
  ScintillaEdit *m_FindResults = NULL;

  void setupTabs();

  EditorWrapper *curEditor();
  EditorWrapper *makeEditor(rdcstr filename, rdcstr text);

  void updateEditorCloseButton();
  void updateNonDebugWarning();
  void updateButtonStates();

  void addRecentFile(rdcstr filename);
  void updateRecentFiles(bool added);

  void addExtensionDirItems(RDTreeWidgetItem *root, QDir dir);

  bool eventFilter(QObject *watched, QEvent *event) override;

  void updateScriptOutput(bool fullRefresh);

  PythonContext *newContext();

  void doAutocomplete(ScintillaEdit *editor);
  void doFunccomplete(ScintillaEdit *editor);

  void hideFunccompleteTooltip();
  void updateCompletionTip();

  void selectedHelp(QString word);
  void refreshCurrentHelp();

  QString scriptHeader();
  void appendText(QTextEdit *output, const QString &text);
  void enableButtons(bool enable);
};
