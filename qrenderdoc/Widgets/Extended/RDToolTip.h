/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

#include <QLabel>
#include <QModelIndex>

struct ICustomToolTipDisplay
{
public:
  virtual void hideTip() = 0;
  virtual QSize configureTip(QWidget *widget, QString text, QModelIndex idx = QModelIndex()) = 0;
  virtual void showTipAtPos(QPoint pos) = 0;
  virtual bool forceTip(QWidget *widget, QModelIndex idx) = 0;
  void showTip(QWidget *widget, QString text, QModelIndex idx = QModelIndex());
};

class RDToolTip : public QLabel, public ICustomToolTipDisplay
{
private:
  Q_OBJECT

  QWidget *mouseListener;

public:
  explicit RDToolTip(QWidget *listener = NULL);

  void hideTip() { hide(); }
  QSize configureTip(QWidget *widget, QString text, QModelIndex idx = QModelIndex());
  void showTipAtPos(QPoint pos);
  bool forceTip(QWidget *widget, QModelIndex idx);

protected:
  void paintEvent(QPaintEvent *);
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseDoubleClickEvent(QMouseEvent *);
  void resizeEvent(QResizeEvent *);

  void sendListenerEvent(QMouseEvent *e);
};
