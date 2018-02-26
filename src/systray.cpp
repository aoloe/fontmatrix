/***************************************************************************
 *   Copyright (C) 2007 by Riku Leino                                      *
 *   riku@scribus.info                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "systray.h"
#include "mainviewwidget.h"
#include "typotek.h"
#include "fontitem.h"
#include "fmfontdb.h"
#include <QtGui>
#include <QDebug>
#include <QMenu>

typotek* Systray::ttek = 0;

Systray::Systray()
{
    createActions();
    createTrayIcon();
    createTagMenu();

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));
	connect(trayIconMenu, SIGNAL(aboutToShow()), this, SLOT(slotPrepareMenu()));

	settings = new QSettings;

	showAllConfirmation = settings->value("Systray/AllConfirmation", true).toBool();
	showTagsConfirmation = settings->value("Systray/TagsConfirmation", false).toBool();

	slotSetActivateAll(settings->value("Systray/ActivateAllVisible", false).toBool());

	trayIcon->setIcon(QIcon(":/fontmatrix_systray_icon.png"));
	if (settings->value("Systray/Visible", false).toBool())
		trayIcon->show();
	else
		trayIcon->hide();
}

Systray::~Systray()
{

}

void Systray::slotSetVisible(bool isVisible)
{
	if (isVisible)
		trayIcon->show();
	else
		trayIcon->hide();

	settings->setValue("Systray/Visible", isVisible);
}

void Systray::slotSetActivateAll(bool isVisible)
{
	activateAllAction->setVisible(isVisible);
	deactivateAllAction->setVisible(isVisible);
	settings->setValue("Systray/ActivateAllVisible", isVisible);
}

void Systray::show()
{
    trayIcon->show();
}

void Systray::hide()
{
    trayIcon->hide();
}

void Systray::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
	case QSystemTrayIcon::Trigger:
	case QSystemTrayIcon::DoubleClick:
		ttek->isVisible() ? ttek->hide() : ttek->show();
		break;
	case QSystemTrayIcon::MiddleClick:
		break;
	default:
		;
	}
}

void Systray::slotMinimize()
{
	ttek->showMinimized();
}

void Systray::slotRestore()
{
	ttek->showNormal();
}

void Systray::slotActivateAll()
{
	ttek->theMainView->slotViewAll();
	if (showAllConfirmation) {
		bool wasVisible = ttek->isVisible();
		if (!wasVisible)
			ttek->show();
		ttek->slotActivateCurrents();
		if (!wasVisible)
			ttek->hide();
	} else
		ttek->theMainView->slotActivateAll();

	disconnect(tagMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotTagMenuClicked(QAction*)));
	QList<QAction*> tags = tagActions.values();
	foreach (QAction* a, tags) {
		a->setChecked(true);
	}
	connect(tagMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotTagMenuClicked(QAction*)));
}

void Systray::slotDeactivateAll()
{
	ttek->theMainView->slotViewAll();
	if (showAllConfirmation) {
		bool wasVisible = ttek->isVisible();
		if (!wasVisible)
			ttek->show();
		ttek->slotDeactivateCurrents();
		if (!wasVisible)
			ttek->hide();
	} else
		ttek->theMainView->slotDesactivateAll();
	disconnect(tagMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotTagMenuClicked(QAction*)));
	QList<QAction*> tags = tagActions.values();
	foreach (QAction* a, tags) {
		a->setChecked(false);
	}
	connect(tagMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotTagMenuClicked(QAction*)));
}

void Systray::slotTagMenuClicked(QAction *action)
{
	action->setIcon(QIcon ());
	QString name = action->text();
	if (name.isEmpty())
		return;

	if (!action->isChecked()) { // deactivate based on the tag name
		ttek->theMainView->slotFilterTag(name);
		if (showTagsConfirmation) {
			bool wasVisible = ttek->isVisible();
			if (!wasVisible)
				ttek->show();
			ttek->slotDeactivateCurrents();
			if (!wasVisible)
				ttek->hide();
		} else
			ttek->theMainView->slotDesactivateAll();
	} else { // activate based on the tag name
		ttek->theMainView->slotFilterTag(name);
		if (showTagsConfirmation) {
			bool wasVisible = ttek->isVisible();
			if (!wasVisible)
				ttek->show();
			ttek->slotActivateCurrents();
			if (!wasVisible)
				ttek->hide();
		} else
			ttek->theMainView->slotActivateAll();
	}
}

void Systray::slotQuit()
{
		ttek->save();
		ttek->writeSettings();
		qApp->quit();
}

void Systray::slotPrepareMenu()
{

	if (ttek->isVisible() && !ttek->isMinimized()) {
		restoreAction->setEnabled(false);
		minimizeAction->setEnabled(true);
	} else if (ttek->isHidden() || ttek->isMinimized()) {
		restoreAction->setEnabled(true);
		minimizeAction->setEnabled(false);
	}
}

void Systray::newTag(QString name)
{
	qDebug()<<"Systray::newTag"<<name;
	if (tagActions.contains(name))
		return; // already added

	QAction *tmp = tagMenu->addAction(name);
	tmp->setCheckable(true);
	if(!ttek)
		ttek = typotek::getInstance();
	QList<FontItem*> taggedFonts = FMFontDb::DB()->Fonts( name , FMFontDb::Tags );
	ttek->resetFilter();
	int nActivated(0);
	int nFonts(taggedFonts.count());
	for(int i = 0; i <  nFonts ; ++i)
	{
// 		qDebug()<<taggedFonts[i]->path();
		Q_ASSERT(taggedFonts[i]);
		if(taggedFonts[i]->isActivated())
		{
			++nActivated;
			
		}
	}
	if(nActivated < nFonts)
	{
		if(nActivated > 0)
		{
			QFont f(tmp->font());
			f.setBold(true);
			tmp->setFont(f);
			tmp->setText(name + " "+QString::number(nActivated)+"/"+QString::number(nFonts));
			tmp->setChecked(false);
		}
		else
		{
			tmp->setChecked(false);
		}
	}
	else
		tmp->setChecked(true);
	
	tagActions[name] = tmp;
}

void Systray::deleteTag(const QString &name)
{
	QAction *tmp = tagActions[name];
	if (tmp) {
		tagMenu->removeAction(tmp);
		tagActions.remove(name);
	}
}

void Systray::createActions()
{
    activateAllAction = new QAction(tr("&Activate all"), this);
    connect(activateAllAction, SIGNAL(triggered()), this, SLOT(slotActivateAll()));

    deactivateAllAction = new QAction(tr("&Deactivate all"), this);
    connect(deactivateAllAction, SIGNAL(triggered()), this, SLOT(slotDeactivateAll()));

    minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, SIGNAL(triggered()), this, SLOT(slotMinimize()));

    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(slotRestore()));

    quitAction = new QAction(tr("E&xit"), this);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(slotQuit()));
}

void Systray::createTrayIcon()
{
    trayIconMenu = new QMenu(0);
    trayIconMenu->addAction(activateAllAction);
    trayIconMenu->addAction(deactivateAllAction);
// 	trayIconMenu->addSeparator();
// 	tagSetMenu = trayIconMenu->addMenu(tr("&Collections"));
    tagMenu = trayIconMenu->addMenu(tr("&Tags"));
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->installEventFilter(this);
}

void Systray::createTagMenu()
{
	if (!ttek)
		ttek = typotek::getInstance();

	QStringList tmp(FMFontDb::DB()->getTags());
	tmp.sort();
	foreach (QString tagName, tmp) {
// 		if (tagName != "Activated_On" && tagName != "Activated_Off")
			newTag(tagName);
	}

	connect(tagMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotTagMenuClicked(QAction*)));
}

bool Systray::isVisible()
{
	return trayIcon->isVisible();
}

bool Systray::hasActivateAll()
{
	return activateAllAction->isVisible();
}

bool Systray::allConfirmation()
{
	return showAllConfirmation;
}

bool Systray::tagsConfirmation()
{
	return showTagsConfirmation;
}

void Systray::requireAllConfirmation(bool doRequire)
{
	showAllConfirmation = doRequire;
	settings->setValue("Systray/AllConfirmation", doRequire);
}

void Systray::requireTagsConfirmation(bool doRequire)
{
	showTagsConfirmation = doRequire;
	settings->setValue("Systray/TagsConfirmation", doRequire);
}

void Systray::updateTagMenu(QString nameOfFontWhichCausedThisUpdate)
{
	QStringList tags(tagActions.keys());
	bool lazy = true;
	foreach(QString tag, tags)
	{
		QList<FontItem*> taggedFonts = FMFontDb::DB()->Fonts( tag , FMFontDb::Tags );
		ttek->resetFilter();
		foreach(FontItem* fit, taggedFonts)
		{
			if(fit->path() == nameOfFontWhichCausedThisUpdate)// we’re concerned
				lazy = false;
		}
	}
	if(lazy)
		return;
	foreach(QString tag, tags)
	{
		deleteTag(tag);
	}
	
	if (!ttek)
		ttek = typotek::getInstance();

	QStringList tmp(FMFontDb::DB()->getTags());
	tmp.sort();
	foreach (QString tagName, tmp) {
// 		if (tagName != "Activated_On" && tagName != "Activated_Off")
			newTag(tagName);
	}
	
}

// bool Systray::eventFilter(QObject * watched, QEvent * event)
// {
// 	if (watched == trayIcon) {
// // 		qDebug() << event;
// 		}
// 	
// 	return Systray::eventFilter(watched, event);
// 	
// }




