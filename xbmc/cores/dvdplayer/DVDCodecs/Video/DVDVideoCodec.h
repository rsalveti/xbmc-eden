#pragma once

/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "system.h"
#include "threads/SingleLock.h"

#include <vector>

// when modifying these structures, make sure you update all codecs accordingly
#define FRAME_TYPE_UNDEF 0
#define FRAME_TYPE_I 1
#define FRAME_TYPE_P 2
#define FRAME_TYPE_B 3
#define FRAME_TYPE_D 4

namespace DXVA { class CSurfaceContext; }
namespace VAAPI { struct CHolder; }
class CDVDVideoCodec;
class CVDPAU;
class COpenMax;
class COpenMaxVideo;
struct OpenMaxVideoBuffer;
#ifdef HAVE_VIDEOTOOLBOXDECODER
  class CDVDVideoCodecVideoToolBox;
  struct __CVBuffer;
#endif

class EGLImageHandle
{
public:
  EGLImageHandle()
  {
  }
  virtual ~EGLImageHandle()
  {
  }
  virtual EGLImageKHR Get() = 0;
  virtual EGLImageHandle * Ref() = 0;
  virtual void UnRef() = 0;
};

// should be entirely filled by all codecs
struct DVDVideoPicture
{
  double pts; // timestamp in seconds, used in the CDVDPlayer class to keep track of pts
  double dts;

  union
  {
    struct {
      BYTE* data[4];      // [4] = alpha channel, currently not used
      int iLineSize[4];   // [4] = alpha channel, currently not used
    };
    struct {
      DXVA::CSurfaceContext* context;
    };
    struct {
      CVDPAU* vdpau;
    };
    struct {
      VAAPI::CHolder* vaapi;
    };

    struct {
      COpenMax *openMax;
      OpenMaxVideoBuffer *openMaxBuffer;
    };
#ifdef HAVE_VIDEOTOOLBOXDECODER
    struct {
      CDVDVideoCodecVideoToolBox *vtb;
      struct __CVBuffer *cvBufferRef;
    };
#endif
  };

  // XXX move this into union, and use instead of data/iLineSize!!
    struct {
      EGLImageHandle *eglImageHandle;
    };

  unsigned int iFlags;

  double       iRepeatPicture;
  double       iDuration;
  unsigned int iFrameType         : 4; // see defines above // 1->I, 2->P, 3->B, 0->Undef
  unsigned int color_matrix       : 4;
  unsigned int color_range        : 1; // 1 indicate if we have a full range of color
  unsigned int chroma_position;
  unsigned int color_primaries;
  unsigned int color_transfer;
  unsigned int extended_format;
  int iGroupId;

  int8_t* qscale_table; // Quantization parameters, primarily used by filters
  int qscale_stride;
  int qscale_type;

  unsigned int iWidth;
  unsigned int iHeight;
  unsigned int iDisplayX, iDisplayY;
  unsigned int iDisplayWidth;  // width of the picture without black bars
  unsigned int iDisplayHeight; // height of the picture without black bars

  enum EFormat {
    FMT_YUV420P = 0,
    FMT_VDPAU,
    FMT_NV12,
    FMT_UYVY,
    FMT_YUY2,
    FMT_DXVA,
    FMT_VAAPI,
    FMT_OMXEGL,
    FMT_CVBREF,
    FMT_EGLIMG,
  } format;
};

struct DVDVideoUserData
{
  BYTE* data;
  int size;
};

#define DVP_FLAG_TOP_FIELD_FIRST    0x00000001
#define DVP_FLAG_REPEAT_TOP_FIELD   0x00000002 //Set to indicate that the top field should be repeated
#define DVP_FLAG_ALLOCATED          0x00000004 //Set to indicate that this has allocated data
#define DVP_FLAG_INTERLACED         0x00000008 //Set to indicate that this frame is interlaced

