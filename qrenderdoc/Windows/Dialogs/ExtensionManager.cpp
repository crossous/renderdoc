/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2026 Baldur Karlsson
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

#include "ExtensionManager.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QKeyEvent>
#include <QRegularExpression>
#include "Code/Interface/QRDInterface.h"
#include "Code/Resources.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "Windows/MainWindow.h"
#include "ui_ExtensionManager.h"

Q_DECLARE_METATYPE(ExtensionMetadata);

ExtensionManager::ExtensionManager(ICaptureContext &ctx)
    : QDialog(NULL), ui(new Ui::ExtensionManager), m_Ctx(ctx)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->extensions->setHeader(header);

    ui->extensions->setColumns({tr("Package"), tr("Name"), tr("Enabled")});
    header->setColumnStretchHints({1, 4, -1});
  }

  ui->name->setText(lit("---"));
  ui->version->setText(lit("---"));
  ui->author->setText(lit("---"));
  ui->URL->setText(lit("---"));
  ui->status->setText(lit("---"));

  QObject::connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

  PopulateExtensionList();
}

void ExtensionManager::PopulateExtensionList()
{
  ui->extensions->clear();

  QString extensionFolder = ConfigFilePath("extensions");

  m_Extensions = m_Ctx.Extensions().GetInstalledExtensions();

  if(m_Extensions.isEmpty())
  {
    QString contrib_url = lit("https://github.com/baldurk/renderdoc-contrib");
    ui->extensions->addTopLevelItem(
        new RDTreeWidgetItem({QString(), tr("No extensions found available"), QString()}));
    ui->extensions->addTopLevelItem(new RDTreeWidgetItem(
        {QString(), tr("Create packages in %1").arg(extensionFolder), QString()}));
    ui->extensions->addTopLevelItem(new RDTreeWidgetItem(
        {QString(), tr("Browse extensions at %1").arg(contrib_url), QString()}));
    ui->URL->setText(lit("<a href=\"%1\">%1</a>").arg(contrib_url));
  }
  else
  {
    for(const ExtensionMetadata &e : m_Extensions)
    {
      RDTreeWidgetItem *item = new RDTreeWidgetItem({e.package, e.name, QString()});

      item->setCheckState(
          2, m_Ctx.Config().AlwaysLoad_Extensions.contains(e.package) ? Qt::Checked : Qt::Unchecked);

      ui->extensions->addTopLevelItem(item);
    }

    ui->extensions->setCurrentItem(ui->extensions->topLevelItem(0));
  }
}

ExtensionManager::~ExtensionManager()
{
  delete ui;
}

void ExtensionManager::loadExtension(RDTreeWidgetItem *item)
{
  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      // if the load succeeds, set us as checked. Otherwise, unchecked
      QString errors = m_Ctx.Extensions().LoadExtension(e.package);
      if(!errors.isEmpty())
      {
        RDDialog::critical(this, tr("Failed to load extension"),
                           tr("Failed to load extension '%1':\n"
                              "%2")
                               .arg(e.name)
                               .arg(errors));
      }

      update_currentItem(item);
    }
  }
}

void ExtensionManager::on_extensions_currentItemChanged(RDTreeWidgetItem *item, RDTreeWidgetItem *)
{
  update_currentItem(item);
}

void ExtensionManager::on_extensions_itemChanged(RDTreeWidgetItem *item, int col)
{
  if(col == 2)
  {
    ui->extensions->setCurrentItem(item);

    QString package = item->text(0);

    bool loaded = m_Ctx.Extensions().IsExtensionLoaded(package);
    bool enabled = (item->checkState(2) == Qt::Checked);

    // if the extension is unloaded and the user just enabled it, try to load it.
    if(!loaded && enabled)
    {
      loadExtension(item);
    }

    // update the config after, in case the extension immediately has crahed
    m_Ctx.Config().AlwaysLoad_Extensions.removeOne(package);
    if(enabled)
      m_Ctx.Config().AlwaysLoad_Extensions.push_back(package);

    m_Ctx.Config().Save();

    update_currentItem(item);
  }
}

void ExtensionManager::update_currentItem(RDTreeWidgetItem *item)
{
  if(!item)
    return;

  if(item != ui->extensions->currentItem())
  {
    ui->extensions->setCurrentItem(item);
    return;
  }

  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      QRegularExpression authRE(lit("^(.*) <(.*)>$"));

      ui->name->setText(e.name);
      ui->version->setText(e.version);
      ui->URL->setText(QFormatStr("<a href=\"%1\">%1</a>").arg(e.extensionURL));
      ui->description->setText(e.description);

      QRegularExpressionMatch match = authRE.match(QString(e.author).trimmed());

      if(match.hasMatch() && match.captured(2).contains(QLatin1Char('@')))
        ui->author->setText(
            QFormatStr("<a href=\"mailto:%2\">%1</a>").arg(match.captured(1)).arg(match.captured(2)));
      else
        ui->author->setText(e.author);

      const bool enabled = m_Ctx.Config().AlwaysLoad_Extensions.contains(e.package);
      const bool loaded = m_Ctx.Extensions().IsExtensionLoaded(e.package);

      if(loaded && enabled)
        ui->status->setText(tr("Loaded (Enabled at startup)"));
      else if(loaded && !enabled)
        ui->status->setText(tr("Loaded (Restart required to disable)"));
      else if(!loaded && enabled)
        ui->status->setText(tr("Failed to load (Enabled at startup)"));
      else if(!loaded && !enabled)
        ui->status->setText(tr("Disabled"));
    }
  }
}
