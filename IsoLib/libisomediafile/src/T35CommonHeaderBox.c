/* This software module was originally developed by Apple Computer, Inc. in the course of
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
 * or derivative works. Copyright (c) 2026.
 */

#include "MP4Atoms.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void destroy(MP4AtomPtr s)
{
  MP4T35CommonHeaderBoxPtr self = (MP4T35CommonHeaderBoxPtr)s;
  if(self == NULL) return;

  if(self->t35_prefix_text)
  {
    free(self->t35_prefix_text);
    self->t35_prefix_text = NULL;
  }
  if(self->super) self->super->destroy(s);
}

static MP4Err serialize(struct MP4Atom *s, char *buffer)
{
  MP4Err err;
  MP4T35CommonHeaderBoxPtr self = (MP4T35CommonHeaderBoxPtr)s;
  err                           = MP4NoErr;

  err = MP4SerializeCommonFullAtomFields((MP4FullAtomPtr)s, buffer);
  if(err) goto bail;
  buffer += self->bytesWritten;

  /* Write UTF-8 text without NUL termination */
  if(self->t35_prefix_text != NULL)
  {
    u32 textLen = (u32)strlen(self->t35_prefix_text);
    PUTBYTES(self->t35_prefix_text, textLen);
  }

  assert(self->bytesWritten == self->size);
bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err calculateSize(struct MP4Atom *s)
{
  MP4Err err;
  MP4T35CommonHeaderBoxPtr self = (MP4T35CommonHeaderBoxPtr)s;
  err                           = MP4NoErr;

  err = MP4CalculateFullAtomFieldSize((MP4FullAtomPtr)s);
  if(err) goto bail;

  /* Size of text without NUL terminator */
  if(self->t35_prefix_text != NULL)
  {
    self->size += (u32)strlen(self->t35_prefix_text);
  }

bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream)
{
  MP4Err err;
  u32 dataSize                  = 0;
  MP4T35CommonHeaderBoxPtr self = (MP4T35CommonHeaderBoxPtr)s;
  err                           = MP4NoErr;

  if(self == NULL) BAILWITHERROR(MP4BadParamErr)
  err = self->super->createFromInputStream(s, proto, (char *)inputStream);
  if(err) goto bail;

  dataSize = self->size - self->bytesRead;
  if(dataSize > 0)
  {
    /* Allocate with space for NUL terminator (internal use) */
    self->t35_prefix_text = (char *)calloc(dataSize + 1, sizeof(char));
    TESTMALLOC(self->t35_prefix_text);
    GETBYTES(dataSize, t35_prefix_text);
    self->t35_prefix_text[dataSize] = '\0'; /* NUL terminate for C string handling */
  }

bail:
  TEST_RETURN(err);
  return err;
}

/**
 * Validate T.35 prefix text format according to specification:
 * - T35Prefix: even number of uppercase hex digits (0-9, A-F)
 * - Optional ":T35Description" without colons or C0 controls in description
 * Regex: ^[0-9A-F]{2}(?:[0-9A-F]{2})*(?::[^\x00-\x1F:]+)?$
 */
static MP4Err validateT35PrefixText(const char *text)
{
  const char *p = text;
  int hexCount  = 0;

  if(text == NULL || *text == '\0') return MP4BadParamErr;

  /* Parse hex digits */
  while(*p && *p != ':')
  {
    if(!isxdigit((unsigned char)*p) || !isupper((unsigned char)*p))
    {
      if(islower((unsigned char)*p))
      {
        /* Lowercase hex digit - should be uppercase */
        return MP4BadParamErr;
      }
      else if(!isdigit((unsigned char)*p))
      {
        /* Not a hex digit */
        return MP4BadParamErr;
      }
    }
    hexCount++;
    p++;
  }

  /* Must have even number of hex digits */
  if(hexCount == 0 || (hexCount % 2) != 0) return MP4BadParamErr;

  /* If colon present, validate description */
  if(*p == ':')
  {
    p++;                                  /* skip colon */
    if(*p == '\0') return MP4BadParamErr; /* colon but no description */

    while(*p)
    {
      unsigned char c = (unsigned char)*p;
      /* Check for C0 controls (0x00-0x1F) or additional colons */
      if(c < 0x20 || c == ':') return MP4BadParamErr;
      p++;
    }
  }

  return MP4NoErr;
}

MP4Err MP4CreateT35CommonHeaderBox(MP4T35CommonHeaderBoxPtr *outAtom)
{
  MP4Err err;
  MP4T35CommonHeaderBoxPtr self;

  self = (MP4T35CommonHeaderBoxPtr)calloc(1, sizeof(MP4T35CommonHeaderBox));
  TESTMALLOC(self);

  err = MP4CreateFullAtom((MP4AtomPtr)self);
  if(err) goto bail;

  self->type                  = MP4T35CommonHeaderBoxType;
  self->name                  = "T35CommonHeaderBox";
  self->createFromInputStream = (cisfunc)createFromInputStream;
  self->destroy               = destroy;
  self->calculateSize         = calculateSize;
  self->serialize             = serialize;
  self->version               = 0;
  self->flags                 = 0;
  self->t35_prefix_text       = NULL;

  *outAtom = self;
bail:
  TEST_RETURN(err);
  return err;
}

MP4Err MP4SetT35CommonHeaderBoxText(MP4T35CommonHeaderBoxPtr self, const char *text)
{
  MP4Err err = MP4NoErr;

  if(self == NULL || text == NULL) BAILWITHERROR(MP4BadParamErr);

  /* Validate format */
  err = validateT35PrefixText(text);
  if(err) goto bail;

  /* Free existing text */
  if(self->t35_prefix_text)
  {
    free(self->t35_prefix_text);
    self->t35_prefix_text = NULL;
  }

  /* Allocate and copy new text */
  self->t35_prefix_text = (char *)calloc(strlen(text) + 1, sizeof(char));
  TESTMALLOC(self->t35_prefix_text);
  strcpy(self->t35_prefix_text, text);

bail:
  TEST_RETURN(err);
  return err;
}
