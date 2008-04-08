/*
 * Copyright (c) 2000, 2001 Red Hat, Inc.
 * Copyright (c) 2003 Robert Collins <rbtcollins@hotmail.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to let the user choose which packages
   to install, and which versions of the package when more than one
   version is provided.  The "trust" level serves as an indication as
   to which version should be the default choice.  At the moment, all
   we do is compare with previously installed packages to skip any
   that are already installed (by setting the action to ACTION_SAME).
   While the "trust" stuff is supported, it's not really implemented
   yet.  We always prefer the "current" option.  In the future, this
   file might have a user dialog added to let the user choose to not
   install packages, or to install packages that aren't installed by
   default. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <algorithm>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "LogSingleton.h"
#include "filemanip.h"
#include "io_stream.h"
#include "propsheet.h"
#include "choose.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"

#include "threebar.h"
#include "Generic.h"
#include "ControlAdjuster.h"
#include "prereq.h"

using namespace std;

extern ThreeBarProgressPage Progress;

/*
  Sizing information.
 */
static ControlAdjuster::ControlInfo ChooserControlsInfo[] = {
  {IDC_CHOOSE_KEEP, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_PREV, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_CURR, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_EXP, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEW, 		CP_RIGHT,   CP_TOP},
  {IDC_LISTVIEW_POS, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEWCAPTION,	CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_LIST,		CP_STRETCH, CP_STRETCH},
  {IDC_CHOOSE_HIDE,             CP_LEFT,    CP_BOTTOM},
  {0, CP_LEFT, CP_TOP}
};

ChooserPage::ChooserPage ()
{
  sizeProcessor.AddControlInfo (ChooserControlsInfo);
}

void
ChooserPage::createListview ()
{
  packagedb db;
  chooser = new PickView (*db.categories.find("All"));
  RECT r = getDefaultListViewSize();
  if (!chooser->Create(this, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,&r))
    // TODO throw exception
    exit (11);
  chooser->init(PickView::views::Category);
  chooser->Show(SW_SHOW);

  chooser->defaultTrust (TRUST_CURR);
  chooser->setViewMode (PickView::views::Category);
  if (!SetDlgItemText (GetHWND (), IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    log (LOG_BABBLE) << "Failed to set View button caption %ld" << 
	 GetLastError () << endLog;
  for_each (db.packages.begin(), db.packages.end(), bind2nd(mem_fun(&packagemeta::set_requirements), chooser->deftrust));
  /* FIXME: do we need to init the desired fields ? */
  static int ta[] = { IDC_CHOOSE_KEEP, IDC_CHOOSE_PREV, IDC_CHOOSE_CURR, IDC_CHOOSE_EXP, 0 };
  rbset (GetHWND (), ta, IDC_CHOOSE_CURR);
}

/* TODO: review ::overrides for possible consolidation */
void
ChooserPage::getParentRect (HWND parent, HWND child, RECT * r)
{
  POINT p;
  ::GetWindowRect (child, r);
  p.x = r->left;
  p.y = r->top;
  ::ScreenToClient (parent, &p);
  r->left = p.x;
  r->top = p.y;
  p.x = r->right;
  p.y = r->bottom;
  ::ScreenToClient (parent, &p);
  r->right = p.x;
  r->bottom = p.y;
}

bool
ChooserPage::Create ()
{
    return PropertyPage::Create (IDD_CHOOSE);
}

void
ChooserPage::setPrompt(char const *aString)
{
  ::SetWindowText (GetDlgItem (IDC_CHOOSE_INST_TEXT), aString);
}

RECT
ChooserPage::getDefaultListViewSize()
{
  RECT result;
  getParentRect (GetHWND (), GetDlgItem (IDC_LISTVIEW_POS), &result);
  result.top += 2;
  result.bottom -= 2;
  return result;
}

void
ChooserPage::OnInit ()
{
  CheckDlgButton (GetHWND (), IDC_CHOOSE_HIDE, BST_CHECKED);

  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_CWD)
    packagemeta::ScanDownloadedFiles ();

  packagedb db;
  db.setExistence ();
  db.fillMissingCategory ();

  if (source == IDC_SOURCE_DOWNLOAD)
    setPrompt("Select packages to download ");
  else
    setPrompt("Select packages to install ");
  createListview ();
  
  AddTooltip (IDC_CHOOSE_KEEP, IDS_TRUSTKEEP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_PREV, IDS_TRUSTPREV_TOOLTIP);
  AddTooltip (IDC_CHOOSE_CURR, IDS_TRUSTCURR_TOOLTIP);
  AddTooltip (IDC_CHOOSE_EXP, IDS_TRUSTEXP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_VIEW, IDS_VIEWBUTTON_TOOLTIP);  
  AddTooltip (IDC_CHOOSE_HIDE, IDS_HIDEOBS_TOOLTIP);
}

void
ChooserPage::OnActivate()
{
    chooser->refresh();;
}

void
ChooserPage::logResults()
{
  log (LOG_BABBLE) << "Chooser results..." << endLog;
  packagedb db;
  for_each (db.packages.begin (), db.packages.end (), mem_fun(&packagemeta::logSelectionStatus));
}

long
ChooserPage::OnNext ()
{
#ifdef DEBUG
  logResults();
#endif

  PrereqChecker p;
  if (p.isMet ())
    {
      if (source == IDC_SOURCE_CWD)
        {
          // Next, install
          Progress.SetActivateTask (WM_APP_START_INSTALL);
        }
      else
        {
          // Next, start download from internet
          Progress.SetActivateTask (WM_APP_START_DOWNLOAD);
        }
      return IDD_INSTATUS;
    }
  else
    {
      // rut-roh, some required things are not selected
      return IDD_PREREQ;
    }
}

long
ChooserPage::OnBack ()
{
  if (source == IDC_SOURCE_CWD)
    return IDD_LOCAL_DIR;
  else
    return IDD_SITE;
}

void
ChooserPage::keepClicked()
{
  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
        i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      pkg.desired = pkg.installed;
    }
  chooser->refresh();
}

