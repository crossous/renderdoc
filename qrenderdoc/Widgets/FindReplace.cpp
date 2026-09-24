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

#include "FindReplace.h"
#include <QKeyEvent>
#include <QLineEdit>
#include "Code/QRDUtils.h"
#include "Code/ScintillaSyntax.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "toolwindowmanager/ToolWindowManagerArea.h"
#include "ui_FindReplace.h"

FindReplace::FindReplace(QList<ScintillaEdit *> &scintillas, QWidget *parent)
    : QFrame(parent), ui(new Ui::FindReplace), m_Scintillas(scintillas)
{
  ui->setupUi(this);

  ui->searchContext->setCurrentIndex(FindReplace::File);

  // default to just find
  setReplaceMode(false);
  setDirection(FindReplace::Down);

  RDLineEdit *edit = new RDLineEdit(this);
  ui->findText->setLineEdit(edit);

  ui->findText->setAutoCompletion(false);
  ui->replaceText->setAutoCompletion(false);

  QObject::connect(edit, &RDLineEdit::keyPress, [this](QKeyEvent *event) {
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
      SearchDirection dir = m_direction;

      if(event->modifiers() & Qt::ShiftModifier)
        m_direction = SearchDirection::Up;
      else
        m_direction = SearchDirection::Down;

      addHistory(ui->findText);
      performFind();

      m_direction = dir;
    }
  });
  QObject::connect(ui->replaceText->lineEdit(), &QLineEdit::returnPressed, this,
                   &FindReplace::on_replace_clicked);
}

FindReplace::~FindReplace()
{
  delete ui;
}

bool FindReplace::replaceMode()
{
  return ui->replaceMode->isChecked();
}

FindReplace::SearchContext FindReplace::context()
{
  return (FindReplace::SearchContext)ui->searchContext->currentIndex();
}

FindReplace::SearchDirection FindReplace::direction()
{
  return m_direction;
}

bool FindReplace::matchCase()
{
  return ui->matchCase->isChecked();
}

bool FindReplace::matchWord()
{
  return ui->matchWord->isChecked();
}

bool FindReplace::regexp()
{
  return ui->regexp->isChecked();
}

void FindReplace::configureFindIndicator(ScintillaEdit *editor, sptr_t indic)
{
  editor->indicSetFore(indic, SCINTILLA_COLOUR(200, 200, 64));
  editor->indicSetStyle(indic, INDIC_FULLBOX);
  editor->indicSetAlpha(indic, 50);
  editor->indicSetOutlineAlpha(indic, 80);
}

void FindReplace::raiseOrShow()
{
  if(m_DockManager)
  {
    if(isVisible())
    {
      ToolWindowManager::raiseToolWindow(this);
    }
    else
    {
      m_DockManager->moveToolWindow(
          this, ToolWindowManager::AreaReference(ToolWindowManager::NewFloatingArea));
      m_DockManager->setToolWindowProperties(this, ToolWindowManager::HideOnClose);
    }
    m_DockManager->areaOf(this)->parentWidget()->activateWindow();
  }
  takeFocus();
}

void FindReplace::setFindAllResultsDisplay(ScintillaEdit *findResults)
{
  m_FindResultsDisplay = findResults;

  configureFindIndicator(m_FindResultsDisplay, INDICATOR_FINDRESULTHIGHLIGHT);

  QColor highlightColor = palette().color(QPalette::Highlight).toRgb();

  m_FindResultsDisplay->indicSetFore(
      INDICATOR_FINDALLCURRENTRESULT,
      SCINTILLA_COLOUR(highlightColor.red(), highlightColor.green(), highlightColor.blue()));
  m_FindResultsDisplay->indicSetStyle(INDICATOR_FINDALLCURRENTRESULT, INDIC_FULLBOX);
  m_FindResultsDisplay->indicSetAlpha(INDICATOR_FINDALLCURRENTRESULT, 120);
  m_FindResultsDisplay->indicSetOutlineAlpha(INDICATOR_FINDALLCURRENTRESULT, 180);

  QObject::connect(m_FindResultsDisplay, &ScintillaEdit::doubleClick, this,
                   &FindReplace::resultsDoubleClick);
}

