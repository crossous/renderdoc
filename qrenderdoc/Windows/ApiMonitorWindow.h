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

#pragma once

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ApiMonitorWindow;
}

class ScintillaEdit;
class LiveCapture;

class ApiMonitorWindow : public QFrame
{
  Q_OBJECT

public:
  explicit ApiMonitorWindow(ICaptureContext &ctx, QWidget *parent = 0);
  ~ApiMonitorWindow();

  QWidget *Widget() { return this; }

  void SetTargetConnection(LiveCapture *live);
  bool LoadScriptFromFile(const QString &path);

public slots:
  void AppendLogs(const QStringList &logs);
  void UpdateStatus(const QString &status);

private slots:
  void on_openScript_clicked();
  void on_saveScript_clicked();
  void on_applyScript_clicked();
  void on_unloadScript_clicked();
  void on_clearLog_clicked();

private:
  Ui::ApiMonitorWindow *ui;
  ICaptureContext &m_Ctx;
  ScintillaEdit *m_ScriptEditor;
  LiveCapture *m_LiveCapture = nullptr;
  QString m_CurrentFilePath;
  int m_LogLineCount = 0;
  static const int MAX_LOG_LINES = 10000;
};
