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

#include <QFrame>
class ScintillaEdit;
class ToolWindowManager;

// from Scintilla
typedef intptr_t sptr_t;

namespace Ui
{
class FindReplace;
}

class QComboBox;

class FindReplace : public QFrame
{
  Q_OBJECT

public:
  explicit FindReplace(QList<ScintillaEdit *> &scintillas, QWidget *parent = 0);
  ~FindReplace();

  enum SearchContext
  {
    File,
    AllFiles,
  };

  enum SearchDirection
  {
    Up,
    Down,
  };

  bool replaceMode();

  SearchContext context();
  SearchDirection direction();
  bool matchCase();
  bool matchWord();
  bool regexp();

  void setDockManager(ToolWindowManager *manager) { m_DockManager = manager; }

  void setFindAllResultsDisplay(ScintillaEdit *findResults);
  void setFindIndicator(sptr_t indic) { m_FindIndicator = indic; }
  void configureFindIndicator(ScintillaEdit *editor)
  {
    configureFindIndicator(editor, m_FindIndicator);
  }

  void raiseOrShow();

  void setFindText(QString text);
  QString findText();
  void setReplaceText(QString text);
  QString replaceText();

  void performFind(bool down);

  QString findAllResultString() { return m_FindAllResultString; }

  void handleEditorKeypress(ScintillaEdit *editor, QKeyEvent *ev);

public slots:
  void allowUserModeChange(bool allow);
  void allowFindAll(bool allow);
  void setReplaceMode(bool replacing);
  void setDirection(SearchDirection dir);
  void takeFocus();
  void clearFindState();
  void resultsDoubleClick(int position, int line);

signals:
  void keyPress(QKeyEvent *e);

private slots:
  // automatic slots
  void on_findPrev_clicked();
  void on_find_clicked();
  void on_findAll_clicked();
  void on_replace_clicked();
  void on_replaceAll_clicked();
  void on_findMode_clicked();
  void on_replaceMode_clicked();

private:
  void keyPressEvent(QKeyEvent *event) override;

  Ui::FindReplace *ui;

  ToolWindowManager *m_DockManager = NULL;

  QList<ScintillaEdit *> &m_Scintillas;
  ScintillaEdit *m_FindResultsDisplay = NULL;

  sptr_t m_FindIndicator = -1;

  static const int INDICATOR_FINDRESULTHIGHLIGHT = 0;
  static const int INDICATOR_FINDALLCURRENTRESULT = 1;

  QString m_FindAllResultString;
  QList<QPair<ScintillaEdit *, int>> m_FindAllResults;

  SearchDirection m_direction;

  struct FindState
  {
    // hash identifies when the search has changed
    QString hash;

    // the range identified when the search first occurred (for incremental find/replace)
    sptr_t start = 0;
    sptr_t end = 0;

    // the current offset where to search from next time, relative to above range
    sptr_t offset = 0;

    // the last result
    QPair<int, int> prevResult;
  } m_FindState;

  void addHistory(QComboBox *combo);

  ScintillaEdit *currentScintilla();
  ScintillaEdit *nextScintilla(ScintillaEdit *cur);

  void configureFindIndicator(ScintillaEdit *editor, sptr_t indic);

  void performFind();
  void performFindAll();
  void performReplace();
  void performReplaceAll();
};
