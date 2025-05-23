/***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************/
/**
 * \file
 * GRIB Weather Data Plugin for OpenCPN.
 *
 * Primary plugin interface for the GRIB weather data visualization system.
 * This plugin enables OpenCPN to display weather forecasts from GRIB files,
 * providing mariners with critical meteorological data including:
 * - Wind speed and direction
 * - Pressure systems and isobars
 * - Wave height, direction and period
 * - Precipitation and cloud cover
 * - Temperature and humidity
 *
 * The plugin supports both GRIB1 and GRIB2 file formats, allows temporal
 * interpolation between forecast times, and provides various visualization
 * options including wind barbs, particle animations, and color-coded overlays.
 */
#ifndef _GRIBPI_H_
#define _GRIBPI_H_

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#include <wx/glcanvas.h>
#endif  // precompiled headers

#define PLUGIN_VERSION_MAJOR 5
#define PLUGIN_VERSION_MINOR 0

#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 16

#include "ocpn_plugin.h"

#include "wx/jsonreader.h"
#include "wx/jsonwriter.h"

#include "GribSettingsDialog.h"
#include "GribOverlayFactory.h"
#include "GribUIDialog.h"

class GribPreferencesDialog;

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

#define GRIB_TOOL_POSITION -1  // Request default positioning of ToolBar tool
#define STARTING_STATE_STYLE 9999  // style option undifined
#define ATTACHED 0                 // dialog are attached
#define SEPARATED 1                // dialog are separated
#define ATTACHED_HAS_CAPTION 0     // dialog attached  has a caption
#define ATTACHED_NO_CAPTION 1      // dialog attached don't have caption
#define SEPARATED_HORIZONTAL 2     // dialog separated shown honrizontaly
#define SEPARATED_VERTICAL 3       // dialog separated shown vaerticaly

enum SettingsDisplay {
  B_ARROWS,
  ISO_LINE,
  ISO_ABBR,
  ISO_LINE_SHORT,
  ISO_LINE_VISI,
  D_ARROWS,
  OVERLAY,
  NUMBERS,
  PARTICLES
};

class grib_pi : public opencpn_plugin_116 {
public:
  grib_pi(void *ppimgr);
  ~grib_pi(void);

  //    The required PlugIn Methods
  int Init(void);
  bool DeInit(void);

  int GetAPIVersionMajor();
  int GetAPIVersionMinor();
  int GetPlugInVersionMajor();
  int GetPlugInVersionMinor();
  wxBitmap *GetPlugInBitmap();
  wxString GetCommonName();
  wxString GetShortDescription();
  wxString GetLongDescription();

