/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Baldur Karlsson
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

#include "ApiMonitorWindow.h"
#include <QFile>
#include <QFileDialog>
#include <QScrollBar>
#include <QTextStream>
#include "Code/QRDUtils.h"
#include "Code/ScintillaSyntax.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_ApiMonitorWindow.h"

ApiMonitorWindow::ApiMonitorWindow(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ApiMonitorWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->logOutput->setFont(Formatter::FixedFont());

  // Create the Scintilla script editor (same setup as PythonShell)
  m_ScriptEditor = new ScintillaEdit(this);

  m_ScriptEditor->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());

  m_ScriptEditor->setMarginLeft(4.0);
  m_ScriptEditor->setMarginWidthN(0, 32.0);
  m_ScriptEditor->setMarginWidthN(1, 0.0);
  m_ScriptEditor->setMarginWidthN(2, 16.0);
  m_ScriptEditor->setObjectName(lit("monitorScriptEditor"));

  m_ScriptEditor->usePopUp(SC_POPUP_NEVER);

  ConfigureSyntax(m_ScriptEditor, SCLEX_PYTHON);

  m_ScriptEditor->setTabWidth(4);
  m_ScriptEditor->setScrollWidth(1);
  m_ScriptEditor->setScrollWidthTracking(true);
  m_ScriptEditor->colourise(0, -1);

  // Insert editor at the top of the splitter (before log output)
  ui->splitter->insertWidget(0, m_ScriptEditor);
  int h = ui->splitter->rect().height();
  if(h <= 0)
    h = 400;
  ui->splitter->setSizes({h * 2 / 3, h / 3});

  // Start with Apply/Unload disabled (no connection)
  ui->applyScript->setEnabled(false);
  ui->unloadScript->setEnabled(false);

  // Default template
  m_ScriptEditor->setText(
      "# API Monitor Script\n"
      "# Use monitor.on(api_name, callback) to register hooks.\n"
      "# Available hooks: on_draw, on_draw_indexed, on_dispatch, on_copy_tex,\n"
      "#   on_copy_buf, on_copy_res, on_barrier, on_create_resource,\n"
      "#   on_create_buffer, on_create_texture1d, on_create_texture2d, on_create_texture3d\n"
      "# Use monitor.log(msg) to send messages to this window.\n"
      "# Use monitor.trigger_capture(n) to trigger a RenderDoc capture.\n"
      "\n"
      "import monitor\n"
      "\n");
}

ApiMonitorWindow::~ApiMonitorWindow()
{
  m_Ctx.BuiltinWindowClosed(this);

  if(m_LiveCapture)
  {
    disconnect(m_LiveCapture, nullptr, this, nullptr);
    m_LiveCapture = nullptr;
  }

  delete ui;
}

void ApiMonitorWindow::SetTargetConnection(LiveCapture *live)
{
  // Disconnect old signals
  if(m_LiveCapture)
  {
    disconnect(m_LiveCapture, &LiveCapture::monitorLogsReceived, this,
               &ApiMonitorWindow::AppendLogs);
    disconnect(m_LiveCapture, &LiveCapture::monitorStatusReceived, this,
               &ApiMonitorWindow::UpdateStatus);
    disconnect(m_LiveCapture, &QObject::destroyed, this, nullptr);
  }

  m_LiveCapture = live;

  if(live)
  {
    connect(live, &LiveCapture::monitorLogsReceived, this, &ApiMonitorWindow::AppendLogs);
    connect(live, &LiveCapture::monitorStatusReceived, this, &ApiMonitorWindow::UpdateStatus);
    connect(live, &QObject::destroyed, this, [this]() {
      m_LiveCapture = nullptr;
      ui->applyScript->setEnabled(false);
      ui->unloadScript->setEnabled(false);
      ui->statusLabel->setText(tr("Disconnected"));
      ui->statusLabel->setStyleSheet(QString());
    });

    ui->applyScript->setEnabled(true);
    ui->unloadScript->setEnabled(true);
    ui->statusLabel->setText(tr("Connected"));
    ui->statusLabel->setStyleSheet(QString());

    // Replay cached logs from before this window was opened
    QStringList cached = live->GetMonitorLogCache();
    if(!cached.isEmpty())
      AppendLogs(cached);
  }
  else
  {
    ui->applyScript->setEnabled(false);
    ui->unloadScript->setEnabled(false);
    ui->statusLabel->setText(tr("Disconnected"));
    ui->statusLabel->setStyleSheet(QString());
  }
}

