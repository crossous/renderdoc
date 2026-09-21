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

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

class QLabel;
class QTabWidget;
class QToolButton;
class QVBoxLayout;
class QXmlStreamWriter;
class PipelineFlowChart;
class RDTreeWidget;
class RDTreeWidgetItem;

class MetalPipelineStateViewer : public QFrame, public ICaptureViewer
{
public:
  explicit MetalPipelineStateViewer(ICaptureContext &ctx, QWidget *parent = 0);

  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  void SelectPipelineStage(PipelineStage stage);
  ResourceId GetResource(RDTreeWidgetItem *item);

private:
  QVBoxLayout *MakeStagePage(const QString &title);
  RDTreeWidget *MakeTree(QVBoxLayout *parentLayout, const QString &title,
                         const QStringList &headers);
  RDTreeWidgetItem *AddResourceRow(RDTreeWidget *tree, const QStringList &values,
                                   ResourceId resource);
  RDTreeWidgetItem *AddEmptyRow(RDTreeWidget *tree, const QStringList &values);
  void ExportHTMLTree(QXmlStreamWriter &xml, const QString &title, RDTreeWidget *tree);
  void ExportHTML();
  void SetState();
  void ClearState();

  ICaptureContext &m_Ctx;
  QLabel *m_Pipeline = NULL;
  QLabel *m_Topology = NULL;
  QLabel *m_Viewport = NULL;
  QLabel *m_Scissor = NULL;
  QLabel *m_CullMode = NULL;
  QLabel *m_FrontFace = NULL;
  PipelineFlowChart *m_PipeFlow = NULL;
  QTabWidget *m_Stages = NULL;
  QToolButton *m_ShowUnused = NULL;
  QToolButton *m_ShowEmpty = NULL;
  QToolButton *m_Export = NULL;
  RDTreeWidget *m_VertexShader = NULL;
  RDTreeWidget *m_FragmentShader = NULL;
  RDTreeWidget *m_FragmentTextures = NULL;
  RDTreeWidget *m_FragmentSamplers = NULL;
  RDTreeWidget *m_VertexAttributes = NULL;
  RDTreeWidget *m_VertexBuffers = NULL;
  RDTreeWidget *m_IndexBuffer = NULL;
  RDTreeWidget *m_DepthState = NULL;
  RDTreeWidget *m_ColorTargets = NULL;
  RDTreeWidget *m_DepthTarget = NULL;
};