void
ChooserPage::changeTrust(trusts aTrust)
{
  chooser->defaultTrust (aTrust);
  packagedb db;
  db.markUnVisited ();
  for_each (db.packages.begin (), db.packages.end (),
            bind2nd (mem_fun (&packagemeta::set_requirements), aTrust));
  chooser->refresh();
  PrereqChecker p;
  p.setTrust (aTrust);
}

bool
ChooserPage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  if (code != BN_CLICKED)
    {
      // Not a click notification, we don't care.
      return false;
    }

  switch (id)
    {
    case IDC_CHOOSE_KEEP:
      if (IsButtonChecked (id))
        keepClicked();
      break;
      
    case IDC_CHOOSE_PREV:
      if (IsButtonChecked (id))
        changeTrust (TRUST_PREV);
      break;
      
    case IDC_CHOOSE_CURR:
      if (IsButtonChecked (id))
        changeTrust (TRUST_CURR);
      break;
      
    case IDC_CHOOSE_EXP:
      if (IsButtonChecked (id))
        changeTrust (TRUST_TEST);
      break;

    case IDC_CHOOSE_VIEW:
      chooser->cycleViewMode ();
      if (!SetDlgItemText
        (GetHWND (), IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
      log (LOG_BABBLE) << "Failed to set View button caption " << 
           GetLastError () << endLog;
      break;
      
    case IDC_CHOOSE_HIDE:
      chooser->setObsolete (!IsButtonChecked (id));
      break;
    default:
      // Wasn't recognized or handled.
      return false;
    }

  // Was handled since we never got to default above.
  return true;
}

BOOL CALLBACK
ChooserPage::OnMouseWheel (UINT message, WPARAM wParam, LPARAM lParam)
{
  return chooser->WindowProc (message, wParam, lParam);
}
