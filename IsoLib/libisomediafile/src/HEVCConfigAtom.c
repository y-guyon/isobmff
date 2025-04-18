/**
 * @file HEVCConfigAtom.c
 * @brief Created for Nokia FAVS project by Tampere University of Technology
 * @version 0.1
 *
 * @copyright This software module was originally developed by Apple Computer, Inc. in the course of
 * development of MPEG-4. This software module is an implementation of a part of one or more MPEG-4
 * tools as specified by MPEG-4. ISO/IEC gives users of MPEG-4 free license to this software module
 * or modifications thereof for use in hardware or software products claiming conformance to MPEG-4.
 * Those intending to use this software module in hardware or software products are advised that its
 * use may infringe existing patents. The original developer of this software module and his/her
 * company, the subsequent editors and their companies, and ISO/IEC have no liability for use of
 * this software module or modifications thereof in an implementation. Copyright is not released for
 * non MPEG-4 conforming products. Apple Computer, Inc. retains full right to use the code for its
 * own purpose, assign or donate the code to a third party and to inhibit third parties from using
 * the code for non MPEG-4 conforming products. This copyright notice must be included in all copies
 * or derivative works. Copyright (c) 1999.
 *
 */

#include "MP4Atoms.h"
#include "MP4Descriptors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void destroy(MP4AtomPtr s)
{
  MP4Err err;
  u32 i;
  ISOHEVCConfigAtomPtr self;
  err  = MP4NoErr;
  self = (ISOHEVCConfigAtomPtr)s;
  if(self == NULL) BAILWITHERROR(MP4BadParamErr)
  if(self->numOfArrays)
  {
    for(i = 0; i < self->numOfArrays; i++)
    {
      err = MP4DeleteLinkedList(self->arrays[i].nalList);
      if(err) goto bail;
      self->arrays[i].nalList = NULL;
    }
  }

  if(self->super) self->super->destroy(s);
bail:
  TEST_RETURN(err);
  return;
}

