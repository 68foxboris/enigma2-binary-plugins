/*
 *      Copyright (C) 2011-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "EGLNativeTypeDreambox.h"
#include "guilib/gui3d.h"
#include "utils/DreamboxUtils.h"
#include "utils/StringUtils.h"
#include "platform/linux/SysfsPath.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <EGL/egl.h>

CEGLNativeTypeDreambox::CEGLNativeTypeDreambox()
{
  m_nativeWindow = (XBNativeWindowType)0L;
}

CEGLNativeTypeDreambox::~CEGLNativeTypeDreambox()
{
}

bool CEGLNativeTypeDreambox::CheckCompatibility()
{
  CSysfsPath model{"/proc/stb/info/model"};
  std::string name = model.Get<std::string>();
  return name == "dm820" || name == "dm900" || name == "dm920" || name == "dm7080";
}

void CEGLNativeTypeDreambox::Initialize()
{
}

void CEGLNativeTypeDreambox::Destroy()
{
}

bool CEGLNativeTypeDreambox::CreateNativeDisplay()
{
  m_nativeDisplay = EGL_DEFAULT_DISPLAY;
  return true;
}

bool CEGLNativeTypeDreambox::CreateNativeWindow()
{
  RESOLUTION_INFO res;
  if (GetNativeResolution(&res))
    SetFramebufferResolution(res.iWidth, res.iHeight);
  else
    SetFramebufferResolution(1280, 720);
  return true;
}

bool CEGLNativeTypeDreambox::GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const
{
  if (!nativeDisplay)
    return false;
  *nativeDisplay = (XBNativeDisplayType *)&m_nativeDisplay;
  return true;
}

bool CEGLNativeTypeDreambox::GetNativeWindow(XBNativeWindowType **nativeWindow) const
{
  if (!nativeWindow)
    return false;
  *nativeWindow = (XBNativeWindowType *)&m_nativeWindow;
  return true;
}

bool CEGLNativeTypeDreambox::DestroyNativeDisplay()
{
  return true;
}

bool CEGLNativeTypeDreambox::DestroyNativeWindow()
{
  return true;
}

bool CEGLNativeTypeDreambox::GetNativeResolution(RESOLUTION_INFO *res) const
{
  std::string mode;
  CSysfsPath videomode{"/proc/stb/video/videomode"};
  mode = videomode.Get<std::string>().value_or("");
  return dreambox_mode_to_resolution(mode.c_str(), res);
}

bool CEGLNativeTypeDreambox::SetNativeResolution(const RESOLUTION_INFO &res)
{
  // Don't set the same mode as current
  std::string mode;
  CSysfsPath videomode{"/proc/stb/video/videomode"};
  mode = videomode.Get<std::string>().value_or("");
  if (res.strId == mode)
    return false;

  return SetDisplayResolution(res.strId.c_str());
}

bool CEGLNativeTypeDreambox::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  std::string valstr;
  CSysfsPath videomode_choices{"/proc/stb/video/videomode_choices"};
  valstr = videomode_choices.Get<std::string>().value_or("");
  std::vector<std::string> probe_str = StringUtils::Split(valstr, " ");

  resolutions.clear();
  RESOLUTION_INFO res;
  for (std::vector<std::string>::const_iterator i = probe_str.begin(); i != probe_str.end(); ++i)
  {
    if (dreambox_mode_to_resolution(i->c_str(), &res))
      resolutions.push_back(res);
  }

  return resolutions.size() > 0;
}

bool CEGLNativeTypeDreambox::GetPreferredResolution(RESOLUTION_INFO *res) const
{
  // check display/mode, it gets defaulted at boot
  if (!GetNativeResolution(res))
  {
    // punt to 720p if we get nothing
    dreambox_mode_to_resolution("720p", res);
  }

  return true;
}

bool CEGLNativeTypeDreambox::ShowWindow(bool show)
{
  CSysfsPath path("/proc/stb/video/alpha", show ? 255 : 0);
  return true;
}

bool CEGLNativeTypeDreambox::SetDisplayResolution(const char *mode)
{
  // switch display resolution
  CSysfsPath videomode{"/proc/stb/video/videomode"};
  mode = &videomode.Get<std::string>()[0];

  RESOLUTION_INFO res;
  dreambox_mode_to_resolution(mode, &res);
  SetFramebufferResolution(res);

  return true;
}

void CEGLNativeTypeDreambox::SetFramebufferResolution(const RESOLUTION_INFO &res) const
{
  SetFramebufferResolution(res.iScreenWidth, res.iScreenHeight);
}

void CEGLNativeTypeDreambox::SetFramebufferResolution(int width, int height) const
{
  const char fbdev[] = "/dev/fb0";
  int fd;

  fd = open(fbdev, O_RDWR | O_CLOEXEC);
  if (fd >= 0)
  {
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0)
    {
      vinfo.xres = width;
      vinfo.yres = height;
      vinfo.xres_virtual = width;
      vinfo.yres_virtual = height * 2;
      vinfo.bits_per_pixel = 32;
      vinfo.activate = FB_ACTIVATE_ALL;
      ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
    }
    close(fd);
  }
}