#define DVP_FLAG_NOSKIP             0x00000010 // indicate this picture should never be dropped
#define DVP_FLAG_DROPPED            0x00000020 // indicate that this picture has been dropped in decoder stage, will have no data

// DVP_FLAG 0x00000100 - 0x00000f00 is in use by libmpeg2!

#define DVP_QSCALE_UNKNOWN          0
#define DVP_QSCALE_MPEG1            1
#define DVP_QSCALE_MPEG2            2
#define DVP_QSCALE_H264             3

class CDVDStreamInfo;
class CDVDCodecOption;
typedef std::vector<CDVDCodecOption> CDVDCodecOptions;

// VC_ messages, messages can be combined
#define VC_ERROR    0x00000001  // an error occured, no other messages will be returned
#define VC_BUFFER   0x00000002  // the decoder needs more data
#define VC_PICTURE  0x00000004  // the decoder got a picture, call Decode(NULL, 0) again to parse the rest of the data
#define VC_USERDATA 0x00000008  // the decoder found some userdata,  call Decode(NULL, 0) again to parse the rest of the data
#define VC_FLUSHED  0x00000010  // the decoder lost it's state, we need to restart decoding again
class CDVDVideoCodec
{
public:

  CDVDVideoCodec() {}
  virtual ~CDVDVideoCodec() {}

  /*
   * Open the decoder, returns true on success
   */
  virtual bool Open(CDVDStreamInfo &hints, CDVDCodecOptions &options) = 0;

  /*
   * Dispose, Free all resources
   */
  virtual void Dispose() = 0;

  /*
   * returns one or a combination of VC_ messages
   * pData and iSize can be NULL, this means we should flush the rest of the data.
   */
  virtual int Decode(BYTE* pData, int iSize, double dts, double pts) = 0;

 /*
   * Reset the decoder.
   * Should be the same as calling Dispose and Open after each other
   */
  virtual void Reset() = 0;

  /*
   * returns true if successfull
   * the data is valid until the next Decode call
   */
  virtual bool GetPicture(DVDVideoPicture* pDvdVideoPicture) = 0;


  /*
   * returns true if successfull
   * the data is cleared to zero
   */ 
  virtual bool ClearPicture(DVDVideoPicture* pDvdVideoPicture)
  {
    if (pDvdVideoPicture->eglImageHandle)
      pDvdVideoPicture->eglImageHandle->UnRef();
    memset(pDvdVideoPicture, 0, sizeof(DVDVideoPicture));
    return true;
  }

  /*
   * returns true if successfull
   * the data is valid until the next Decode call
   * userdata can be anything, for now we use it for closed captioning
   */
  virtual bool GetUserData(DVDVideoUserData* pDvdVideoUserData)
  {
    pDvdVideoUserData->data = NULL;
    pDvdVideoUserData->size = 0;
    return false;
  }

  /*
   * will be called by video player indicating if a frame will eventually be dropped
   * codec can then skip actually decoding the data, just consume the data set picture headers
   */
  virtual void SetDropState(bool bDrop) = 0;


  enum EFilterFlags {
    FILTER_NONE                =  0x0,
    FILTER_DEINTERLACE_YADIF   =  0x1,  /* use first deinterlace mode */
    FILTER_DEINTERLACE_ANY     =  0xf,  /* use any deinterlace mode */
    FILTER_DEINTERLACE_FLAGGED = 0x10,  /* only deinterlace flagged frames */
    FILTER_DEINTERLACE_HALFED  = 0x20,  /* do half rate deinterlacing */
  };

  /*
   * set the type of filters that should be applied at decoding stage if possible
   */
  virtual unsigned int SetFilters(unsigned int filters) { return 0u; }

  /*
   *
   * should return codecs name
   */
  virtual const char* GetName() = 0;

  /*
   *
   * How many packets should player remember, so codec
   * can recover should something cause it to flush
   * outside of players control
   */
  virtual unsigned GetConvergeCount()
  {
    return 0;
  }
};