  //    The override PlugIn Methods
  bool MouseEventHook(wxMouseEvent &event);
  bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
  bool RenderOverlayMultiCanvas(wxDC &dc, PlugIn_ViewPort *vp, int canvasIndex);
  void SetCursorLatLon(double lat, double lon);
  void OnContextMenuItemCallback(int id);
  void SetPluginMessage(wxString &message_id, wxString &message_body);
  bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);
  bool RenderGLOverlayMultiCanvas(wxGLContext *pcontext, PlugIn_ViewPort *vp,
                                  int canvasIndex);
  void SendTimelineMessage(wxDateTime time);
  void SetDefaults(void);
  int GetToolBarToolCount(void);
  void ShowPreferencesDialog(wxWindow *parent);
  void OnToolbarToolCallback(int id);
  bool QualifyCtrlBarPosition(wxPoint position, wxSize size);
  void MoveDialog(wxDialog *dialog, wxPoint position);
  void SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix);

  // Other public methods
  void SetCtrlBarXY(wxPoint p) { m_CtrlBarxy = p; }
  void SetCursorDataXY(wxPoint p) { m_CursorDataxy = p; }
  void SetCtrlBarSizeXY(wxSize p) { m_CtrlBar_Sizexy = p; }
  void SetColorScheme(PI_ColorScheme cs);
  void SetDialogFont(wxWindow *window, wxFont *font = OCPNGetFont(_("Dialog")));
  /**
   * Callback invoked by OpenCPN core whenever the current ViewPort changes or
   * through periodic updates.
   *
   * In multi-canvas configurations, each canvas triggers a viewport update.
   */
  void SetCurrentViewPort(PlugIn_ViewPort &vp) { m_current_vp = vp; }
  PlugIn_ViewPort &GetCurrentViewPort() { return m_current_vp; }

  void OnGribCtrlBarClose();

  wxPoint GetCtrlBarXY() { return m_CtrlBarxy; }
  wxPoint GetCursorDataXY() { return m_CursorDataxy; }
  const wxString GetTimezoneSelector() {
    switch (m_bTimeZone) {
      case 0:
        return "UTC";
      case 1:
        return "Local Time";
      default:
        return wxEmptyString;
    }
  }
  void SetTimeZone(int tz);
  int GetStartOptions() { return m_bStartOptions; }
  /**
   * Returns true if cumulative parameters like precipitation and cloud cover
   * should initialize their start values from the first record.
   *
   * This avoids artificial zero values at the beginning of the time series.
   */
  bool GetCopyFirstCumRec() { return m_bCopyFirstCumRec; }
  /**
   * Returns true if wave data should be propagated across time periods where
   * wave records are missing.
   *
   * This ensures continuity of wave visualization even when data points are
   * sparse.
   */
  bool GetCopyMissWaveRec() { return m_bCopyMissWaveRec; }

  GRIBOverlayFactory *m_pGRIBOverlayFactory;
  GRIBOverlayFactory *GetGRIBOverlayFactory() { return m_pGRIBOverlayFactory; }

  void UpdatePrefs(GribPreferencesDialog *Pref);

  int m_MenuItem;
  bool m_DialogStyleChanged;

  wxSize m_coreToolbarSize;
  wxPoint m_coreToolbarPosn;
  bool m_bZoomToCenterAtInit;
  wxString m_local_sources_catalog;
  double m_boat_lat, m_boat_lon;
  double m_boat_cog, m_boat_sog;
  time_t m_boat_time;

private:
  bool LoadConfig(void);
  bool SaveConfig(void);

  bool DoRenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp,
                         int canvasIndex);
  bool DoRenderOverlay(wxDC &dc, PlugIn_ViewPort *vp, int canvasIndex);

  wxFileConfig *m_pconfig;
  wxWindow *m_parent_window;

  GRIBUICtrlBar *m_pGribCtrlBar;

  int m_display_width, m_display_height;
  int m_leftclick_tool_id;

  wxPoint m_CtrlBarxy, m_CursorDataxy;
  wxSize m_CtrlBar_Sizexy;

  //    Controls added to Preferences panel
  wxCheckBox *m_pGRIBUseHiDef;
  wxCheckBox *m_pGRIBUseGradualColors;

  GribTimelineRecordSet *m_pLastTimelineSet;

  // preference data
  bool m_bGRIBUseHiDef;
  bool m_bGRIBUseGradualColors;
  bool m_bDrawBarbedArrowHead;
  int m_bTimeZone;
  /** Controls whether cumulative parameters like precipitation and cloud cover
   * should initialize their start values from the first record. This avoids
   * artificial zero values at the beginning of the time series.
   */
  bool m_bCopyFirstCumRec;
  /** Controls propagation of wave data across time periods where wave records
   * are missing. This ensures continuity of wave visualization even when data
   * points are sparse.
   */
  bool m_bCopyMissWaveRec;
  int m_bLoadLastOpenFile;
  int m_bStartOptions;
  wxString m_RequestConfig;
  wxString m_bMailToAddresses;
  wxString m_bMailFromAddress;
  wxString m_ZyGribLogin;
  wxString m_ZyGribCode;
  double m_GUIScaleFactor;
#ifdef __WXMSW__
  double m_GribIconsScaleFactor;
#endif
  bool m_bGRIBShowIcon;

  bool m_bShowGrib;
  /**
   * Stores current viewport.
   *
   * In multi-canvas configurations, each canvas triggers independent viewport
   * updates.
   */
  PlugIn_ViewPort m_current_vp;
  wxBitmap m_panelBitmap;
};

//----------------------------------------------------------------------------------------
// Preference dialog definition
//----------------------------------------------------------------------------------------

class GribPreferencesDialog : public GribPreferencesDialogBase {
public:
  GribPreferencesDialog(wxWindow *pparent)
      : GribPreferencesDialogBase(pparent) {}
  ~GribPreferencesDialog() {}

  void OnOKClick(wxCommandEvent &event);

private:
  void OnStartOptionChange(wxCommandEvent &event);
};
#endif