void FindReplace::setFindText(QString text)
{
  ui->findText->setCurrentText(text);
}

QString FindReplace::findText()
{
  return ui->findText->currentText();
}

void FindReplace::setReplaceText(QString text)
{
  ui->replaceText->setCurrentText(text);
}

QString FindReplace::replaceText()
{
  return ui->replaceText->currentText();
}

void FindReplace::allowUserModeChange(bool allow)
{
  ui->modeChangeFrame->setVisible(allow);
}

void FindReplace::allowFindAll(bool allow)
{
  ui->searchContextLabel->setVisible(allow);
  ui->searchContext->setVisible(allow);
  if(!allow)
    ui->searchContext->setCurrentIndex(FindReplace::File);
}

void FindReplace::setReplaceMode(bool replacing)
{
  ui->replaceLabel->setVisible(replacing);
  ui->replaceText->setVisible(replacing);
  ui->replace->setVisible(replacing);
  ui->replaceAll->setVisible(replacing);

  ui->findMode->setChecked(!replacing);
  ui->replaceMode->setChecked(replacing);

  setWindowTitle(replacing ? tr("Find && Replace") : tr("Find"));
}

void FindReplace::setDirection(SearchDirection dir)
{
  m_direction = dir;
}

void FindReplace::takeFocus()
{
  ui->findText->setFocus();
  ui->findText->lineEdit()->selectAll();
}

void FindReplace::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_F3)
  {
    SearchDirection dir = m_direction;

    if(event->modifiers() & Qt::ShiftModifier)
      m_direction = SearchDirection::Up;
    else
      m_direction = SearchDirection::Down;

    performFind();

    m_direction = dir;
  }
  else
  {
    if(event->key() == Qt::Key_Escape)
    {
      // if we are in a floating area, hide on escape
      ToolWindowManagerArea *area = m_DockManager->areaOf(this);

      if(area && m_DockManager->isFloating(this))
      {
        m_DockManager->hideToolWindow(this);
      }
    }

    emit keyPress(event);
  }
}

void FindReplace::addHistory(QComboBox *combo)
{
  QString text = combo->currentText();

  for(int i = 0; i < combo->count(); i++)
  {
    if(combo->itemText(i) == text)
    {
      // remove the item so we can bump it up to the top of the list
      combo->removeItem(i);
      break;
    }
  }

  combo->insertItem(0, text);
  combo->setCurrentText(text);
}

void FindReplace::on_findPrev_clicked()
{
  setDirection(FindReplace::Up);
  addHistory(ui->findText);
  performFind();
}

void FindReplace::on_find_clicked()
{
  setDirection(FindReplace::Down);
  addHistory(ui->findText);
  performFind();
}

void FindReplace::on_findAll_clicked()
{
  addHistory(ui->findText);
  performFindAll();
}

void FindReplace::on_replace_clicked()
{
  addHistory(ui->findText);
  addHistory(ui->replaceText);
  performReplace();
}

void FindReplace::on_replaceAll_clicked()
{
  addHistory(ui->findText);
  addHistory(ui->replaceText);
  performReplaceAll();
}

void FindReplace::on_findMode_clicked()
{
  setReplaceMode(false);
}

void FindReplace::on_replaceMode_clicked()
{
  setReplaceMode(true);
}

ScintillaEdit *FindReplace::currentScintilla()
{
  ScintillaEdit *cur = qobject_cast<ScintillaEdit *>(QApplication::focusWidget());

  if(cur == NULL)
  {
    for(ScintillaEdit *s : m_Scintillas)
    {
      if(s->isVisible())
      {
        cur = s;
        break;
      }
    }
  }

  return cur;
}

