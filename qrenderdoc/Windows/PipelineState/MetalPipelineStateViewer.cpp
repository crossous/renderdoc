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

#include "MetalPipelineStateViewer.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QXmlStreamWriter>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "PipelineStateViewer.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDTreeWidget.h"
#include "Widgets/PipelineFlowChart.h"

static const int MetalResourceIdRole = Qt::UserRole + 1;
static const int MetalBufferOffsetRole = Qt::UserRole + 2;
static const int MetalBufferSizeRole = Qt::UserRole + 3;
static const int MetalBufferSlotRole = Qt::UserRole + 4;
static const uint32_t MetalSamplerDescriptorOffset = 0x100;
static const uint32_t MetalBufferDescriptorOffset = 0x200;

static uint32_t MetalDescriptorSlot(const DescriptorAccess &access)
{
  if(access.type == DescriptorType::Sampler && access.byteOffset >= MetalSamplerDescriptorOffset)
    return access.byteOffset - MetalSamplerDescriptorOffset;
  if(access.type == DescriptorType::ConstantBuffer &&
     access.byteOffset >= MetalBufferDescriptorOffset)
    return access.byteOffset - MetalBufferDescriptorOffset;
  return access.byteOffset;
}

MetalPipelineStateViewer::MetalPipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), m_Ctx(ctx)
{
  setObjectName(lit("MetalPipelineStateViewer"));

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  QFrame *toolbar = new QFrame(this);
  toolbar->setFrameShape(QFrame::Panel);
  toolbar->setFrameShadow(QFrame::Raised);
  QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(6, 2, 6, 2);
  toolbarLayout->setSpacing(2);
  QLabel *controls = new QLabel(tr("Controls"), toolbar);
  controls->setMargin(4);
  toolbarLayout->addWidget(controls);

  QFrame *separator = new QFrame(toolbar);
  separator->setFrameShape(QFrame::VLine);
  separator->setFrameShadow(QFrame::Sunken);
  toolbarLayout->addWidget(separator);

  m_ShowUnused = new QToolButton(toolbar);
  m_ShowUnused->setText(tr("Show Unused Items"));
  m_ShowUnused->setToolTip(
      tr("Metal shader binding reflection is not available yet, so unused bindings cannot be "
         "identified reliably."));
  m_ShowUnused->setCheckable(true);
  m_ShowUnused->setEnabled(false);
  m_ShowUnused->setAutoRaise(true);
  m_ShowUnused->setIcon(QIcon(lit(":/page_white_delete.png")));
  m_ShowUnused->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toolbarLayout->addWidget(m_ShowUnused);

  m_ShowEmpty = new QToolButton(toolbar);
  m_ShowEmpty->setText(tr("Show Empty Items"));
  m_ShowEmpty->setToolTip(tr("Show known pipeline slots which have no resource bound."));
  m_ShowEmpty->setCheckable(true);
  m_ShowEmpty->setAutoRaise(true);
  m_ShowEmpty->setIcon(QIcon(lit(":/page_white_database.png")));
  m_ShowEmpty->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toolbarLayout->addWidget(m_ShowEmpty);

  m_Export = new QToolButton(toolbar);
  m_Export->setText(tr("Export"));
  m_Export->setIcon(Icons::save());
  m_Export->setToolTip(tr("Export current state to HTML"));
  m_Export->setAccessibleName(tr("Export Pipeline State"));
  m_Export->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_Export->setAutoRaise(true);
  toolbarLayout->addWidget(m_Export);
  toolbarLayout->addStretch();

  QLabel *pipelineLabel = new QLabel(tr("Graphics Pipeline:"), toolbar);
  toolbarLayout->addWidget(pipelineLabel);
  m_Pipeline = new QLabel(this);
  m_Pipeline->setObjectName(lit("metalPipeline"));
  m_Pipeline->setTextInteractionFlags(Qt::TextSelectableByMouse);
  toolbarLayout->addWidget(m_Pipeline);
  layout->addWidget(toolbar);

  m_PipeFlow = new PipelineFlowChart(this);
  QFont flowFont = m_PipeFlow->font();
  flowFont.setPointSize(12);
  m_PipeFlow->setFont(flowFont);
  m_PipeFlow->setStages({lit("IA"), lit("VS"), lit("RS"), lit("FS"), lit("OM")},
                        {tr("Input Assembly"), tr("Vertex Shader"), tr("Rasterizer"),
                         tr("Fragment Shader"), tr("Output Merger")});
  layout->addWidget(m_PipeFlow);

  m_Stages = new QTabWidget(this);
  m_Stages->setDocumentMode(true);
  layout->addWidget(m_Stages);

  QVBoxLayout *ia = MakeStagePage(tr("Input Assembly"));
  QGroupBox *assemblyState = new QGroupBox(tr("Input Assembly State"), this);
  QFormLayout *assemblySummary = new QFormLayout(assemblyState);
  m_Topology = new QLabel(this);
  m_Topology->setObjectName(lit("metalTopology"));
  assemblySummary->addRow(tr("Primitive Topology:"), m_Topology);
  ia->addWidget(assemblyState);
  m_VertexAttributes = MakeTree(
      ia, tr("Vertex Attributes"), {tr("Attribute"), tr("Format"), tr("Buffer"), tr("Offset")});
  m_VertexBuffers = MakeTree(ia, tr("Vertex Buffers"),
                             {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Size"),
                              tr("Stride"), tr("Step")});
  m_IndexBuffer =
      MakeTree(ia, tr("Index Buffer"), {tr("Buffer"), tr("Offset"), tr("Size"), tr("Type")});
  ia->addStretch();

  QVBoxLayout *vs = MakeStagePage(tr("Vertex Shader"));
  m_VertexShader = MakeTree(vs, tr("Vertex Shader"), {tr("Function"), tr("Entry Point")});
  vs->addStretch();

  QVBoxLayout *rs = MakeStagePage(tr("Rasterizer"));
  QGroupBox *rasterState = new QGroupBox(tr("Rasterizer State"), this);
  QFormLayout *rasterSummary = new QFormLayout(rasterState);
  m_Viewport = new QLabel(this);
  m_Viewport->setObjectName(lit("metalViewport"));
  m_Scissor = new QLabel(this);
  m_Scissor->setObjectName(lit("metalScissor"));
  m_CullMode = new QLabel(this);
  m_CullMode->setObjectName(lit("metalCullMode"));
  m_FrontFace = new QLabel(this);
  m_FrontFace->setObjectName(lit("metalFrontFace"));
  rasterSummary->addRow(tr("Viewport:"), m_Viewport);
  rasterSummary->addRow(tr("Scissor:"), m_Scissor);
  rasterSummary->addRow(tr("Cull Mode:"), m_CullMode);
  rasterSummary->addRow(tr("Front Face:"), m_FrontFace);
  rs->addWidget(rasterState);
  rs->addStretch();

  QVBoxLayout *fs = MakeStagePage(tr("Fragment Shader"));
  m_FragmentShader =
      MakeTree(fs, tr("Fragment Shader"), {tr("Function"), tr("Entry Point")});
  m_FragmentBuffers = MakeTree(fs, tr("Constant Buffers"),
                               {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Size"),
                                tr("Bytes Needed")});
  m_FragmentTextures = MakeTree(
      fs, tr("Read-Only Resources"), {tr("Slot"), tr("Texture"), tr("Type"), tr("Format")});
  m_FragmentSamplers = MakeTree(
      fs, tr("Samplers"), {tr("Slot"), tr("Sampler"), tr("Filter"), tr("Address")});
  fs->addStretch();

  QVBoxLayout *om = MakeStagePage(tr("Output Merger"));
  m_MultisampleState =
      MakeTree(om, tr("Multisample State"),
               {tr("Samples"), tr("Alpha to Coverage"), tr("Alpha to One")});
  m_ColorTargets = MakeTree(
      om, tr("Color Targets"),
      {tr("Slot"), tr("Texture"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
       tr("Array Size"), tr("Samples"), tr("Format"), tr("Mip"), tr("Slice")});
  m_ResolveTargets = MakeTree(
      om, tr("Resolve Targets"),
      {tr("Slot"), tr("Texture"), tr("Type"), tr("Width"), tr("Height"), tr("Samples"),
       tr("Format"), tr("Mip"), tr("Slice")});
  m_ColorBlends =
      MakeTree(om, tr("Blend State"),
               {tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Write Mask")});
  m_DepthTarget =
      MakeTree(om, tr("Depth Target"), {tr("Texture"), tr("Mip"), tr("Slice")});
  m_DepthState =
      MakeTree(om, tr("Depth State"), {tr("State"), tr("Compare"), tr("Write")});
  m_StencilState = MakeTree(
      om, tr("Stencil State"),
      {tr("Face"), tr("Reference"), tr("Compare Mask"), tr("Write Mask"), tr("Function"),
       tr("Pass Op"), tr("Fail Op"), tr("Depth Fail Op")});
  om->addStretch();

  m_Stages->setCurrentIndex(0);
  m_Stages->tabBar()->setVisible(false);
  QObject::connect(m_PipeFlow, &PipelineFlowChart::stageSelected, m_Stages,
                   &QTabWidget::setCurrentIndex);
  QObject::connect(m_ShowEmpty, &QToolButton::toggled, this,
                   [this](bool) { SetState(); });
  QObject::connect(m_ShowUnused, &QToolButton::toggled, this,
                   [this](bool) { SetState(); });
  QObject::connect(m_Export, &QToolButton::clicked, this,
                   &MetalPipelineStateViewer::ExportHTML);
}

QVBoxLayout *MetalPipelineStateViewer::MakeStagePage(const QString &title)
{
  QScrollArea *scroll = new QScrollArea(m_Stages);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  QWidget *contents = new QWidget(scroll);
  QVBoxLayout *stageLayout = new QVBoxLayout(contents);
  stageLayout->setContentsMargins(6, 6, 6, 6);
  scroll->setWidget(contents);
  m_Stages->addTab(scroll, title);
  return stageLayout;
}

RDTreeWidget *MetalPipelineStateViewer::MakeTree(QVBoxLayout *parentLayout, const QString &title,
                                                 const QStringList &headers)
{
  QGroupBox *group = new QGroupBox(title, this);
  QVBoxLayout *groupLayout = new QVBoxLayout(group);
  RDTreeWidget *tree = new RDTreeWidget(group);
  QString treeName = title;
  treeName.remove(QLatin1Char(' '));
  tree->setObjectName(lit("metal") + treeName);
  tree->setAccessibleName(title);
  RDHeaderView *header = new RDHeaderView(Qt::Horizontal, tree);
  tree->setHeader(header);
  tree->setColumns(headers);
  QList<int> stretchHints;
  for(int column = 0; column < headers.size(); column++)
    stretchHints << (column == headers.size() - 1 ? 2 : 1);
  header->setColumnStretchHints(stretchHints);
  tree->setRootIsDecorated(false);
  tree->setItemsExpandable(false);
  tree->setAllColumnsShowFocus(true);
  tree->setAlternatingRowColors(true);
  tree->setFont(Formatter::PreferredFont());
  tree->setClearSelectionOnFocusLoss(true);
  tree->setInstantTooltips(true);
  groupLayout->addWidget(tree);
  parentLayout->addWidget(group);

  PipelineStateViewer *common = static_cast<PipelineStateViewer *>(parentWidget());
  common->SetupResourceView(tree);

  QObject::connect(tree, &RDTreeWidget::itemActivated, this,
                   [this, tree](RDTreeWidgetItem *item, int column) {
                     if(tree == m_VertexAttributes)
                     {
                       m_Ctx.ShowMeshPreview();
                       return;
                     }

                     if(tree == m_VertexShader || tree == m_FragmentShader)
                     {
                       const ShaderStage stage = tree == m_VertexShader ? ShaderStage::Vertex
                                                                        : ShaderStage::Fragment;
                       const PipeState &pipe = m_Ctx.CurPipelineState();
                       const ShaderReflection *reflection = pipe.GetShaderReflection(stage);
                       if(reflection == NULL)
                         return;

                       IShaderViewer *viewer =
                           m_Ctx.ViewShader(reflection, pipe.GetGraphicsPipelineObject());
                       m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
                       return;
                     }

                     ResourceId id = GetResource(item);
                     if(id == ResourceId())
                       return;

                     if(tree == m_VertexBuffers || tree == m_IndexBuffer ||
                        tree == m_FragmentBuffers)
                     {
                       const uint64_t offset = item->data(0, MetalBufferOffsetRole).toULongLong();
                       const uint64_t size = item->data(0, MetalBufferSizeRole).toULongLong();
                       QString format;
                       if(tree == m_VertexBuffers)
                       {
                         const uint32_t slot = item->data(0, MetalBufferSlotRole).toUInt();
                         PipelineStateViewer *common =
                             static_cast<PipelineStateViewer *>(parentWidget());
                         format = common->GetVBufferFormatString(slot);
                       }
                       else if(tree == m_IndexBuffer)
                       {
                         const uint32_t stride = item->data(0, MetalBufferSlotRole).toUInt();
                         format = stride == 2 ? lit("ushort index;") : lit("uint index;");
                       }

                       IBufferViewer *viewer = m_Ctx.ViewBuffer(offset, size, id, format);
                       m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
                     }
                     else if(m_Ctx.GetTexture(id) != NULL)
                     {
                       if(!m_Ctx.HasTextureViewer())
                         m_Ctx.ShowTextureViewer();
                       m_Ctx.GetTextureViewer()->ViewTexture(id, CompType::Typeless, true);
                     }
                     else
                     {
                       m_Ctx.ShowResourceInspector();
                       m_Ctx.GetResourceInspector()->Inspect(id);
                     }
                   });

  return tree;
}

RDTreeWidgetItem *MetalPipelineStateViewer::AddEmptyRow(RDTreeWidget *tree,
                                                        const QStringList &values)
{
  RDTreeWidgetItem *item = AddResourceRow(tree, values, ResourceId());
  item->setBackgroundColor(QColor(255, 70, 70));
  item->setForegroundColor(QColor(0, 0, 0));
  return item;
}

RDTreeWidgetItem *MetalPipelineStateViewer::AddResourceRow(RDTreeWidget *tree,
                                                           const QStringList &values,
                                                           ResourceId resource)
{
  QVariantList columns;
  for(const QString &value : values)
    columns << value;
  RDTreeWidgetItem *item = new RDTreeWidgetItem(columns);
  item->setTag(QVariant::fromValue(resource));
  item->setData(0, MetalResourceIdRole, QVariant::fromValue(resource));
  tree->addTopLevelItem(item);
  return item;
}

ResourceId MetalPipelineStateViewer::GetResource(RDTreeWidgetItem *item)
{
  if(item == NULL || !item->tag().canConvert<ResourceId>())
    return ResourceId();
  return item->tag().value<ResourceId>();
}

void MetalPipelineStateViewer::ExportHTMLTree(QXmlStreamWriter &xml, const QString &title,
                                              RDTreeWidget *tree)
{
  xml.writeStartElement(lit("h2"));
  xml.writeCharacters(title);
  xml.writeEndElement();

  QList<QVariantList> rows;
  const QStringList headers = tree->getHeaders();
  for(int row = 0; row < tree->topLevelItemCount(); row++)
  {
    RDTreeWidgetItem *item = tree->topLevelItem(row);
    QVariantList values;
    for(int column = 0; column < headers.count(); column++)
      values << item->text(column);
    rows << values;
  }

  PipelineStateViewer *common = static_cast<PipelineStateViewer *>(parentWidget());
  common->exportHTMLTable(xml, headers, rows);
}

void MetalPipelineStateViewer::ExportHTML()
{
  PipelineStateViewer *common = static_cast<PipelineStateViewer *>(parentWidget());
  QXmlStreamWriter *xmlptr = common->beginHTMLExport();
  if(xmlptr == NULL)
    return;

  QXmlStreamWriter &xml = *xmlptr;
  const QStringList stageNames = m_PipeFlow->stageNames();
  const QStringList stageAbbreviations = m_PipeFlow->stageAbbreviations();

  for(int stage = 0; stage < stageNames.count(); stage++)
  {
    xml.writeStartElement(lit("div"));
    xml.writeStartElement(lit("a"));
    xml.writeAttribute(lit("name"), stageAbbreviations[stage]);
    xml.writeEndElement();
    xml.writeEndElement();

    xml.writeStartElement(lit("div"));
    xml.writeAttribute(lit("class"), lit("stage"));
    xml.writeStartElement(lit("h1"));
    xml.writeCharacters(stageNames[stage]);
    xml.writeEndElement();

    switch(stage)
    {
      case 0:
        common->exportHTMLTable(xml, {tr("Primitive Topology")}, {m_Topology->text()});
        ExportHTMLTree(xml, tr("Vertex Attributes"), m_VertexAttributes);
        ExportHTMLTree(xml, tr("Vertex Buffers"), m_VertexBuffers);
        ExportHTMLTree(xml, tr("Index Buffer"), m_IndexBuffer);
        break;
      case 1: ExportHTMLTree(xml, tr("Vertex Shader"), m_VertexShader); break;
      case 2:
        common->exportHTMLTable(
            xml, {tr("Viewport"), tr("Scissor"), tr("Cull Mode"), tr("Front Face")},
            {m_Viewport->text(), m_Scissor->text(), m_CullMode->text(), m_FrontFace->text()});
        break;
      case 3:
        ExportHTMLTree(xml, tr("Fragment Shader"), m_FragmentShader);
        ExportHTMLTree(xml, tr("Constant Buffers"), m_FragmentBuffers);
        ExportHTMLTree(xml, tr("Read-Only Resources"), m_FragmentTextures);
        ExportHTMLTree(xml, tr("Samplers"), m_FragmentSamplers);
        break;
      case 4:
        ExportHTMLTree(xml, tr("Multisample State"), m_MultisampleState);
        ExportHTMLTree(xml, tr("Color Targets"), m_ColorTargets);
        ExportHTMLTree(xml, tr("Resolve Targets"), m_ResolveTargets);
        ExportHTMLTree(xml, tr("Blend State"), m_ColorBlends);
        ExportHTMLTree(xml, tr("Depth Target"), m_DepthTarget);
        ExportHTMLTree(xml, tr("Depth State"), m_DepthState);
        ExportHTMLTree(xml, tr("Stencil State"), m_StencilState);
        break;
      default: break;
    }

    xml.writeEndElement();
  }

  common->endHTMLExport(xmlptr);
}

void MetalPipelineStateViewer::OnCaptureLoaded()
{
  SetState();
}

void MetalPipelineStateViewer::OnCaptureClosed()
{
  ClearState();
}

void MetalPipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  SetState();
}

void MetalPipelineStateViewer::SelectPipelineStage(PipelineStage stage)
{
  int index = -1;
  switch(stage)
  {
    case PipelineStage::VertexInput: index = 0; break;
    case PipelineStage::VertexShader: index = 1; break;
    case PipelineStage::Rasterizer: index = 2; break;
    case PipelineStage::PixelShader: index = 3; break;
    case PipelineStage::ColorDepthOutput:
    case PipelineStage::SampleMask: index = 4; break;
    default: break;
  }

  if(index >= 0)
    m_PipeFlow->setSelectedStage(index);
}

void MetalPipelineStateViewer::ClearState()
{
  m_Pipeline->setText(tr("No render pipeline bound"));
  m_Topology->setText(ToQStr(Topology::Unknown));
  m_Viewport->setText(tr("Disabled"));
  m_Scissor->setText(tr("Disabled"));
  m_CullMode->setText(ToQStr(CullMode::NoCull));
  m_FrontFace->setText(tr("Clockwise"));
  m_VertexShader->clear();
  m_FragmentShader->clear();
  m_FragmentBuffers->clear();
  m_FragmentTextures->clear();
  m_FragmentSamplers->clear();
  m_VertexAttributes->clear();
  m_VertexBuffers->clear();
  m_IndexBuffer->clear();
  m_DepthState->clear();
  m_StencilState->clear();
  m_MultisampleState->clear();
  m_ColorTargets->clear();
  m_ResolveTargets->clear();
  m_ColorBlends->clear();
  m_DepthTarget->clear();
  m_PipeFlow->setStagesEnabled({true, false, true, false, true});
}

void MetalPipelineStateViewer::SetState()
{
  ClearState();

  const PipeState &pipe = m_Ctx.CurPipelineState();
  if(!pipe.IsCaptureMetal())
    return;

  ResourceId pipeline = pipe.GetGraphicsPipelineObject();
  if(pipeline != ResourceId())
    m_Pipeline->setText(m_Ctx.GetResourceName(pipeline));
  m_Topology->setText(ToQStr(pipe.GetPrimitiveTopology()));

  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL)
    return;

  const ShaderReflection *fragmentReflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  const bool hasBindingReflection =
      fragmentReflection != NULL && (!fragmentReflection->constantBlocks.empty() ||
                                     !fragmentReflection->samplers.empty() ||
                                     !fragmentReflection->readOnlyResources.empty() ||
                                     !fragmentReflection->readWriteResources.empty());
  m_ShowUnused->setEnabled(hasBindingReflection);
  m_ShowUnused->setToolTip(
      hasBindingReflection
          ? tr("Show resources which are bound but statically unused by the shader.")
          : tr("Metal shader binding reflection is not available for this pipeline, so unused "
               "bindings cannot be identified reliably."));

  const Viewport viewport = pipe.GetViewport(0);
  if(viewport.enabled)
    m_Viewport->setText(tr("%1, %2  %3 x %4  depth %5 - %6")
                            .arg(viewport.x)
                            .arg(viewport.y)
                            .arg(viewport.width)
                            .arg(viewport.height)
                            .arg(viewport.minDepth)
                            .arg(viewport.maxDepth));
  const Scissor scissor = pipe.GetScissor(0);
  if(scissor.enabled)
    m_Scissor->setText(tr("%1, %2  %3 x %4")
                           .arg(scissor.x)
                           .arg(scissor.y)
                           .arg(scissor.width)
                           .arg(scissor.height));
  const RasterState raster = pipe.GetRasterState();
  m_CullMode->setText(ToQStr(raster.cullMode));
  m_FrontFace->setText(raster.frontCCW ? tr("Counter Clockwise") : tr("Clockwise"));

  ResourceId vertexShader = pipe.GetShader(ShaderStage::Vertex);
  if(vertexShader != ResourceId())
  {
    AddResourceRow(m_VertexShader,
                   {m_Ctx.GetResourceName(vertexShader),
                    pipe.GetShaderEntryPoint(ShaderStage::Vertex)},
                   vertexShader);
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_VertexShader, {tr("Unbound"), QString()});
  }

  ResourceId fragmentShader = pipe.GetShader(ShaderStage::Fragment);
  if(fragmentShader != ResourceId())
  {
    AddResourceRow(m_FragmentShader,
                   {m_Ctx.GetResourceName(fragmentShader),
                    pipe.GetShaderEntryPoint(ShaderStage::Fragment)},
                   fragmentShader);
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_FragmentShader, {tr("Unbound"), QString()});
  }
  m_PipeFlow->setStagesEnabled(
      {true, vertexShader != ResourceId(), true, fragmentShader != ResourceId(), true});

  for(const UsedDescriptor &binding :
      pipe.GetConstantBlocks(ShaderStage::Fragment, !m_ShowUnused->isChecked()))
  {
    const Descriptor &descriptor = binding.descriptor;
    if(descriptor.resource == ResourceId())
      continue;

    uint32_t bytesNeeded = 0;
    if(fragmentReflection != NULL &&
       binding.access.index < fragmentReflection->constantBlocks.size())
      bytesNeeded = fragmentReflection->constantBlocks[binding.access.index].byteSize;

    RDTreeWidgetItem *item = AddResourceRow(
        m_FragmentBuffers,
        {Formatter::Format(MetalDescriptorSlot(binding.access)),
         m_Ctx.GetResourceName(descriptor.resource),
         Formatter::HumanFormat(descriptor.byteOffset, Formatter::OffsetSize),
         Formatter::HumanFormat(descriptor.byteSize, Formatter::OffsetSize),
         Formatter::HumanFormat(bytesNeeded, Formatter::OffsetSize)},
        descriptor.resource);
    item->setData(0, MetalBufferOffsetRole, qulonglong(descriptor.byteOffset));
    item->setData(0, MetalBufferSizeRole, qulonglong(descriptor.byteSize));
  }
  if(m_ShowEmpty->isChecked())
  {
    const size_t slotCount = qMax<size_t>(1, state->fragmentBuffers.size());
    for(size_t slot = 0; slot < slotCount; slot++)
    {
      if(slot >= state->fragmentBuffers.size() ||
         state->fragmentBuffers[slot].resourceId == ResourceId())
        AddEmptyRow(m_FragmentBuffers,
                    {Formatter::Format((uint32_t)slot), tr("Empty"), QString(), QString(),
                     QString()});
    }
  }

  for(const UsedDescriptor &binding :
      pipe.GetReadOnlyResources(ShaderStage::Fragment, !m_ShowUnused->isChecked()))
  {
    const Descriptor &descriptor = binding.descriptor;
    if(descriptor.resource == ResourceId())
      continue;
    AddResourceRow(m_FragmentTextures,
                   {Formatter::Format(MetalDescriptorSlot(binding.access)),
                    m_Ctx.GetResourceName(descriptor.resource), ToQStr(descriptor.textureType),
                    QString(descriptor.format.Name())},
                   descriptor.resource);
  }
  if(m_ShowEmpty->isChecked())
  {
    const size_t slotCount = qMax<size_t>(1, state->fragmentTextures.size());
    for(size_t slot = 0; slot < slotCount; slot++)
    {
      if(slot >= state->fragmentTextures.size() || state->fragmentTextures[slot] == ResourceId())
        AddEmptyRow(m_FragmentTextures,
                    {Formatter::Format((uint32_t)slot), tr("Empty"), QString(), QString()});
    }
  }

  for(const UsedDescriptor &binding :
      pipe.GetSamplers(ShaderStage::Fragment, !m_ShowUnused->isChecked()))
  {
    const SamplerDescriptor &sampler = binding.sampler;
    if(sampler.object == ResourceId())
      continue;
    const QString filter = tr("%1 / %2 / %3")
                               .arg(ToQStr(sampler.filter.minify))
                               .arg(ToQStr(sampler.filter.magnify))
                               .arg(ToQStr(sampler.filter.mip));
    const QString address = tr("%1 / %2 / %3")
                                .arg(ToQStr(sampler.addressU))
                                .arg(ToQStr(sampler.addressV))
                                .arg(ToQStr(sampler.addressW));
    AddResourceRow(m_FragmentSamplers,
                   {Formatter::Format(MetalDescriptorSlot(binding.access)),
                    m_Ctx.GetResourceName(sampler.object), filter, address},
                   sampler.object);
  }
  if(m_ShowEmpty->isChecked())
  {
    const size_t slotCount = qMax<size_t>(1, state->fragmentSamplers.size());
    for(size_t slot = 0; slot < slotCount; slot++)
    {
      if(slot >= state->fragmentSamplers.size() || state->fragmentSamplers[slot] == ResourceId())
        AddEmptyRow(m_FragmentSamplers,
                    {Formatter::Format((uint32_t)slot), tr("Empty"), QString(), QString()});
    }
  }

  for(const MetalPipe::VertexAttribute &attribute : state->vertexAttributes)
  {
    AddResourceRow(m_VertexAttributes,
                   {Formatter::Format(attribute.attributeIndex), QString(attribute.format.Name()),
                    Formatter::Format(attribute.bufferIndex),
                    Formatter::HumanFormat(attribute.byteOffset, Formatter::OffsetSize)},
                   ResourceId());
  }
  if(m_ShowEmpty->isChecked() && state->vertexAttributes.empty())
    AddEmptyRow(m_VertexAttributes, {tr("Empty"), QString(), QString(), QString()});

  rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  for(size_t i = 0; i < vertexBuffers.size(); i++)
  {
    const BoundVBuffer &buffer = vertexBuffers[i];
    if(buffer.resourceId == ResourceId())
    {
      if(m_ShowEmpty->isChecked())
        AddEmptyRow(m_VertexBuffers,
                    {Formatter::Format((uint32_t)i), tr("Empty"), QString(), QString(), QString(),
                     QString()});
      continue;
    }

    RDTreeWidgetItem *item = AddResourceRow(
        m_VertexBuffers,
        {Formatter::Format((uint32_t)i), m_Ctx.GetResourceName(buffer.resourceId),
         Formatter::HumanFormat(buffer.byteOffset, Formatter::OffsetSize),
         Formatter::HumanFormat(buffer.byteSize, Formatter::OffsetSize),
         Formatter::HumanFormat(buffer.byteStride, Formatter::OffsetSize),
         state->vertexBuffers[i].perInstance
             ? tr("Instance / %1").arg(state->vertexBuffers[i].stepRate)
             : tr("Vertex / %1").arg(state->vertexBuffers[i].stepRate)},
        buffer.resourceId);
    item->setData(0, MetalBufferOffsetRole, QVariant::fromValue((qulonglong)buffer.byteOffset));
    item->setData(0, MetalBufferSizeRole, QVariant::fromValue((qulonglong)buffer.byteSize));
    item->setData(0, MetalBufferSlotRole, QVariant::fromValue((uint32_t)i));
  }

  const BoundVBuffer indexBuffer = pipe.GetIBuffer();
  if(indexBuffer.resourceId != ResourceId())
  {
    RDTreeWidgetItem *item = AddResourceRow(
        m_IndexBuffer,
        {m_Ctx.GetResourceName(indexBuffer.resourceId),
         Formatter::HumanFormat(indexBuffer.byteOffset, Formatter::OffsetSize),
         Formatter::HumanFormat(indexBuffer.byteSize, Formatter::OffsetSize),
         indexBuffer.byteStride == 2 ? tr("UInt16") : tr("UInt32")},
        indexBuffer.resourceId);
    item->setData(0, MetalBufferOffsetRole,
                  QVariant::fromValue((qulonglong)indexBuffer.byteOffset));
    item->setData(0, MetalBufferSizeRole,
                  QVariant::fromValue((qulonglong)indexBuffer.byteSize));
    item->setData(0, MetalBufferSlotRole, QVariant::fromValue(indexBuffer.byteStride));
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_IndexBuffer, {tr("Empty"), QString(), QString(), QString()});
  }

  const DepthTestState depth = pipe.GetDepthTestState();
  if(state->depthStencil.resourceId != ResourceId())
  {
    AddResourceRow(m_DepthState,
                   {m_Ctx.GetResourceName(state->depthStencil.resourceId),
                    ToQStr(depth.depthFunction), depth.depthWrites ? tr("Enabled") : tr("Disabled")},
                   state->depthStencil.resourceId);
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_DepthState, {tr("Unbound"), QString(), QString()});
  }

  if(pipe.IsStencilTestEnabled())
  {
    const rdcpair<StencilFace, StencilFace> faces = pipe.GetStencilFaces();
    auto addStencilFace = [this](const QString &name, const StencilFace &face) {
      AddResourceRow(
          m_StencilState,
          {name, Formatter::Format(face.reference, true),
           Formatter::Format(face.compareMask, true), Formatter::Format(face.writeMask, true),
           ToQStr(face.function), ToQStr(face.passOperation), ToQStr(face.failOperation),
           ToQStr(face.depthFailOperation)},
          ResourceId());
    };
    addStencilFace(tr("Front"), faces.first);
    addStencilFace(tr("Back"), faces.second);
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_StencilState,
                {tr("Disabled"), QString(), QString(), QString(), QString(), QString(), QString(),
                 QString()});
  }

  AddResourceRow(m_MultisampleState,
                 {Formatter::Format(state->sampleCount),
                  state->alphaToCoverageEnabled ? tr("Enabled") : tr("Disabled"),
                  state->alphaToOneEnabled ? tr("Enabled") : tr("Disabled")},
                 ResourceId());

  rdcarray<Descriptor> colorTargets = pipe.GetOutputTargets();
  for(size_t i = 0; i < colorTargets.size(); i++)
  {
    const Descriptor &target = colorTargets[i];
    if(target.resource == ResourceId())
    {
      if(m_ShowEmpty->isChecked())
        AddEmptyRow(m_ColorTargets,
                    {Formatter::Format((uint32_t)i), tr("Empty"), QString(), QString(),
                     QString(), QString(), QString(), QString(), QString(), QString(), QString()});
      continue;
    }

    TextureDescription *texture = m_Ctx.GetTexture(target.resource);
    AddResourceRow(m_ColorTargets,
                   {Formatter::Format((uint32_t)i), m_Ctx.GetResourceName(target.resource),
                    texture ? ToQStr(texture->type) : ToQStr(target.textureType),
                    texture ? Formatter::Format(texture->width) : QString(),
                    texture ? Formatter::Format(texture->height) : QString(),
                    texture ? Formatter::Format(texture->depth) : QString(),
                    texture ? Formatter::Format(texture->arraysize) : QString(),
                    texture ? Formatter::Format(texture->msSamp) : QString(),
                    QString(target.format.Name()), Formatter::Format((uint32_t)target.firstMip),
                    Formatter::Format((uint32_t)target.firstSlice)},
                   target.resource);
  }
  if(m_ShowEmpty->isChecked() && colorTargets.empty())
    AddEmptyRow(m_ColorTargets,
                {lit("0"), tr("Empty"), QString(), QString(), QString(), QString(), QString(),
                 QString(), QString(), QString(), QString()});

  for(size_t i = 0; i < state->resolveTargets.size(); i++)
  {
    const Descriptor &target = state->resolveTargets[i];
    if(target.resource == ResourceId())
    {
      if(m_ShowEmpty->isChecked())
        AddEmptyRow(m_ResolveTargets,
                    {Formatter::Format((uint32_t)i), tr("Empty"), QString(), QString(),
                     QString(), QString(), QString(), QString(), QString()});
      continue;
    }

    TextureDescription *texture = m_Ctx.GetTexture(target.resource);
    AddResourceRow(m_ResolveTargets,
                   {Formatter::Format((uint32_t)i), m_Ctx.GetResourceName(target.resource),
                    texture ? ToQStr(texture->type) : ToQStr(target.textureType),
                    texture ? Formatter::Format(texture->width) : QString(),
                    texture ? Formatter::Format(texture->height) : QString(),
                    texture ? Formatter::Format(texture->msSamp) : QString(),
                    QString(target.format.Name()), Formatter::Format((uint32_t)target.firstMip),
                    Formatter::Format((uint32_t)target.firstSlice)},
                   target.resource);
  }
  if(m_ShowEmpty->isChecked() && state->resolveTargets.empty())
    AddEmptyRow(m_ResolveTargets,
                {lit("0"), tr("Empty"), QString(), QString(), QString(), QString(), QString(),
                 QString(), QString()});

  const rdcarray<ColorBlend> colorBlends = pipe.GetColorBlends();
  for(size_t i = 0; i < colorBlends.size(); i++)
  {
    const ColorBlend &blend = colorBlends[i];
    const QString writeMask =
        QFormatStr("%1%2%3%4")
            .arg((blend.writeMask & 0x1) == 0 ? lit("_") : lit("R"))
            .arg((blend.writeMask & 0x2) == 0 ? lit("_") : lit("G"))
            .arg((blend.writeMask & 0x4) == 0 ? lit("_") : lit("B"))
            .arg((blend.writeMask & 0x8) == 0 ? lit("_") : lit("A"));

    AddResourceRow(m_ColorBlends,
                   {Formatter::Format((uint32_t)i), blend.enabled ? tr("True") : tr("False"),
                    ToQStr(blend.colorBlend.source), ToQStr(blend.colorBlend.destination),
                    ToQStr(blend.colorBlend.operation), ToQStr(blend.alphaBlend.source),
                    ToQStr(blend.alphaBlend.destination), ToQStr(blend.alphaBlend.operation),
                    writeMask},
                   ResourceId());
  }
  if(m_ShowEmpty->isChecked() && colorBlends.empty())
    AddEmptyRow(m_ColorBlends,
                {lit("0"), tr("Empty"), QString(), QString(), QString(), QString(), QString(),
                 QString(), QString()});

  const Descriptor depthTarget = pipe.GetDepthTarget();
  if(depthTarget.resource != ResourceId())
  {
    AddResourceRow(m_DepthTarget,
                   {m_Ctx.GetResourceName(depthTarget.resource),
                    Formatter::Format((uint32_t)depthTarget.firstMip),
                    Formatter::Format((uint32_t)depthTarget.firstSlice)},
                   depthTarget.resource);
  }
  else if(m_ShowEmpty->isChecked())
  {
    AddEmptyRow(m_DepthTarget, {tr("Empty"), QString(), QString()});
  }
}