bool ApiMonitorWindow::LoadScriptFromFile(const QString &path)
{
  QFile f(path);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream stream(&f);
    QString text = stream.readAll();
    m_ScriptEditor->setText(text.toUtf8().data());
    m_ScriptEditor->emptyUndoBuffer();
    m_CurrentFilePath = path;
    setWindowTitle(tr("API Monitor - %1").arg(QFileInfo(path).fileName()));
    return true;
  }
  return false;
}

void ApiMonitorWindow::AppendLogs(const QStringList &logs)
{
  QTextCursor cursor = ui->logOutput->textCursor();
  cursor.movePosition(QTextCursor::End);

  for(const QString &line : logs)
  {
    // Color errors in red
    if(line.contains(lit("Error")) || line.contains(lit("error")) ||
       line.contains(lit("Traceback")) || line.contains(lit("exception")))
    {
      QTextCharFormat fmt;
      fmt.setForeground(QColor(220, 50, 50));
      cursor.insertText(line + lit("\n"), fmt);
    }
    else
    {
      cursor.insertText(line + lit("\n"));
    }
    m_LogLineCount++;
  }

  // Trim old lines if exceeding limit
  if(m_LogLineCount > MAX_LOG_LINES)
  {
    cursor.movePosition(QTextCursor::Start);
    int linesToRemove = m_LogLineCount - MAX_LOG_LINES;
    for(int i = 0; i < linesToRemove; i++)
      cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_LogLineCount = MAX_LOG_LINES;
  }

  // Auto-scroll to bottom
  ui->logOutput->verticalScrollBar()->setValue(ui->logOutput->verticalScrollBar()->maximum());
}

void ApiMonitorWindow::UpdateStatus(const QString &status)
{
  ui->statusLabel->setText(status);

  if(status.startsWith(lit("Error")) || status.startsWith(lit("Script error")))
  {
    ui->statusLabel->setStyleSheet(lit("color: red;"));
    // Also show the error in the log
    AppendLogs({status});
  }
  else if(status.startsWith(lit("Active")))
  {
    ui->statusLabel->setStyleSheet(lit("color: green;"));
  }
  else
  {
    ui->statusLabel->setStyleSheet(QString());
  }
}

void ApiMonitorWindow::on_openScript_clicked()
{
  QString filename = RDDialog::getOpenFileName(this, tr("Open Monitor Script"), m_CurrentFilePath,
                                               tr("Python Scripts (*.py);;All Files (*)"));

  if(!filename.isEmpty())
  {
    LoadScriptFromFile(filename);
  }
}

void ApiMonitorWindow::on_saveScript_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save Monitor Script"), m_CurrentFilePath,
                                               tr("Python Scripts (*.py);;All Files (*)"));

  if(!filename.isEmpty())
  {
    QFile f(filename);
    if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
      // Get text from Scintilla editor
      sptr_t len = m_ScriptEditor->textLength();
      QByteArray rawText = m_ScriptEditor->getText(len + 1);
      // Remove trailing null and carriage returns
      QString text = QString::fromUtf8(rawText);
      text.remove(QLatin1Char('\r'));
      f.write(text.toUtf8());
      m_CurrentFilePath = filename;
      setWindowTitle(tr("API Monitor - %1").arg(QFileInfo(filename).fileName()));
    }
    else
    {
      RDDialog::critical(this, tr("Error saving script"),
                         tr("Couldn't open path %1 for write.").arg(filename));
    }
  }
}

void ApiMonitorWindow::on_applyScript_clicked()
{
  if(!m_LiveCapture)
  {
    RDDialog::information(this, tr("No Target Connected"),
                          tr("No target application is connected. "
                             "Launch an application first, then apply the script."));
    return;
  }

  // Get script text from editor
  sptr_t len = m_ScriptEditor->textLength();
  QByteArray rawText = m_ScriptEditor->getText(len + 1);
  QString scriptSource = QString::fromUtf8(rawText);
  scriptSource.remove(QLatin1Char('\r'));

  if(scriptSource.trimmed().isEmpty())
  {
    RDDialog::information(this, tr("Empty Script"), tr("The script editor is empty."));
    return;
  }

  m_LiveCapture->SendMonitorScript(scriptSource);
  ui->statusLabel->setText(tr("Sending script..."));
  ui->statusLabel->setStyleSheet(QString());
}

void ApiMonitorWindow::on_unloadScript_clicked()
{
  if(!m_LiveCapture)
    return;

  m_LiveCapture->SendMonitorControl(lit("unload"));
  ui->statusLabel->setText(tr("Unloading..."));
  ui->statusLabel->setStyleSheet(QString());
}

void ApiMonitorWindow::on_clearLog_clicked()
{
  ui->logOutput->clear();
  m_LogLineCount = 0;
}