ScintillaEdit *FindReplace::nextScintilla(ScintillaEdit *cur)
{
  for(int i = 0; i < m_Scintillas.count(); i++)
  {
    if(m_Scintillas[i] == cur)
    {
      if(i + 1 < m_Scintillas.count())
        return m_Scintillas[i + 1];

      return m_Scintillas[0];
    }
  }

  if(!m_Scintillas.isEmpty())
    return m_Scintillas[0];

  return NULL;
}

void FindReplace::clearFindState()
{
  m_FindState = FindState();
}

void FindReplace::performFind(bool down)
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = findText();

  sptr_t flags = 0;

  if(matchCase())
    flags |= SCFIND_MATCHCASE;
  if(matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext ctx = context();

  QString findHash = QFormatStr("%1%2%3%4").arg(find).arg(flags).arg((int)ctx).arg(down);

  if(findHash != m_FindState.hash)
  {
    m_FindState.hash = findHash;
    m_FindState.start = 0;
    m_FindState.end = cur->length();
    m_FindState.offset = cur->currentPos();
    if(down && cur->selectionStart() == m_FindState.offset &&
       cur->selectionEnd() - m_FindState.offset == find.length())
      m_FindState.offset += find.length();
  }

  int start = m_FindState.start + m_FindState.offset;
  int end = m_FindState.end;

  if(!down)
    end = m_FindState.start;

  QPair<int, int> result = cur->findText(flags, find.toUtf8().data(), start, end);

  m_FindState.prevResult = result;

  if(result.first == -1)
  {
    sptr_t maxOffset = down ? 0 : m_FindState.end;

    // if we're at offset 0 searching down, there are no results. Same for offset max and
    // searching up
    if(m_FindState.offset == maxOffset)
      return;

    // otherwise, we can wrap the search around

    if(ctx == FindReplace::AllFiles)
    {
      cur = nextScintilla(cur);
      ToolWindowManager::raiseToolWindow(cur);
      cur->activateWindow();
      cur->QWidget::setFocus();
    }

    m_FindState.offset = maxOffset;

    start = m_FindState.start + m_FindState.offset;
    end = m_FindState.end;

    if(!down)
      end = m_FindState.start;

    result = cur->findText(flags, find.toUtf8().data(), start, end);

    m_FindState.prevResult = result;

    if(result.first == -1)
      return;
  }

  cur->setSelection(result.first, result.second);

  EnsureLineScrolled(cur, cur->lineFromPosition(result.first));

  if(down)
    m_FindState.offset = result.second - m_FindState.start;
  else
    m_FindState.offset = result.first - m_FindState.start;
}

void FindReplace::handleEditorKeypress(ScintillaEdit *editor, QKeyEvent *ev)
{
  bool readOnly = editor->readOnly();

  if((ev->modifiers() & Qt::ControlModifier) && (ev->key() == Qt::Key_F || ev->key() == Qt::Key_F3))
  {
    // if there's a selection, fill the find prompt with that
    if(!editor->getSelText().isEmpty())
    {
      setFindText(QString::fromUtf8(editor->getSelText()));
    }
    else
    {
      // otherwise pick the word under the cursor, if there is one
      sptr_t scintillaPos = editor->currentPos();

      sptr_t start = editor->wordStartPosition(scintillaPos, true);
      sptr_t end = editor->wordEndPosition(scintillaPos, true);

      QByteArray text = editor->textRange(start, end);

      if(!text.isEmpty())
        setFindText(QString::fromUtf8(text));
    }
  }

  if(ev->key() == Qt::Key_F && (ev->modifiers() & Qt::ControlModifier))
  {
    setReplaceMode(false);
    raiseOrShow();
  }

  if(ev->key() == Qt::Key_F3)
  {
    setReplaceMode(false);
    performFind((ev->modifiers() & Qt::ShiftModifier) == 0);
  }

  if(!readOnly && ev->key() == Qt::Key_H && (ev->modifiers() & Qt::ControlModifier))
  {
    setReplaceMode(true);
    raiseOrShow();
  }
}