static MP4Err serialize(struct MP4Atom *s, char *buffer)
{
  MP4Err err;
  u32 x, i, array_index;
  ISOHEVCConfigAtomPtr self = (ISOHEVCConfigAtomPtr)s;
  err                       = MP4NoErr;

  err = MP4SerializeCommonBaseAtomFields((MP4AtomPtr)s, buffer);
  if(err) goto bail;
  buffer += self->bytesWritten;

  /* config_version */
  x = 1;
  PUT8_V(x);

  /* general_profile_space(2) + general_tier_flag(1) + general_profile_idc(5) */
  x = self->general_profile_idc;
  PUT8_V(x);
  PUT32(general_profile_compatibility_flags);
  /* general_constraint_indicator_flags (48) */
  x = 0;
  PUT32_V(x);
  PUT16_V(x);

  PUT8(general_level_idc);

  /* reserved '1111'b + min_spatial_segmentation_idc (12) */
  x = (0xf << 12);
  PUT16_V(x);

  /* reserved '111111'b + parallelismType (2) */
  x = (0x3f << 2);
  PUT8_V(x);

  /* reserved '111111'b + chromaFormat (2) */
  x = (0x3f << 2) | self->chroma_format_idc;
  PUT8_V(x);

  /* reserved '11111'b + bitDepthLumaMinus8 (3) */
  x = (0x1f << 3) | self->bit_depth_luma_minus8;
  PUT8_V(x);

  /* reserved '11111'b + bitDepthChromaMinus8 (3) */
  x = (0x1f << 3) | self->bit_depth_chroma_minus8;
  PUT8_V(x);

  /* avgFrameRate */
  PUT16(avgFrameRate);

  /* constantFrameRate(2) + numTemporalLayers(3) + temporalIdNested(1) + lengthSizeMinusOne(2) */
  x = (1 << 3) | (self->temporalIdNested << 2) | (self->lengthSizeMinusOne & 3);
  PUT8_V(x);

  PUT8(numOfArrays);

  for(array_index = 0; array_index < self->numOfArrays; array_index++)
  {
    u32 count;
    err = MP4GetListEntryCount(self->arrays[array_index].nalList, &count);
    if(err) goto bail;
    x =
      (self->arrays[array_index].array_completeness << 7) | self->arrays[array_index].NAL_unit_type;
    PUT8_V(x);

    PUT16_V(count);

    for(i = 0; i < count; i++)
    {
      MP4Handle b;
      u32 the_size;
      err = MP4GetListEntry(self->arrays[array_index].nalList, i, (char **)&b);
      if(err) goto bail;
      err = MP4GetHandleSize(b, &the_size);
      if(err) goto bail;
      PUT16_V(the_size);
      PUTBYTES(*b, the_size);
    }
  }

  assert(self->bytesWritten == self->size);
bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err calculateSize(struct MP4Atom *s)
{
  MP4Err err;
  ISOHEVCConfigAtomPtr self = (ISOHEVCConfigAtomPtr)s;
  u32 i, ii;
  err = MP4NoErr;

  err = MP4CalculateBaseAtomFieldSize((MP4AtomPtr)s);
  if(err) goto bail;
  self->size += 23;

  if(self->numOfArrays)
  {
    self->size += 3 * self->numOfArrays;
    for(i = 0; i < self->numOfArrays; i++)
    {
      u32 count;
      err = MP4GetListEntryCount(self->arrays[i].nalList, &count);
      if(err) goto bail;
      if(count >> 5) BAILWITHERROR(MP4BadParamErr);

      for(ii = 0; ii < count; ii++)
      {
        MP4Handle b;
        u32 the_size;
        err = MP4GetListEntry(self->arrays[i].nalList, ii, (char **)&b);
        if(err) goto bail;
        err = MP4GetHandleSize(b, &the_size);
        if(err) goto bail;
        self->size += 2 + the_size;
      }
    }
  }

bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream)
{
  MP4Err err;
  ISOHEVCConfigAtomPtr self = (ISOHEVCConfigAtomPtr)s;
  u32 x, i, array_index;
  char debug_buffer[70];

  err = MP4NoErr;
  if(self == NULL) BAILWITHERROR(MP4BadParamErr)
  err = self->super->createFromInputStream(s, proto, (char *)inputStream);
  if(err) goto bail;

  GET8(configurationVersion);
  if(self->configurationVersion != 1) BAILWITHERROR(MP4BadDataErr);

  /* general_profile_space(2) + general_tier_flag(1) + general_profile_idc(5) */
  GET8_V_NOMSG(x);
  self->general_profile_space = (x & 0xC0) >> 6;
  self->general_tier_flag     = (x & 0x20) >> 5;
  self->general_profile_idc   = x & 0x1f;
  snprintf(debug_buffer, sizeof(debug_buffer), "general_profile_space = %d",
           self->general_profile_space);
  DEBUG_MSG(debug_buffer);
  snprintf(debug_buffer, sizeof(debug_buffer), "general_tier_flag = %d", self->general_tier_flag);
  DEBUG_MSG(debug_buffer);
  snprintf(debug_buffer, sizeof(debug_buffer), "general_profile_idc = %d",
           self->general_profile_idc);
  DEBUG_MSG(debug_buffer);

  GET32_V_NOMSG(x);
  self->general_profile_compatibility_flags = x;
  snprintf(debug_buffer, sizeof(debug_buffer), "general_profile_compatibility_flags = 0x%08X",
           self->general_profile_compatibility_flags);
  DEBUG_MSG(debug_buffer);

  /* general_constraint_indicator_flags (48) */
  GET32_V_NOMSG(x);
  self->general_constraint_indicator_flags = (u64)x << 16; /* MSB (upper 32 bits) */
  GET16_V_NOMSG(x);
  self->general_constraint_indicator_flags |= x; /* LSB (lower 16 bits) */
  snprintf(debug_buffer, sizeof(debug_buffer), "general_constraint_indicator_flags = 0x%012llX",
           self->general_constraint_indicator_flags);
  DEBUG_MSG(debug_buffer);

  /* general_level_idc (8) */
  GET8_V_NOMSG(x);
  self->general_level_idc = x;
  snprintf(debug_buffer, sizeof(debug_buffer), "general_level_idc = %d (Level %.1f)",
           self->general_level_idc, self->general_level_idc / 30.0);
  DEBUG_MSG(debug_buffer);

  /* reserved '1111'b + min_spatial_segmentation_idc (12) */
  GET16_V_NOMSG(x);
  self->min_spatial_segmentation_idc = x & 0x0FFF;
  snprintf(debug_buffer, sizeof(debug_buffer), "min_spatial_segmentation_idc = %d",
           self->min_spatial_segmentation_idc);
  DEBUG_MSG(debug_buffer);

  /* reserved '111111'b + parallelismType (2) */
  GET8_V_NOMSG(x);
  self->parallelismType = x & 0x3;
  snprintf(debug_buffer, sizeof(debug_buffer), "parallelismType = %d", self->parallelismType);
  DEBUG_MSG(debug_buffer);

  /* reserved '111111'b + chromaFormat (2) */
  GET8_V_NOMSG(x);
  self->chroma_format_idc = x & 0x3;
  snprintf(debug_buffer, sizeof(debug_buffer), "chroma_format_idc = %d", self->chroma_format_idc);
  DEBUG_MSG(debug_buffer);

  /* reserved '11111'b + bitDepthLumaMinus8 (3) */
  GET8_V_NOMSG(x);
  self->bit_depth_luma_minus8 = x & 0x7;
  snprintf(debug_buffer, sizeof(debug_buffer), "bit_depth_luma_minus8 = %d (%d bits)",
           self->bit_depth_luma_minus8, self->bit_depth_luma_minus8 + 8);
  DEBUG_MSG(debug_buffer);

  /* reserved '11111'b + bitDepthChromaMinus8 (3) */
  GET8_V_NOMSG(x);
  self->bit_depth_chroma_minus8 = x & 0x7;
  snprintf(debug_buffer, sizeof(debug_buffer), "bit_depth_chroma_minus8 = %d (%d bits)",
           self->bit_depth_chroma_minus8, self->bit_depth_chroma_minus8 + 8);
  DEBUG_MSG(debug_buffer);

  /* avgFrameRate */
  GET16(avgFrameRate);

  /* constantFrameRate (2) + numTemporalLayers (3) + temporalIdNested (1) + lengthSizeMinusOne (2)
   */
  GET8_V_NOMSG(x);
  self->constantFrameRate  = (x >> 6) & 0x3;
  self->numTemporalLayers  = (x >> 3) & 0x7;
  self->temporalIdNested   = (x >> 2) & 1;
  self->lengthSizeMinusOne = x & 0x3;
  snprintf(debug_buffer, sizeof(debug_buffer), "constantFrameRate = %d", self->constantFrameRate);
  DEBUG_MSG(debug_buffer);
  snprintf(debug_buffer, sizeof(debug_buffer), "numTemporalLayers = %d", self->numTemporalLayers);
  DEBUG_MSG(debug_buffer);
  snprintf(debug_buffer, sizeof(debug_buffer), "temporalIdNested = %d", self->temporalIdNested);
  DEBUG_MSG(debug_buffer);
  snprintf(debug_buffer, sizeof(debug_buffer), "lengthSizeMinusOne = %d (%d bytes)",
           self->lengthSizeMinusOne, self->lengthSizeMinusOne + 1);
  DEBUG_MSG(debug_buffer);

  GET8(numOfArrays);
  for(array_index = 0; array_index < self->numOfArrays; array_index++)
  {
    GET8_V_NOMSG(x);
    self->arrays[array_index].array_completeness = (x & 0x80) ? 1 : 0;
    self->arrays[array_index].NAL_unit_type      = x & 0x3f;
    snprintf(debug_buffer, sizeof(debug_buffer), "--- Array %d ---", array_index);
    DEBUG_MSG(debug_buffer);
    snprintf(debug_buffer, sizeof(debug_buffer), "array_completeness = %d",
             self->arrays[array_index].array_completeness);
    DEBUG_MSG(debug_buffer);
    snprintf(debug_buffer, sizeof(debug_buffer), "NAL_unit_type = %d",
             self->arrays[array_index].NAL_unit_type);
    DEBUG_MSG(debug_buffer);
    err = MP4MakeLinkedList(&self->arrays[array_index].nalList);
    if(err) goto bail;

    GET16(arrays[array_index].numNalus);
    for(i = 0; i < self->arrays[array_index].numNalus; i++)
    {
      MP4Handle b;
      u32 the_size;

      GET16_V(the_size);
      err = MP4NewHandle(the_size, &b);
      if(err) goto bail;

      GETBYTES_V_MSG(the_size, *b, "NAL");
      err = MP4AddListEntry((void *)b, self->arrays[array_index].nalList);
      if(err) goto bail;
    }
  }

bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err addParameterSet(struct ISOHEVCConfigAtom *self, MP4Handle ps, u32 nalu)
{
  MP4Err err;
  MP4Handle b;
  u32 the_size;
  u32 i;

  err = MP4NoErr;
  err = MP4GetHandleSize(ps, &the_size);
  if(err) goto bail;
  err = MP4NewHandle(the_size, &b);
  if(err) goto bail;
  memcpy(*b, *ps, the_size);

  for(i = 0; i <= 8; i++)
  {
    if(self->arrays[i].NAL_unit_type == nalu)
    {
      u32 nalCount = 0;
      err          = MP4GetListEntryCount(self->arrays[i].nalList, &nalCount);
      if(err) goto bail;
      if(!nalCount)
      {
        self->numOfArrays++;
      }
      err = MP4AddListEntry((void *)b, self->arrays[i].nalList);
      if(err) goto bail;
      break;
    }
  }

bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err getParameterSet(struct ISOHEVCConfigAtom *self, MP4Handle ps, u32 nalu, u32 index)
{
  MP4Err err;
  MP4Handle b = NULL;
  u32 the_size;
  u32 i;

  err = MP4NoErr;

  for(i = 0; i < self->numOfArrays; i++)
  {
    if(self->arrays[i].NAL_unit_type == nalu)
    {
      err = MP4GetListEntry(self->arrays[i].nalList, index - 1, (char **)&b);
      if(err) goto bail;
      break;
    }
  }

  err = MP4GetHandleSize(b, &the_size);
  if(err) goto bail;
  err = MP4SetHandleSize(ps, the_size);
  if(err) goto bail;
  memcpy(*ps, *b, the_size);

bail:
  TEST_RETURN(err);
  return err;
}

MP4Err MP4CreateHEVCConfigAtom(ISOHEVCConfigAtomPtr *outAtom)
{
  MP4Err err;
  ISOHEVCConfigAtomPtr self;
  u32 i;

  self = (ISOHEVCConfigAtomPtr)calloc(1, sizeof(ISOHEVCConfigAtom));
  TESTMALLOC(self);

  err = MP4CreateBaseAtom((MP4AtomPtr)self);
  if(err) goto bail;
  self->type                  = ISOHEVCConfigAtomType;
  self->name                  = "HEVCConfig";
  self->createFromInputStream = (cisfunc)createFromInputStream;
  self->destroy               = destroy;
  self->calculateSize         = calculateSize;
  self->serialize             = serialize;
  self->complete_rep          = 1;
  self->addParameterSet       = addParameterSet;
  self->getParameterSet       = getParameterSet;

  self->configurationVersion                = 0;
  self->general_profile_space               = 0;
  self->general_tier_flag                   = 0;
  self->general_profile_idc                 = 0;
  self->general_profile_compatibility_flags = 0;
  self->general_constraint_indicator_flags  = 0;
  self->general_level_idc                   = 0;
  self->min_spatial_segmentation_idc        = 0;
  self->parallelismType                     = 0;
  self->chroma_format_idc                   = 0;
  self->bit_depth_luma_minus8               = 0;
  self->bit_depth_chroma_minus8             = 0;
  self->avgFrameRate                        = 0;
  self->constantFrameRate                   = 0;
  self->numTemporalLayers                   = 0;
  self->temporalIdNested                    = 0;
  self->lengthSizeMinusOne                  = 0;
  self->numOfArrays                         = 0;

  for(i = 0; i <= 8; i++)
  {
    err = MP4MakeLinkedList(&self->arrays[i].nalList);
    if(err) goto bail;
    self->arrays[i].NAL_unit_type      = 32 + i;
    self->arrays[i].array_completeness = 1;
  }

  *outAtom = self;
bail:
  TEST_RETURN(err);
  return err;
}
