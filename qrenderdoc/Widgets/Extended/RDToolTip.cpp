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

#include "RDToolTip.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QMouseEvent>
#include <QProxyStyle>
#include <QStyleOptionFrame>
#include <QStylePainter>

void ICustomToolTipDisplay::showTip(QWidget *widget, QString text, QModelIndex idx)
{
  QPoint p = QCursor::pos();

  // estimate, as this is not easily queryable
  const QPoint cursorSize(16, 16);
  const QRect screenAvailGeom = QApplication::desktop()->availableGeometry(p);

  // start with the tooltip placed bottom-right of the cursor, as the default
  QRect tooltipRect;
  tooltipRect.setTopLeft(p + cursorSize);
  tooltipRect.setSize(configureTip(widget, text, idx));

  // clip by the available geometry in x
  if(tooltipRect.right() > screenAvailGeom.right())
    tooltipRect.moveRight(screenAvailGeom.right());

  // if we'd go out of bounds in y, place the tooltip above the cursor. Don't just clip like
  // in x, because that could place the tooltip over the cursor.
  if(tooltipRect.bottom() > screenAvailGeom.bottom())
    tooltipRect.moveBottom(p.y() - cursorSize.y());

  showTipAtPos(tooltipRect.topLeft());
}

RDToolTip::RDToolTip(QWidget *listener) : QLabel(NULL), mouseListener(listener)
{
  int margin = style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, NULL, this);
  int opacity = style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, NULL, this);

  setWindowFlags(Qt::ToolTip | Qt::WindowDoesNotAcceptFocus);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundRole(QPalette::ToolTipText);
  setBackgroundRole(QPalette::ToolTipBase);
  setMargin(margin + 1);
  setFrameStyle(QFrame::NoFrame);
  setAlignment(Qt::AlignLeft);
  setIndent(1);
  setWindowOpacity(opacity / 255.0);
}

QSize RDToolTip::configureTip(QWidget *, QString text, QModelIndex)
{
  setText(text);
  QSize ret = minimumSizeHint();
  resize(ret);
  return ret;
}

void RDToolTip::showTipAtPos(QPoint pos)
{
  move(pos);
  show();
}

bool RDToolTip::forceTip(QWidget *widget, QModelIndex idx)
{
  return false;
}

void RDToolTip::paintEvent(QPaintEvent *ev)
{
  QStylePainter p(this);
  QStyleOptionFrame opt;
  opt.init(this);
  p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
  p.end();

  QLabel::paintEvent(ev);
}

void RDToolTip::mousePressEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDToolTip::sendListenerEvent(QMouseEvent *e)
{
  QMouseEvent *duplicate =
      new QMouseEvent(e->type(), mouseListener->mapFromGlobal(e->globalPos()), e->windowPos(),
                      e->globalPos(), e->button(), e->buttons(), e->modifiers(), e->source());
  QCoreApplication::postEvent(mouseListener, duplicate);
}

void RDToolTip::mouseReleaseEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDToolTip::mouseDoubleClickEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDToolTip::resizeEvent(QResizeEvent *e)
{
  QStyleHintReturnMask frameMask;
  QStyleOption option;
  option.init(this);
  if(style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
    setMask(frameMask.region);

  QLabel::resizeEvent(e);
}
