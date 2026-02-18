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

static void destroy(MP4AtomPtr s)
{
  MP4T35MetadataSampleEntryPtr self = (MP4T35MetadataSampleEntryPtr)s;
  if(self == NULL) return;

  if(self->description)
  {
    free(self->description);
    self->description = NULL;
  }

  if(self->t35_identifier)
  {
    free(self->t35_identifier);
    self->t35_identifier = NULL;
  }

  if(self->super) self->super->destroy(s);
}

static MP4Err serialize(struct MP4Atom *s, char *buffer)
{
  MP4Err err;
  MP4T35MetadataSampleEntryPtr self = (MP4T35MetadataSampleEntryPtr)s;

  err = MP4SerializeCommonBaseAtomFields(s, buffer);
  if(err) goto bail;
  buffer += self->bytesWritten;

  PUTBYTES(self->reserved, 6);
  PUT16(dataReferenceIndex);

  /* Write description as null-terminated UTF-8 string */
  if(self->description != NULL)
  {
    u32 descLen = (u32)strlen(self->description) + 1; /* Include null terminator */
    PUTBYTES(self->description, descLen);
  }
  else
  {
    /* Empty description: just write '\0' */
    u8 nullByte = 0;
    PUT8_V(nullByte);
  }

  /* Write t35_identifier byte array */
  if(self->t35_identifier != NULL && self->t35_identifier_size > 0)
  {
    PUTBYTES(self->t35_identifier, self->t35_identifier_size);
  }

  assert(self->bytesWritten == self->size);
bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err calculateSize(struct MP4Atom *s)
{
  MP4Err err;
  MP4T35MetadataSampleEntryPtr self = (MP4T35MetadataSampleEntryPtr)s;

  err = MP4CalculateBaseAtomFieldSize(s);
  if(err) goto bail;

  self->size += (6 + 2); /* reserved + dataReferenceIndex */

  /* Add description size (null-terminated string) */
  if(self->description != NULL)
  {
    self->size += (u32)strlen(self->description) + 1;
  }
  else
  {
    self->size += 1; /* Just '\0' */
  }

  /* Add t35_identifier size */
  if(self->t35_identifier != NULL)
  {
    self->size += self->t35_identifier_size;
  }

bail:
  TEST_RETURN(err);
  return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream)
{
  MP4Err err;
  MP4T35MetadataSampleEntryPtr self = (MP4T35MetadataSampleEntryPtr)s;
  s64 bytesToRead;
  u32 descLen;
  u32 i;

  if(self == NULL) BAILWITHERROR(MP4BadParamErr)
  err = self->super->createFromInputStream(s, proto, (char *)inputStream);
  if(err) goto bail;

  GETBYTES(6, reserved);
  GET16(dataReferenceIndex);

  /* Calculate remaining bytes to read */
  bytesToRead = (s64)(self->size - self->bytesRead);
  if(bytesToRead < 0) BAILWITHERROR(MP4BadDataErr);

  if(bytesToRead > 0)
  {
    /* Read description (null-terminated UTF-8 string) */
    /* First, scan for null terminator to find description length */
    descLen = 0;
    for(i = 0; i < (u32)bytesToRead; i++)
    {
      u8 byte;
      err = inputStream->read8(inputStream, (u32 *)&byte, NULL);
      if(err) goto bail;
      descLen++;
      self->bytesRead++;
      if(byte == 0) break;
    }

    /* Allocate and read description - we need to re-read the bytes */
    /* Rewind by backing up bytesRead */
    self->bytesRead -= descLen;

    self->description = (char *)calloc(descLen, 1);
    TESTMALLOC(self->description);
    err = inputStream->readData(inputStream, descLen, (char *)self->description, NULL);
    if(err) goto bail;
    self->bytesRead += descLen;

    /* Recalculate bytes remaining */
    bytesToRead = (s64)(self->size - self->bytesRead);

    /* Read t35_identifier (everything remaining) */
    if(bytesToRead > 0)
    {
      self->t35_identifier_size = (u32)bytesToRead;
      self->t35_identifier      = (u8 *)calloc(bytesToRead, 1);
      TESTMALLOC(self->t35_identifier);
      err = inputStream->readData(inputStream, bytesToRead, (char *)self->t35_identifier, NULL);
      if(err) goto bail;
      self->bytesRead += (u32)bytesToRead;
    }
  }

  if(self->bytesRead != self->size) BAILWITHERROR(MP4BadDataErr)

bail:
  TEST_RETURN(err);
  return err;
}

MP4Err MP4CreateT35MetadataSampleEntry(MP4T35MetadataSampleEntryPtr *outAtom)
{
  MP4Err err;
  MP4T35MetadataSampleEntryPtr self;

  self = (MP4T35MetadataSampleEntryPtr)calloc(1, sizeof(MP4T35MetadataSampleEntry));
  TESTMALLOC(self)

  err = MP4CreateBaseAtom((MP4AtomPtr)self);
  if(err) goto bail;

  self->type                  = MP4T35MetadataSampleEntryType;
  self->name                  = "T35MetadataSampleEntry";
  self->createFromInputStream = (cisfunc)createFromInputStream;
  self->destroy               = destroy;
  self->calculateSize         = calculateSize;
  self->serialize             = serialize;

  self->dataReferenceIndex = 1;
  memset(self->reserved, 0, 6);

  self->description         = NULL;
  self->t35_identifier      = NULL;
  self->t35_identifier_size = 0;

  *outAtom = self;
bail:
  TEST_RETURN(err);
  return err;
}
