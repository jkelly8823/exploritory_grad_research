/* crypto/asn1/a_d2i_fp.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <limits.h>
#include "cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/asn1_mac.h>

static int asn1_d2i_read_bio(BIO *in, BUF_MEM **pb);

#ifndef NO_OLD_ASN1
#ifndef OPENSSL_NO_FP_API

void *ASN1_d2i_fp(void *(*xnew)(void), d2i_of_void *d2i, FILE *in, void **x)
        {
        BIO *b;
        void *ret;

        if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		ASN1err(ASN1_F_ASN1_D2I_FP,ERR_R_BUF_LIB);
                return(NULL);
		}
        BIO_set_fp(b,in,BIO_NOCLOSE);
        ret=ASN1_d2i_bio(xnew,d2i,b,x);
        BIO_free(b);
        return(ret);
        }
	{
	BUF_MEM *b;
	unsigned char *p;
	int i;
	ASN1_const_CTX c;
	size_t want=HEADER_SIZE;
	int eos=0;
	size_t off=0;
	size_t len=0;

	b=BUF_MEM_new();
	if (b == NULL)
		{
		ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ERR_R_MALLOC_FAILURE);
		return -1;
		}
			{
			want-=(len-off);

			if (len + want < len || !BUF_MEM_grow_clean(b,len+want))
				{
				ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ERR_R_MALLOC_FAILURE);
				goto err;
				}
				{
				ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_NOT_ENOUGH_DATA);
				goto err;
				}
			if (i > 0)
				{
				if (len+i < len)
					{
					ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
					goto err;
					}
			{
			/* no data body so go round again */
			eos++;
			if (eos < 0)
				{
				ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_HEADER_TOO_LONG);
				goto err;
				}
			{
			/* suck in c.slen bytes of data */
			want=c.slen;
			if (want > (len-off))
				{
				want-=(len-off);
				if (want > INT_MAX /* BIO_read takes an int length */ ||
					len+want < len)
						{
						ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
						goto err;
						}
				if (!BUF_MEM_grow_clean(b,len+want))
					{
					ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ERR_R_MALLOC_FAILURE);
					goto err;
					}
				while (want > 0)
					{
					i=BIO_read(in,&(b->data[len]),want);
					if (i <= 0)
						{
						ASN1err(ASN1_F_ASN1_D2I_READ_BIO,
						    ASN1_R_NOT_ENOUGH_DATA);
						goto err;
						}
					/* This can't overflow because
					 * |len+want| didn't overflow. */
					len+=i;
					want-=i;
					}
				}
			if (off + c.slen < off)
				{
				ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
				goto err;
				}
			off+=c.slen;
			if (eos <= 0)
				{
				break;
				}
			else
				want=HEADER_SIZE;
			}
						{
						ASN1err(ASN1_F_ASN1_D2I_READ_BIO,
						    ASN1_R_NOT_ENOUGH_DATA);
						goto err;
						}
					/* This can't overflow because
					 * |len+want| didn't overflow. */
					len+=i;
					want-=i;
					}
				}
			if (off + c.slen < off)
				{
				ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
				goto err;
				}
			off+=c.slen;
			if (eos <= 0)
				{
				break;
				}
			else
				want=HEADER_SIZE;
			}
		}

	if (off > INT_MAX)
		{
		ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
		goto err;
		}

	*pb = b;
	return off;
err:
	if (b != NULL) BUF_MEM_free(b);
	return -1;
	}
				{
				break;
				}
			else
				want=HEADER_SIZE;
			}
		}

	if (off > INT_MAX)
		{
		ASN1err(ASN1_F_ASN1_D2I_READ_BIO,ASN1_R_TOO_LONG);
		goto err;
		}

	*pb = b;
	return off;
err:
	if (b != NULL) BUF_MEM_free(b);
	return -1;
	}