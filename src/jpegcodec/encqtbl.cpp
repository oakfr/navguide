/*
//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//    Copyright (c) 2001-2006 Intel Corporation. All Rights Reserved.
//
*/

#include "precomp.h"

#ifndef __ENCQTBL_H__
#include "encqtbl.h"
#endif




CJPEGEncoderQuantTable::CJPEGEncoderQuantTable(void)
{
  m_id          = 0;
  m_precision   = 0;
  m_initialized = 0;

  m_raw = (Ipp8u*)OWN_ALIGN_PTR(m_rbf,32);
  m_qnt = (Ipp16u*)OWN_ALIGN_PTR(m_qbf,32); // align for max performance

  ippsZero_8u(m_raw,DCTSIZE2*sizeof(Ipp8u));
  ippsZero_8u((Ipp8u*)m_qnt,DCTSIZE2*sizeof(Ipp16u));

  return;
} // ctor


CJPEGEncoderQuantTable::~CJPEGEncoderQuantTable(void)
{
  m_initialized = 0;

  ippsZero_8u(m_raw,sizeof(m_raw));
  ippsZero_8u((Ipp8u*)m_qnt,sizeof(m_qnt));

  return;
} // dtor


JERRCODE CJPEGEncoderQuantTable::Init(int id,int quality,Ipp8u raw[64])
{
  IppStatus status;

  m_id        = id  & 0x0f;
  m_precision = (id & 0xf0) >> 4;

  ippsCopy_8u(raw,m_raw,DCTSIZE2);

  status = ippiQuantFwdRawTableInit_JPEG_8u(m_raw,quality);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantFwdRawTableInit_JPEG_8u() failed - ",status);
    return JPEG_INTERNAL_ERROR;
  }

  status = ippiQuantFwdTableInit_JPEG_8u16u(m_raw,m_qnt);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantFwdTableInit_JPEG_8u() failed - ",status);
    return JPEG_INTERNAL_ERROR;
  }

  m_initialized = 1;

  return JPEG_OK;
} // CJPEGEncoderQuantTable::Init()