void FindReplace::performFind()
{
  performFind(direction() == FindReplace::Down);
}

void FindReplace::performFindAll()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  clearFindState();
  m_FindAllResults.clear();

  QString find = findText();

  sptr_t flags = 0;

  m_FindAllResultString = tr("Find all \"%1\"").arg(find);

  if(matchCase())
  {
    flags |= SCFIND_MATCHCASE;
    m_FindAllResultString += tr(", Match case");
  }

  if(matchWord())
  {
    flags |= SCFIND_WHOLEWORD;
    m_FindAllResultString += tr(", Match whole word");
  }

  if(regexp())
  {
    flags |= SCFIND_REGEXP | SCFIND_POSIX;
    m_FindAllResultString += tr(", with Regular Expressions");
  }

  FindReplace::SearchContext ctx = context();

  if(ctx == FindReplace::File)
    m_FindAllResultString += tr(", in current file\n");
  else
    m_FindAllResultString += tr(", in all files\n");

  QList<ScintillaEdit *> scintillas = m_Scintillas;

  if(ctx == FindReplace::File)
    scintillas = {cur};

  QList<QPair<int, int>> resultList;

  QByteArray findUtf8 = find.toUtf8();

  if(findUtf8.isEmpty())
    return;

  for(ScintillaEdit *s : scintillas)
  {
    sptr_t start = 0;
    sptr_t end = s->length();

    if(m_FindIndicator >= 0)
    {
      s->setIndicatorCurrent(m_FindIndicator);
      s->indicatorClearRange(start, end);
    }

    QPair<int, int> result;

    do
    {
      result = s->findText(flags, findUtf8.data(), start, end);

      if(result.first >= 0)
      {
        int line = s->lineFromPosition(result.first);
        sptr_t lineStart = s->positionFromLine(line);
        sptr_t lineEnd = s->lineEndPosition(line);

        if(m_FindIndicator >= 0)
          s->indicatorFillRange(result.first, result.second - result.first);

        QString lineText = QString::fromUtf8(s->textRange(lineStart, lineEnd));

        m_FindAllResultString += QFormatStr("  %1(%2): ").arg(s->windowTitle()).arg(line + 1, 4);
        int startPos = m_FindAllResultString.length();

        m_FindAllResultString += lineText;
        m_FindAllResultString += lit("\n");

        resultList.push_back(
            qMakePair(result.first - lineStart + startPos, result.second - lineStart + startPos));

        m_FindAllResults.push_back({s, result.first});
      }

      start = result.second;

    } while(result.first >= 0);
  }

  m_FindAllResultString += tr("Matching lines: %1").arg(resultList.count());

  if(m_FindResultsDisplay)
  {
    m_FindResultsDisplay->setReadOnly(false);
    m_FindResultsDisplay->setText(m_FindAllResultString.toUtf8().data());

    m_FindResultsDisplay->setIndicatorCurrent(INDICATOR_FINDALLCURRENTRESULT);
    m_FindResultsDisplay->indicatorClearRange(0, m_FindResultsDisplay->length());

    m_FindResultsDisplay->setIndicatorCurrent(INDICATOR_FINDRESULTHIGHLIGHT);

    for(QPair<int, int> r : resultList)
      m_FindResultsDisplay->indicatorFillRange(r.first, r.second - r.first);

    m_FindResultsDisplay->setReadOnly(true);

    if(m_FindResultsDisplay->isVisible())
    {
      if(m_DockManager)
        ToolWindowManager::raiseToolWindow(m_FindResultsDisplay);
    }
    else
    {
      if(m_DockManager)
      {
        m_DockManager->moveToolWindow(m_FindResultsDisplay, ToolWindowManager::AreaReference(
                                                                ToolWindowManager::BottomOf,
                                                                m_DockManager->areaOf(cur), 0.2f));
        m_DockManager->setToolWindowProperties(
            m_FindResultsDisplay,
            ToolWindowManager::HideOnClose | ToolWindowManager::DisallowFloatWindow);
      }
    }
  }
}

