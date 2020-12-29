/* md5.c - an implementation of the MD5 algorithm and MD5 crypt */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000, 2001  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* See RFC 1321 for a description of the MD5 algorithm.
 */

#include <grub/misc.h>
#include <grub/lib.h>

GRUB_EXPORT(grub_md5_password);

typedef unsigned int UINT4;

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x >> (32 - (n)))))

static UINT4 initstate[4] =
{
  0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 
};

static char s1[4] = {  7, 12, 17, 22 };
static char s2[4] = {  5,  9, 14, 20 };
static char s3[4] = {  4, 11, 16, 23 };
static char s4[4] = {  6, 10, 15, 21 };

static UINT4 T[64] =
{
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const char *b64t =
"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static UINT4 state[4];
static unsigned int length;
static unsigned char buffer[64];

static void
md5_transform (const unsigned char block[64])
{
  int i, j;
  UINT4 a,b,c,d,tmp;
  const UINT4 *x = (UINT4 *) block;

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];

  /* Round 1 */
  for (i = 0; i < 16; i++)
    {
      tmp = a + F (b, c, d) + grub_le_to_cpu32 (x[i]) + T[i];
      tmp = ROTATE_LEFT (tmp, s1[i & 3]);
      tmp += b;
      a = d; d = c; c = b; b = tmp;
    }
  /* Round 2 */
  for (i = 0, j = 1; i < 16; i++, j += 5)
    {
      tmp = a + G (b, c, d) + grub_le_to_cpu32 (x[j & 15]) + T[i+16];
      tmp = ROTATE_LEFT (tmp, s2[i & 3]);
      tmp += b;
      a = d; d = c; c = b; b = tmp;
    }
  /* Round 3 */
  for (i = 0, j = 5; i < 16; i++, j += 3)
    {
      tmp = a + H (b, c, d) + grub_le_to_cpu32 (x[j & 15]) + T[i+32];
      tmp = ROTATE_LEFT (tmp, s3[i & 3]);
      tmp += b;
      a = d; d = c; c = b; b = tmp;
    }
  /* Round 4 */
  for (i = 0, j = 0; i < 16; i++, j += 7)
    {
      tmp = a + I (b, c, d) + grub_le_to_cpu32 (x[j & 15]) + T[i+48];
      tmp = ROTATE_LEFT (tmp, s4[i & 3]);
      tmp += b;
      a = d; d = c; c = b; b = tmp;
    }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

static void
md5_init(void)
{
  grub_memcpy ((char *) state, (char *) initstate, sizeof (initstate));
  length = 0;
}

static void
md5_update (const char *input, int inputlen)
{
  int buflen = length & 63;
  length += inputlen;
  if (buflen + inputlen < 64) 
    {
      grub_memcpy (buffer + buflen, input, inputlen);
      buflen += inputlen;
      return;
    }
  
  grub_memcpy (buffer + buflen, input, 64 - buflen);
  md5_transform (buffer);
  input += 64 - buflen;
  inputlen -= 64 - buflen;
  while (inputlen >= 64)
    {
      md5_transform ((unsigned char *) input);
      input += 64;
      inputlen -= 64;
    }
  grub_memcpy (buffer, input, inputlen);
  buflen = inputlen;
}

static unsigned char *
md5_final (void)
{
  int i, buflen = length & 63;

  buffer[buflen++] = 0x80;
  grub_memset (buffer+buflen, 0, 64 - buflen);
  if (buflen > 56)
    {
      md5_transform (buffer);
      grub_memset (buffer, 0, 64);
      buflen = 0;
    }
  
  *(UINT4 *) (buffer + 56) = grub_cpu_to_le32 (8 * length);
  *(UINT4 *) (buffer + 60) = 0;
  md5_transform (buffer);

  for (i = 0; i < 4; i++)
    state[i] = grub_cpu_to_le32 (state[i]);
  return (unsigned char *) state;
}

/* If CHECK is true, check a password for correctness. Returns 0
   if password was correct, and a value != 0 for error, similarly
   to strcmp.
   If CHECK is false, crypt KEY and save the result in CRYPTED.
   CRYPTED must have a salt.  */
int
grub_md5_password (const char *key, char *crypted, int check)
{
  int keylen = grub_strlen (key);
  char *salt = crypted + 3; /* skip $1$ header */
  char *p; 
  int saltlen;
  int i, n;
  unsigned char alt_result[16];
  unsigned char *digest;

  if (check)
    {
      /* If our crypted password isn't 3 chars, then it can't be md5
	 crypted. So, they don't match.  */
      if (grub_strlen(crypted) <= 3)
	return 1;
      
      saltlen = grub_strstr (salt, "$") - salt;
    }
  else
    {
      char *end = grub_strstr (salt, "$");
      if (end && end - salt < 8)
	saltlen = end - salt;
      else
	saltlen = 8;

      salt[saltlen] = '$';
    }
  
  md5_init ();
  md5_update (key, keylen);
  md5_update (salt, saltlen);
  md5_update (key, keylen);
  digest = md5_final ();
  grub_memcpy (alt_result, digest, 16);
  
  grub_memcpy ((char *) state, (char *) initstate, sizeof (initstate));
  length = 0;
  md5_update (key, keylen);
  md5_update (crypted, 3 + saltlen); /* include the $1$ header */
  for (i = keylen; i > 16; i -= 16)
    md5_update ((char *) alt_result, 16);
  md5_update ((char *) alt_result, i);

  for (i = keylen; i > 0; i >>= 1)
    md5_update (key + ((i & 1) ? keylen : 0), 1);
  digest = md5_final ();

  for (i = 0; i < 1000; i++)
    {
      grub_memcpy (alt_result, digest, 16);

      grub_memcpy ((char *) state, (char *) initstate, sizeof (initstate));
      length = 0;
      if ((i & 1) != 0)
	md5_update (key, keylen);
      else
	md5_update ((char *) alt_result, 16);
      
      if (i % 3 != 0)
	md5_update (salt, saltlen);

      if (i % 7 != 0)
	md5_update (key, keylen);

      if ((i & 1) != 0)
	md5_update ((char *) alt_result, 16);
      else
	md5_update (key, keylen);
      digest = md5_final ();
    }

  p = salt + saltlen + 1;
  for (i = 0; i < 5; i++)
    {
      unsigned int w = 
	digest[i == 4 ? 5 : 12+i] | (digest[6+i] << 8) | (digest[i] << 16);
      for (n = 4; n-- > 0;)
	{
	  if (check)
	    {
	      if (*p++ != b64t[w & 0x3f])
		return 1;
	    }
	  else
	    {
	      *p++ = b64t[w & 0x3f];
	    }
	  
	  w >>= 6;
	}
    }
  {
    unsigned int w = digest[11];
    for (n = 2; n-- > 0;)
      {
	if (check)
	  {
	    if (*p++ != b64t[w & 0x3f])
	      return 1;
	  }
	else
	  {
	    *p++ = b64t[w & 0x3f];
	  }
	
	w >>= 6;
      }
  }

  if (! check)
    *p = '\0';
  
  return *p;
}