void FindReplace::performReplace()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = findText();

  if(find.isEmpty())
    return;

  sptr_t flags = 0;

  if(matchCase())
    flags |= SCFIND_MATCHCASE;
  if(matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext ctx = context();

  bool down = direction() == FindReplace::Down;
  QString findHash = QFormatStr("%1%2%3%4").arg(find).arg(flags).arg((int)ctx).arg(down);

  // if we didn't have a valid previous find, just do a find and bail
  if(findHash != m_FindState.hash)
  {
    performFind();
    return;
  }

  if(m_FindState.prevResult.first == -1)
    return;

  cur->setTargetRange(m_FindState.prevResult.first, m_FindState.prevResult.second);

  FindState save = m_FindState;

  QString dest = replaceText();

  // otherwise we have a valid previous find. Do the replace now
  // note this will invalidate the find state (as most user operations would), so we save/restore
  // the state
  if(regexp())
    cur->replaceTargetRE(-1, dest.toUtf8().data());
  else
    cur->replaceTarget(-1, dest.toUtf8().data());

  m_FindState = save;

  // adjust the offset if we replaced text and it went up or down in size
  m_FindState.offset += (dest.count() - find.count());

  // move to the next result
  performFind();
}

void FindReplace::performReplaceAll()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = findText();
  QString replace = replaceText();

  if(find.isEmpty())
    return;

  sptr_t flags = 0;

  if(matchCase())
    flags |= SCFIND_MATCHCASE;
  if(matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext ctx = context();

  // trash the find state for any incremental finds
  clearFindState();

  QList<ScintillaEdit *> scintillas = m_Scintillas;

  if(ctx == FindReplace::File)
    scintillas = {cur};

  int numReplacements = 0;

  for(ScintillaEdit *s : scintillas)
  {
    sptr_t start = 0;
    sptr_t end = s->length();

    QPair<int, int> result;

    QByteArray findUtf8 = find.toUtf8();
    QByteArray replaceUtf8 = replace.toUtf8();

    do
    {
      result = s->findText(flags, findUtf8.data(), start, end);

      if(result.first >= 0)
      {
        s->setTargetRange(result.first, result.second);

        if(regexp())
          s->replaceTargetRE(-1, replaceUtf8.data());
        else
          s->replaceTarget(-1, replaceUtf8.data());

        numReplacements++;
      }

      start = result.second + (replaceUtf8.count() - findUtf8.count());

    } while(result.first >= 0);
  }

  QString message;

  if(scintillas.count() > 1)
    message = tr("%1 replacements made in %2 files.").arg(numReplacements).arg(scintillas.count());
  else
    message = tr("%1 replacements made.").arg(numReplacements);

  RDDialog::information(this, tr("Replace all"), message);
}

void FindReplace::resultsDoubleClick(int position, int line)
{
  if(line >= 1 && line - 1 < m_FindAllResults.count())
  {
    m_FindResultsDisplay->setIndicatorCurrent(INDICATOR_FINDALLCURRENTRESULT);
    m_FindResultsDisplay->indicatorClearRange(0, m_FindResultsDisplay->length());

    sptr_t start = m_FindResultsDisplay->positionFromLine(line);
    sptr_t length = m_FindResultsDisplay->lineLength(line);

    m_FindResultsDisplay->indicatorFillRange(start, length);

    m_FindResultsDisplay->setSelection(position, position);

    ScintillaEdit *s = m_FindAllResults[line - 1].first;
    int resultPos = m_FindAllResults[line - 1].second;
    ToolWindowManager::raiseToolWindow(s);
    s->activateWindow();
    s->QWidget::setFocus();
    s->clearSelections();
    s->setSelection(resultPos, resultPos);
    s->scrollCaret();
  }
}
